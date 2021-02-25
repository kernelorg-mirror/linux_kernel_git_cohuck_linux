/* SPDX-License-Identifier: GPL-2.0 */
/*
 * common definitions for the ccw based virtio transport
 *
 * Copyright IBM Corp. 2012, 2014
 * Copyright Red Hat, Inc. 2021
 *
 *    Author(s): Cornelia Huck <cohuck@redhat.com>
 */

#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ring.h>
#include <asm/cio.h>
#include <asm/virtio-ccw.h>

#ifdef CONFIG_VIRTIO_CCW_LEGACY
#define VIRTIO_CCW_REV_MIN 0
void virtio_ccw_del_vq_legacy(struct virtqueue *vq, struct ccw1 *ccw);
struct virtqueue *virtio_ccw_setup_vq_legacy(struct virtio_device *vdev,
					     int i, vq_callback_t *callback,
					     const char *name, bool ctx,
					     struct ccw1 *ccw);
#else
#define VIRTIO_CCW_REV_MIN 1
static inline void virtio_ccw_del_vq_legacy(struct virtqueue *vq,
					    struct ccw1 *ccw)
{
}
static inline struct virtqueue *
virtio_ccw_setup_vq_legacy(struct virtio_device *vdev,
			   int i, vq_callback_t *callback,
			   const char *name, bool ctx,
			   struct ccw1 *ccw)
{
	return ERR_PTR(-EINVAL);
}
#endif

extern int min_revision;

struct virtio_ccw_vq_info {
	struct virtqueue *vq;
	int num;
	void *info_block;
	int bit_nr;
	struct list_head node;
	long cookie;
};

#define CCW_CMD_SET_VQ 0x13
#define CCW_CMD_VDEV_RESET 0x33
#define CCW_CMD_SET_IND 0x43
#define CCW_CMD_SET_CONF_IND 0x53
#define CCW_CMD_READ_FEAT 0x12
#define CCW_CMD_WRITE_FEAT 0x11
#define CCW_CMD_READ_CONF 0x22
#define CCW_CMD_WRITE_CONF 0x21
#define CCW_CMD_WRITE_STATUS 0x31
#define CCW_CMD_READ_VQ_CONF 0x32
#define CCW_CMD_READ_STATUS 0x72
#define CCW_CMD_SET_IND_ADAPTER 0x73
#define CCW_CMD_SET_VIRTIO_REV 0x83

#define VIRTIO_CCW_DOING_SET_VQ 0x00010000
#define VIRTIO_CCW_DOING_RESET 0x00040000
#define VIRTIO_CCW_DOING_READ_FEAT 0x00080000
#define VIRTIO_CCW_DOING_WRITE_FEAT 0x00100000
#define VIRTIO_CCW_DOING_READ_CONFIG 0x00200000
#define VIRTIO_CCW_DOING_WRITE_CONFIG 0x00400000
#define VIRTIO_CCW_DOING_WRITE_STATUS 0x00800000
#define VIRTIO_CCW_DOING_SET_IND 0x01000000
#define VIRTIO_CCW_DOING_READ_VQ_CONF 0x02000000
#define VIRTIO_CCW_DOING_SET_CONF_IND 0x04000000
#define VIRTIO_CCW_DOING_SET_IND_ADAPTER 0x08000000
#define VIRTIO_CCW_DOING_SET_VIRTIO_REV 0x10000000
#define VIRTIO_CCW_DOING_READ_STATUS 0x20000000
#define VIRTIO_CCW_INTPARM_MASK 0xffff0000

#define VIRTIO_CCW_CONFIG_SIZE 0x100
/* same as PCI config space size, should be enough for all drivers */

struct virtio_ccw_device {
	struct virtio_device vdev;
	__u8 config[VIRTIO_CCW_CONFIG_SIZE];
	struct ccw_device *cdev;
	__u32 curr_io;
	int err;
	unsigned int revision; /* Transport revision */
	wait_queue_head_t wait_q;
	spinlock_t lock;
	struct mutex io_lock; /* Serializes I/O requests */
	struct list_head virtqueues;
	bool is_thinint;
	bool going_away;
	bool device_lost;
	unsigned int config_ready;
	void *airq_info;
	struct vcdev_dma_area *dma_area;
};

static struct virtio_ccw_device *to_vc_device(struct virtio_device *vdev)
{
	return container_of(vdev, struct virtio_ccw_device, vdev);
}

int ccw_io_helper(struct virtio_ccw_device *vcdev,
		  struct ccw1 *ccw, __u32 intparm);

int virtio_ccw_read_vq_conf(struct virtio_ccw_device *vcdev,
			    struct ccw1 *ccw, int index);

bool virtio_ccw_kvm_notify(struct virtqueue *vq);
