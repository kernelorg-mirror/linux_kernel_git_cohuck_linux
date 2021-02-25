// SPDX-License-Identifier: GPL-2.0
/*
 * ccw based virtio transport -- legacy support
 *
 * Copyright IBM Corp. 2012, 2014
 * Copyright Red Hat, Inc. 2021
 *
 *    Author(s): Cornelia Huck <cohuck@redhat.com>
 */

#include <linux/slab.h>
#include <asm/ccwdev.h>
#include "virtio_ccw_common.h"

struct vq_info_block_legacy {
	__u64 queue;
	__u32 align;
	__u16 index;
	__u16 num;
} __packed;


void virtio_ccw_del_vq_legacy(struct virtqueue *vq, struct ccw1 *ccw)
{
	struct virtio_ccw_device *vcdev = to_vc_device(vq->vdev);
	struct virtio_ccw_vq_info *info = vq->priv;
	struct vq_info_block_legacy *info_block;
	unsigned long flags;
	int ret;
	unsigned int index = vq->index;

	/* Remove from our list. */
	spin_lock_irqsave(&vcdev->lock, flags);
	list_del(&info->node);
	spin_unlock_irqrestore(&vcdev->lock, flags);

	/* Release from host. */
	info_block = info->info_block;
	info_block->queue = 0;
	info_block->align = 0;
	info_block->index = index;
	info_block->num = 0;
	ccw->count = sizeof(*info_block);
	ccw->cmd_code = CCW_CMD_SET_VQ;
	ccw->flags = 0;
	ccw->cda = (__u32)(unsigned long)(info->info_block);
	ret = ccw_io_helper(vcdev, ccw,
			    VIRTIO_CCW_DOING_SET_VQ | index);
	/*
	 * -ENODEV isn't considered an error: The device is gone anyway.
	 * This may happen on device detach.
	 */
	if (ret && (ret != -ENODEV))
		dev_warn(&vq->vdev->dev, "Error %d while deleting queue %d\n",
			 ret, index);

	vring_del_virtqueue(vq);
	ccw_device_dma_free(vcdev->cdev, info->info_block,
			    sizeof(*info_block));
	kfree(info);
}

struct virtqueue *virtio_ccw_setup_vq_legacy(struct virtio_device *vdev,
					     int i, vq_callback_t *callback,
					     const char *name, bool ctx,
					     struct ccw1 *ccw)
{
	struct virtio_ccw_device *vcdev = to_vc_device(vdev);
	int err;
	struct virtqueue *vq = NULL;
	struct virtio_ccw_vq_info *info;
	struct vq_info_block_legacy *info_block;
	u64 queue;
	unsigned long flags;

	/* Allocate queue. */
	info = kzalloc(sizeof(struct virtio_ccw_vq_info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto out_err;
	}
	info->info_block = ccw_device_dma_zalloc(vcdev->cdev,
						 sizeof(struct vq_info_block_legacy));
	if (!info->info_block) {
		dev_warn(&vcdev->cdev->dev, "no info block\n");
		err = -ENOMEM;
		goto out_err;
	}
	info->num = virtio_ccw_read_vq_conf(vcdev, ccw, i);
	if (info->num < 0) {
		err = info->num;
		goto out_err;
	}
	vq = vring_create_virtqueue(i, info->num, KVM_VIRTIO_CCW_RING_ALIGN,
				    vdev, true, false, ctx,
				    virtio_ccw_kvm_notify, callback, name);

	if (!vq) {
		dev_warn(&vcdev->cdev->dev, "no vq\n");
		err = -ENOMEM;
		goto out_err;
	}

	/* Register it with the host. */
	queue = virtqueue_get_desc_addr(vq);
	info_block->queue = queue;
	info_block->align = KVM_VIRTIO_CCW_RING_ALIGN;
	info_block->index = i;
	info_block->num = info->num;
	ccw->count = sizeof(*info_block);
	ccw->cmd_code = CCW_CMD_SET_VQ;
	ccw->flags = 0;
	ccw->cda = (__u32)(unsigned long)(info->info_block);
	err = ccw_io_helper(vcdev, ccw, VIRTIO_CCW_DOING_SET_VQ | i);
	if (err) {
		dev_warn(&vcdev->cdev->dev, "SET_VQ failed\n");
		goto out_err;
	}

	info->vq = vq;
	vq->priv = info;

	/* Save it to our list. */
	spin_lock_irqsave(&vcdev->lock, flags);
	list_add(&info->node, &vcdev->virtqueues);
	spin_unlock_irqrestore(&vcdev->lock, flags);

	return vq;

out_err:
	if (vq)
		vring_del_virtqueue(vq);
	if (info) {
		ccw_device_dma_free(vcdev->cdev, info->info_block,
				    sizeof(*info_block));
	}
	kfree(info);
	return ERR_PTR(err);
}
