#include <stdint.h>
#include "kvm_util.h"
#include "processor.h"
#include "test_util.h"
#include <linux/bitfield.h>

static uint64_t masks[KVM_ARM_FEATURE_ID_RANGE_SIZE];

static void guest_code(void)
{
	GUEST_DONE();
}

static void guest_run(struct kvm_vcpu *vcpu)
{
	bool done = false;
	struct ucall uc;

	while (!done) {
		vcpu_run(vcpu);

		switch (get_ucall(vcpu, &uc)) {
		case UCALL_ABORT:
			REPORT_GUEST_ASSERT(uc);
			break;
		case UCALL_SYNC:
			break;
		case UCALL_PRINTF:
			REPORT_GUEST_PRINTF(uc);
			break;
		case UCALL_DONE:
			done = true;
			break;
		default:
			TEST_FAIL("Unexpected ucall: %lu", uc.cmd);
		}
	}
}

static void get_writable_masks(struct kvm_vm *vm)
{
	struct reg_mask_range range = {
		.addr = (__u64)masks,
	};

	memset(range.reserved, 0, sizeof(range.reserved));
	vm_ioctl(vm, KVM_ARM_GET_REG_WRITABLE_MASKS, &range);
}

#define kvm_sys_reg_Op0(id) (((id) & KVM_REG_ARM64_SYSREG_OP0_MASK) >> KVM_REG_ARM64_SYSREG_OP0_SHIFT)
#define kvm_sys_reg_Op1(id) (((id) & KVM_REG_ARM64_SYSREG_OP1_MASK) >> KVM_REG_ARM64_SYSREG_OP1_SHIFT)
#define kvm_sys_reg_CRn(id) (((id) & KVM_REG_ARM64_SYSREG_CRN_MASK) >> KVM_REG_ARM64_SYSREG_CRN_SHIFT)
#define kvm_sys_reg_CRm(id) (((id) & KVM_REG_ARM64_SYSREG_CRM_MASK) >> KVM_REG_ARM64_SYSREG_CRM_SHIFT)
#define kvm_sys_reg_Op2(id) (((id) & KVM_REG_ARM64_SYSREG_OP2_MASK) >> KVM_REG_ARM64_SYSREG_OP2_SHIFT)

static bool reg_in_feature_id_range(uint64_t reg)
{
	return kvm_sys_reg_Op0(reg) == 3 &&
		(kvm_sys_reg_Op1(reg) == 0 || kvm_sys_reg_Op1(reg) == 1 ||
		 kvm_sys_reg_Op1(reg) == 3) &&
		kvm_sys_reg_CRn(reg) == 0 &&
		kvm_sys_reg_CRm(reg) >= 0 && kvm_sys_reg_CRm(reg) <= 7 &&
		kvm_sys_reg_Op2(reg) >= 0 && kvm_sys_reg_Op2(reg) <= 7;
}

static uint64_t reg_get_mask(uint64_t reg)
{
	int idx = KVM_ARM_FEATURE_ID_RANGE_IDX(kvm_sys_reg_Op0(reg), kvm_sys_reg_Op1(reg),
					       kvm_sys_reg_CRn(reg), kvm_sys_reg_CRm(reg),
					       kvm_sys_reg_Op2(reg));

	return reg_in_feature_id_range(reg) ? masks[idx] : 0;
}

static void dump_regs(struct kvm_vcpu *vcpu)
{
	struct kvm_reg_list *reg_list = vcpu_get_reg_list(vcpu);
	uint64_t reg, val[8];
	int i, ret;

	for (i = 0; i < reg_list->n; i++) {
		reg = reg_list->reg[i];
		ret = __vcpu_get_reg(vcpu, reg, val);
		if (!ret)
			pr_info("%lx : %16lx %lx\n", reg, val[0], reg_get_mask(reg));
	}

	free(reg_list);
}

static struct kvm_vm *create_vm(struct kvm_vcpu **vcpu, void *guest_code)
{
	struct kvm_vcpu_init init;
	struct kvm_vm *vm;

	vm = vm_create(1);
	vm_ioctl(vm, KVM_ARM_PREFERRED_TARGET, &init);

	if (kvm_has_cap(KVM_CAP_ARM_PTRAUTH_ADDRESS)) {
		init.features[0] |= (1 << KVM_ARM_VCPU_PTRAUTH_ADDRESS);
		init.features[0] |= (1 << KVM_ARM_VCPU_PTRAUTH_GENERIC);
	}

	*vcpu = aarch64_vcpu_add(vm, 0, &init, guest_code);

	return vm;
}

int main(void)
{
	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;

	vm = create_vm(&vcpu, guest_code);
	get_writable_masks(vm);
	guest_run(vcpu);
	dump_regs(vcpu);
	kvm_vm_free(vm);
}
