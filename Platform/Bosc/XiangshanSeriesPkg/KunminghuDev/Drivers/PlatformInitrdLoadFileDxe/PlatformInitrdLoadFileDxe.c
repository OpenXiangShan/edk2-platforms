/** @file

  Expose a fixed memory region as LINUX_EFI_INITRD_MEDIA_GUID via LoadFile2.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Guid/LinuxEfiInitrdMedia.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/FdtClient.h>
#include <Protocol/LoadFile2.h>

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH          VenMediaNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} KMH_INITRD_DEVICE_PATH;
#pragma pack ()

STATIC EFI_HANDLE mInitrdHandle;
STATIC UINT64     mInitrdBase;
STATIC UINT64     mInitrdSize;
STATIC KMH_INITRD_DEVICE_PATH mInitrdDevicePath = {
  {
    {
      MEDIA_DEVICE_PATH, MEDIA_VENDOR_DP,
      { sizeof (VENDOR_DEVICE_PATH), 0 }
    },
    LINUX_EFI_INITRD_MEDIA_GUID
  },
  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
  }
};

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
  OUT UINT64       *Base,
  OUT UINT64       *Size
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

STATIC
EFI_STATUS
EFIAPI
KmhInitrdLoadFile2 (
  IN      EFI_LOAD_FILE2_PROTOCOL   *This,
  IN      EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  IN      BOOLEAN                   BootPolicy,
  IN  OUT UINTN                     *BufferSize,
  OUT     VOID                      *Buffer OPTIONAL
  )
{
  if (BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  if ((BufferSize == NULL) || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((FilePath->Type != END_DEVICE_PATH_TYPE) ||
      (FilePath->SubType != END_ENTIRE_DEVICE_PATH_SUBTYPE) ||
      (mInitrdSize == 0))
  {
    return EFI_NOT_FOUND;
  }

  if ((Buffer == NULL) || (*BufferSize < mInitrdSize)) {
    *BufferSize = (UINTN)mInitrdSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Buffer, (VOID *)(UINTN)mInitrdBase, (UINTN)mInitrdSize);
  *BufferSize = (UINTN)mInitrdSize;
  return EFI_SUCCESS;
}

STATIC EFI_LOAD_FILE2_PROTOCOL mInitrdLoadFile2 = {
  KmhInitrdLoadFile2
};

EFI_STATUS
EFIAPI
KmhPlatformInitrdEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  if (!FeaturePcdGet (PcdInitrdSupported)) {
    DEBUG ((DEBUG_INFO, "%a: initrd disabled by PCD\n", __func__));
    return EFI_UNSUPPORTED;
  }

  if (!KmhFpgaMultistageBurnEnabled ()) {
    DEBUG ((DEBUG_INFO, "%a: FPGA multi-stage burn disabled\n", __func__));
    return EFI_UNSUPPORTED;
  }

  Status = KmhGetChosenPayloadRegion (
             "image4 initrd",
             "linux,initrd-start",
             "linux,initrd-end",
             "linux,initrd-size",
             &mInitrdBase,
             &mInitrdSize
             );
  if (EFI_ERROR (Status)) {
    Status = KmhGetChosenPayloadRegion (
               "image4 initrd",
               "kmh,image4-start",
               "kmh,image4-end",
               "kmh,image4-size",
               &mInitrdBase,
               &mInitrdSize
               );
  }
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mInitrdHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mInitrdDevicePath,
                  &gEfiLoadFile2ProtocolGuid,
                  &mInitrdLoadFile2,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to install LoadFile2 - %r\n", __func__, Status));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a: initrd LoadFile2 installed, base=0x%lx size=0x%lx\n",
      __func__,
      mInitrdBase,
      mInitrdSize
      ));
  }

  return Status;
}
