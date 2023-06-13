#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/HiiLib.h>
#include <Library/DebugLib.h>

#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_HANDLE gShellLoaderToolHiiHandle = NULL;
CONST CHAR16 gShellLoaderToolFileName[] = L"LoaderToolShellCommand";

CONST CHAR16*
EFIAPI
ShellCommandGetManFileNameLoaderTool (
  VOID
  )
{
  return gShellLoaderToolFileName;
}

SHELL_STATUS
EFIAPI
ShellBootFromAddress (unsigned long entry, unsigned long dtb, unsigned long hartid)
{
  ((void(*)(unsigned long, unsigned long))entry)(hartid, dtb);
  
  return EFI_SUCCESS;
}

SHELL_STATUS
EFIAPI
ShellCommandRunBoscLoaderTool (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "Bosc Loader...\n"));
  
  unsigned long long kernel_entry = FixedPcdGet32(PcdKernelBase);
  unsigned long long dtb_start = FixedPcdGet32(PcdDTBBase);
  int hart_id = FixedPcdGet32 (PcdBootHartId);
 
  return ShellBootFromAddress (kernel_entry, dtb_start, hart_id);
}

EFI_STATUS
EFIAPI
ShellBoscLoaderToolLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gShellLoaderToolHiiHandle = HiiAddPackages (
                        &gShellLoaderToolHiiGuid, gImageHandle, 
                        UefiShellLoaderToolLibStrings, NULL
                        );
  if (gShellLoaderToolHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }
  
  ShellCommandRegisterCommandName (
     L"bl", ShellCommandRunBoscLoaderTool, ShellCommandGetManFileNameLoaderTool, 0,
     L"bl", TRUE , gShellLoaderToolHiiHandle, STRING_TOKEN (STR_GET_HELP_LOADER_TOOL)
     );
 
  return EFI_SUCCESS; 
}

EFI_STATUS
EFIAPI
ShellBoscLoaderToolLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gShellLoaderToolHiiHandle != NULL) {
    HiiRemovePackages (gShellLoaderToolHiiHandle);
  }

  return EFI_SUCCESS;
}
