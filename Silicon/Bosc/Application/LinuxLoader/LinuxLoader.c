#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

STATIC
EFI_STATUS
EFIAPI
EfiBootFromAddress (unsigned long entry, unsigned long dtb, unsigned long hartid)
{
  ((void(*)(unsigned long, unsigned long))entry)(hartid, dtb);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain
(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE  *SystemTable)
{
  unsigned long long kernel_entry = FixedPcdGet32(PcdKernelBase);
  unsigned long long dtb_start = FixedPcdGet32(PcdDTBBase);
  int hart_id = FixedPcdGet32 (PcdBootHartId);

  SystemTable->ConOut->OutputString(
            SystemTable->ConOut, L"Bosc Loader...\r\n"
	    );

  return EfiBootFromAddress (kernel_entry, dtb_start, hart_id);
}
