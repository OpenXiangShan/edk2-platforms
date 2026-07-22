/** @file

  KMH platform RamDisk registration driver.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/FdtClient.h>
#include <Protocol/RamDisk.h>

STATIC
EFI_STATUS
KmhFdtReadChosenU64 (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  INT32                ChosenNode,
  IN  CONST CHAR8          *PropertyName,
  OUT UINT64               *Value
  )
{
  EFI_STATUS    Status;
  CONST UINT32  *Property;
  UINT32        PropertySize;

  if ((FdtClient == NULL) || (PropertyName == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, ChosenNode, PropertyName, (CONST VOID **)&Property, &PropertySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (PropertySize == sizeof (UINT64)) {
    *Value = LShiftU64 (SwapBytes32 (Property[0]), 32) | SwapBytes32 (Property[1]);
    return EFI_SUCCESS;
  }

  if (PropertySize == sizeof (UINT32)) {
    *Value = SwapBytes32 (Property[0]);
    return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}

STATIC
BOOLEAN
KmhFpgaMultistageBurnEnabled (
  VOID
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  INT32                ChosenNode;
  CONST UINT32        *Property;
  UINT32               PropertySize;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = FdtClient->GetOrInsertChosenNode (FdtClient, &ChosenNode);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = FdtClient->GetNodeProperty (
                      FdtClient,
                      ChosenNode,
                      "kmh,fpga-multistage-burn",
                      (CONST VOID **)&Property,
                      &PropertySize
                      );
  if (EFI_ERROR (Status) || (PropertySize < sizeof (UINT32))) {
    return FALSE;
  }

  return SwapBytes32 (Property[0]) != 0;
}

STATIC
EFI_STATUS
KmhGetChosenPayloadRegion (
  IN  CONST CHAR8  *Name,
  IN  CONST CHAR8  *StartPropertyName,
  IN  CONST CHAR8  *EndPropertyName,
  IN  CONST CHAR8  *SizePropertyName,
  OUT UINT64  *Base,
  OUT UINT64  *Size
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  INT32                ChosenNode;
  UINT64               End;

  if ((Name == NULL) || (StartPropertyName == NULL) || (EndPropertyName == NULL) || (SizePropertyName == NULL) ||
      (Base == NULL) || (Size == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *Base = 0;
  *Size = 0;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FDT client not found - %r\n", __func__, Status));
    return Status;
  }

  Status = FdtClient->GetOrInsertChosenNode (FdtClient, &ChosenNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: /chosen node not available - %r\n", __func__, Status));
    return Status;
  }

  Status = KmhFdtReadChosenU64 (FdtClient, ChosenNode, StartPropertyName, Base);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: /chosen/%a not found for %a - %r\n", __func__, StartPropertyName, Name, Status));
    return Status;
  }

  Status = KmhFdtReadChosenU64 (FdtClient, ChosenNode, EndPropertyName, &End);
  if (!EFI_ERROR (Status)) {
    if (End <= *Base) {
      return EFI_INVALID_PARAMETER;
    }

    *Size = End - *Base;
    return EFI_SUCCESS;
  }

  Status = KmhFdtReadChosenU64 (FdtClient, ChosenNode, SizePropertyName, Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: /chosen/%a or /chosen/%a not found for %a - %r\n", __func__, EndPropertyName, SizePropertyName, Name, Status));
    return Status;
  }

  if (*Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

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
  UINT64                    RamDiskBase;
  UINT64                    RamDiskSize;

  if (!FeaturePcdGet (PcdRamDiskSupported)) {
    DEBUG ((DEBUG_INFO, "%a: RamDisk disabled by PCD\n", __func__));
    return EFI_UNSUPPORTED;
  }

  if (!KmhFpgaMultistageBurnEnabled ()) {
    DEBUG ((DEBUG_INFO, "%a: FPGA multi-stage burn disabled\n", __func__));
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

  Status = KmhGetChosenPayloadRegion (
             "image3 ramdisk",
             "kmh,image3-start",
             "kmh,image3-end",
             "kmh,image3-size",
             &RamDiskBase,
             &RamDiskSize
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RamDisk->Register (
                      RamDiskBase,
                      RamDiskSize,
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
            RamDiskBase,
            RamDiskSize));
  }

  return Status;
}
