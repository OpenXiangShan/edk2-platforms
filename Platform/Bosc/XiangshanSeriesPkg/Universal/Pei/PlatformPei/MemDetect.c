/** @file
  Memory Detection for Virtual Machines.

  Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

  MemDetect.c

**/

//
// The package level header files this module uses
//
#include <PiPei.h>

//
// The Library classes this module consumes
//
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/ResourcePublicationLib.h>

#include <Library/RiscVFirmwareContextLib.h>

#include <libfdt.h>

#include "Platform.h"

/**
  Publish PEI core memory.

  @return EFI_SUCCESS     The PEIM initialized successfully.

**/
EFI_STATUS
PublishPeiMemory (
  IN EFI_PHYSICAL_ADDRESS  MemoryBase,
  IN UINT64                MemorySize
  )
{
  EFI_STATUS            Status;

  DEBUG ((DEBUG_INFO, "PublishPeiMemory(): MemoryBase:0x%lx MemorySize:0x%lx\n", MemoryBase, MemorySize));

  //
  // Publish this memory to the PEI Core
  //
  Status = PublishSystemMemory (MemoryBase, MemorySize);
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Publish system RAM and reserve memory regions.

**/
VOID
InitializeRamRegions (
  IN EFI_PHYSICAL_ADDRESS  SystemMemoryBase,
  IN UINT64                SystemMemorySize
  )
{
  AddMemoryRangeHob (
    SystemMemoryBase,
    SystemMemoryBase + SystemMemorySize
    );
}

/**
  Initialize memory hob based on the DTB information.

  @return EFI_SUCCESS     The memory hob added successfully.

**/
EFI_STATUS
MemPeimInitialization (
  VOID
  )
{
  CONST UINT64                        *RegProp;
  CONST CHAR8                         *Type;
  UINT64                              CurBase, CurSize;
  INT32                               Node, Prev;
  INT32                               Len;
  VOID                                *DeviceTreeBase;
  EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT  *FirmwareContext;

  FirmwareContext = NULL;
  GetFirmwareContextPointer (&FirmwareContext);

  if (FirmwareContext == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: OpenSBI Firmware Context is NULL\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  DeviceTreeBase = (VOID *)FirmwareContext->FlattenedDeviceTree;
  if (DeviceTreeBase == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid FDT pointer\n", __func__));
    return EFI_UNSUPPORTED;
  }

  // Make sure we have a valid device tree blob
  ASSERT (fdt_check_header (DeviceTreeBase) == 0);
  DEBUG ((DEBUG_INFO, "%a: DeviceTreeBase at address: 0x%x \n", __FUNCTION__, DeviceTreeBase));

  // Look for the lowest memory node
  for (Prev = 0;; Prev = Node) {
    Node = fdt_next_node (DeviceTreeBase, Prev, NULL);
    if (Node < 0) {
      break;
    }

    // Check for memory node
    Type = fdt_getprop (DeviceTreeBase, Node, "device_type", &Len);
    if (Type && (AsciiStrnCmp (Type, "memory", Len) == 0)) {
      // Get the 'reg' property of this node. For now, we will assume
      // two 8 byte quantities for base and size, respectively.
      RegProp = fdt_getprop (DeviceTreeBase, Node, "reg", &Len);
      if ((RegProp != 0) && (Len == (2 * sizeof (UINT64)))) {
        CurBase = fdt64_to_cpu (ReadUnaligned64 (RegProp));
        CurSize = fdt64_to_cpu (ReadUnaligned64 (RegProp + 1));

        DEBUG ((
          DEBUG_INFO,
          "%a: System RAM @ 0x%lx - 0x%lx\n",
          __func__,
          CurBase,
          CurBase + CurSize - 1
          ));
        //0x80000000 is DDR Start Address
        //64MB is payload
        if (CurBase == 0x80000000) {
          CurBase = CurBase + 0x4000000UL + 0x1000000; // 64MB + 16MB
          CurSize = CurSize - 0x4000000UL - 0x1000000; // MemorySize - 64M - 16MB
        }

        PublishPeiMemory (CurBase, CurSize);
        DEBUG ((DEBUG_INFO, "PEI memory published.\n"));
        InitializeRamRegions (CurBase, CurSize);
        DEBUG ((DEBUG_INFO, "Platform RAM regions initiated.\n"));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Failed to parse FDT memory node\n",
          __FUNCTION__));
      }
    }
  }

  return EFI_SUCCESS;
}
