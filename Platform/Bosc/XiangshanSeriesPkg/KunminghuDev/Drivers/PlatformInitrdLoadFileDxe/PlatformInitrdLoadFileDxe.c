/** @file

  Expose a fixed memory region as LINUX_EFI_INITRD_MEDIA_GUID via LoadFile2.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Guid/LinuxEfiInitrdMedia.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadFile2.h>

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH          VenMediaNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} KMH_INITRD_DEVICE_PATH;
#pragma pack ()

STATIC EFI_HANDLE mInitrdHandle;
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
EFIAPI
KmhInitrdLoadFile2 (
  IN      EFI_LOAD_FILE2_PROTOCOL   *This,
  IN      EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  IN      BOOLEAN                   BootPolicy,
  IN  OUT UINTN                     *BufferSize,
  OUT     VOID                      *Buffer OPTIONAL
  )
{
  UINT64 Base;
  UINT64 Size;

  Base = FixedPcdGet64 (PcdInitrdBase);
  Size = FixedPcdGet64 (PcdInitrdSize);

  if (BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  if ((BufferSize == NULL) || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((FilePath->Type != END_DEVICE_PATH_TYPE) ||
      (FilePath->SubType != END_ENTIRE_DEVICE_PATH_SUBTYPE) ||
      (Size == 0))
  {
    return EFI_NOT_FOUND;
  }

  if ((Buffer == NULL) || (*BufferSize < Size)) {
    *BufferSize = (UINTN)Size;
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Buffer, (VOID *)(UINTN)Base, (UINTN)Size);
  *BufferSize = (UINTN)Size;
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
      (UINT64)FixedPcdGet64 (PcdInitrdBase),
      (UINT64)FixedPcdGet64 (PcdInitrdSize)
      ));
  }

  return Status;
}
