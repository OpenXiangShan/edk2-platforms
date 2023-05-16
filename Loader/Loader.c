#include <Uefi.h>
#include <Base.h>
#include <Library/PeCoffGetEntryPointLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/ExtractGuidedSectionLib.h>

#define __io_br()	do {} while (0)
#define __io_ar()	__asm__ __volatile__ ("fence i,r" : : : "memory");
#define __io_bw()	__asm__ __volatile__ ("fence w,o" : : : "memory");
#define __io_aw()	do {} while (0)

static inline void __raw_writel(unsigned int val, volatile void *addr)
{
	asm volatile("sw %0, 0(%1)" : : "r"(val), "r"(addr));
}

#define writel(v,c)	({ __io_bw(); __raw_writel((v),(c)); __io_aw(); })

EFI_STATUS
EFIAPI
DEMO_LOADER(
	IN EFI_HANDLE           ImageHandle,
	IN EFI_SYSTEM_TABLE*  	SystemTable
)
{
	unsigned long long kernel_entry = FixedPcdGet32(PcdKernelBase);
	unsigned long long dtb_start = FixedPcdGet32(PcdDTBBase);
	int hart_id = FixedPcdGet32 (PcdBootHartId);
	DEBUG ((DEBUG_INFO, "\r\n\033[33mKernelBase: 0x%x DtbBase: 0x%x boot_hartid: %d\033[0m\n", kernel_entry, dtb_start, hart_id));

	if (kernel_entry == 0) {
		DEBUG ((DEBUG_INFO, "warning!!! kernel_entry is NULL... can not enter linux\n"));
		return EFI_OUT_OF_RESOURCES;
	}
	else if (dtb_start == 0) {
		DEBUG ((DEBUG_INFO, "warning!!! dtb_start is NULL... can not entry linux\n"));
		return EFI_OUT_OF_RESOURCES;
	}
	else
		((void(*)(unsigned long, unsigned long))kernel_entry)(hart_id, dtb_start);

	DEBUG ((DEBUG_INFO, "warning!!! linux kernel exit...\n"));

	return EFI_SUCCESS;
}
