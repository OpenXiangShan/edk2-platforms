/** @file
The library call to pass the device tree to DXE via HOB.

Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>

/**
  Cpu Peim initialization.

**/
EFI_STATUS
CpuPeimInitialization (
  VOID
  )
{
  //
  // Cover KmhBoscSoc die3 PCIe ECAM/MMIO windows below 2^46.
  //
  BuildCpuHob (46, 39);

  return EFI_SUCCESS;
}
