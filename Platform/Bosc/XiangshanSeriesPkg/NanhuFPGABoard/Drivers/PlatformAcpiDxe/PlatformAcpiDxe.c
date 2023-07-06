/** @file
 *
 *  ACPI support for the Bosc platform
 *
 *  Copyright (c) 2021, Jared McNeill <jmcneill@invisible.ca>
 *  Copyright (c) 2017,2021 Andrey Warkentin <andrey.warkentin@gmail.com>
 *  Copyright (c) 2016, Linaro, Ltd. All rights reserved.
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/AcpiLib.h>

STATIC CONST EFI_GUID mAcpiTableFile = {
  0x50EB9109, 0x15AC, 0xAFCC, { 0x2A, 0x2D, 0x15, 0x40, 0xC2, 0x60, 0x41, 0x88 }
};

EFI_STATUS
EFIAPI
PlatformAcpiDriverEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  return LocateAndInstallAcpiFromFv (&mAcpiTableFile);
}
