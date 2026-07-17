/** @file

  KMH platform RamDisk registration driver.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/RamDisk.h>

EFI_STATUS
EFIAPI
KmhPlatformRamDiskEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_RAM_DISK_PROTOCOL     *RamDisk;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  if (!FeaturePcdGet (PcdRamDiskSupported)) {
    DEBUG ((DEBUG_INFO, "%a: RamDisk disabled by PCD\n", __func__));
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateProtocol (
                  &gEfiRamDiskProtocolGuid,
                  NULL,
                  (VOID **)&RamDisk
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RamDisk protocol not found - %r\n",
            __func__, Status));
    return Status;
  }

  Status = RamDisk->Register (
                      FixedPcdGet64 (PcdRamDiskBase),
                      FixedPcdGet64 (PcdRamDiskSize),
                      &gEfiVirtualDiskGuid,
                      NULL,
                      &DevicePath
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to register RamDisk - %r\n",
            __func__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a: RamDisk registered, base=0x%lx size=0x%lx\n",
            __func__,
            (UINT64)FixedPcdGet64 (PcdRamDiskBase),
            (UINT64)FixedPcdGet64 (PcdRamDiskSize)));
  }

  return Status;
}
