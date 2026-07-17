/** @file
  PCI host bridge library instance for NanHuDev SOC.

  Copyright (C) 2020, Phytium Technology Co Ltd. All rights reserved.<BR>
  Copyright (c) 2024, Bosc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <PiDxe.h>
#include <Protocol/FdtClient.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <RiscVBitOp.h>

#include "PciHostBridgeLib.h"

#define KMH_PCI_RANGE_TYPE_MEM32        0x02000000U
#define KMH_PCI_RANGE_TYPE_MEM64        0x03000000U
#define KMH_PCI_RANGE_TYPE_MASK         0x03000000U
#define KMH_PCIE_RC0_DBI_BASE           0x32000000ULL
#define KMH_PCIE_ECAM_BASE              0x67FF0000ULL
#define KMH_PCIE_MMIO32_CPU_BASE        0x60000000ULL
#define KMH_PCIE_MMIO32_PCI_BASE        0x40000000ULL
#define KMH_PCIE_MMIO32_SIZE            0x07FF0000ULL
#define KMH_PCIE_MMIO64_CPU_BASE        0x4000000000ULL
#define KMH_PCIE_MMIO64_PCI_BASE        0x4000000000ULL
#define KMH_PCIE_MMIO64_SIZE            0x1000000000ULL
#define KMH_UEFI_PCIE_SCAN_BUS_MAX      2
#define KMH_PCIE_CFG0_CPU_OFFSET        0x00100000ULL
#define KMH_PCIE_CFG1_CPU_OFFSET        0x00200000ULL
#define KMH_PCIE_CFG0_SIZE              0x00100000ULL
#define KMH_PCIE_CFG1_SIZE              0x0FE00000ULL
#define KMH_PCIE_CFG0_PCI_BASE          0x01000000ULL
#define KMH_PCIE_CFG1_PCI_BASE          0x02000000ULL

#define PCI_COMMAND_OFFSET              0x04
#define PCI_COMMAND_IO                  BIT(0)
#define PCI_COMMAND_MEMORY              BIT(1)
#define PCI_COMMAND_MASTER              BIT(2)
#define PCI_COMMAND_SERR                BIT(8)
#define PCI_BASE_ADDRESS_0              0x10
#define PCI_BASE_ADDRESS_1              0x14
#define PCI_PRIMARY_BUS                 0x18
#define PCI_INTERRUPT_LINE              0x3C
#define PCI_CLASS_DEVICE                0x0A
#define PCI_CLASS_BRIDGE_PCI            0x0604

#define PCIE_ATU_VIEWPORT               0x900
#define PCIE_ATU_VIEWPORT_BASE          0x904
#define PCIE_ATU_REGION_CTRL1           0x000
#define PCIE_ATU_REGION_CTRL2           0x004
#define PCIE_ATU_LOWER_BASE             0x008
#define PCIE_ATU_UPPER_BASE             0x00C
#define PCIE_ATU_LIMIT                  0x010
#define PCIE_ATU_LOWER_TARGET           0x014
#define PCIE_ATU_UPPER_TARGET           0x018
#define PCIE_ATU_TYPE_MEM               0x0
#define PCIE_ATU_TYPE_CFG0              0x4
#define PCIE_ATU_TYPE_CFG1              0x5
#define PCIE_ATU_ENABLE                 BIT(31)
#define PCIE_ATU_CFG_SHIFT_MODE_ENABLE  BIT(28)

#define PCIE_MISC_CONTROL_1_OFF         0x8BC
#define PCIE_DBI_RO_WR_EN               BIT(0)
#define PCIE_PORT_DEBUG1                0x72C
#define PCIE_PORT_DEBUG1_LINK_UP        BIT(4)
#define PCIE_LINK_WIDTH_SPEED_CONTROL   0x80C
#define PORT_LOGIC_SPEED_CHANGE         BIT(17)

#pragma pack(1)

typedef struct {
  ACPI_HID_DEVICE_PATH     AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;

#pragma pack ()

typedef struct {
  BOOLEAN  Found;
  UINT64   McfgBase;
  UINT8    BusMin;
  UINT8    BusMax;
  BOOLEAN  FoundMmio32;
  UINT64   Mmio32CpuBase;
  UINT64   Mmio32PciBase;
  UINT64   Mmio32Size;
  BOOLEAN  FoundMmio64;
  UINT64   Mmio64CpuBase;
  UINT64   Mmio64PciBase;
  UINT64   Mmio64Size;
} KMH_DT_PCIE_RC_INFO;

STATIC CONST EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mEfiPciRootBridgeDevicePath = {
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH) >> 8)
      }
    },
    EISA_PNP_ID (0x0A08),
    0
  },
  END_DEVICE_PATH_DEF
};

GLOBAL_REMOVE_IF_UNREFERENCED
CHAR16 *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

STATIC
UINT64
KmhPcieFdtReadCells (
  IN CONST UINT32  *Cells,
  IN UINTN         CellCount
  )
{
  UINT64  Value;
  UINTN   Index;

  Value = 0;
  for (Index = 0; Index < CellCount; Index++) {
    Value = LShiftU64 (Value, 32) | SwapBytes32 (Cells[Index]);
  }

  return Value;
}

STATIC
EFI_STATUS
KmhPcieGetRcInfoFromDt (
  OUT KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  INT32                Node;

  if (RcInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  RcInfo->Found         = FALSE;
  RcInfo->McfgBase      = 0;
  RcInfo->BusMin        = 0;
  RcInfo->BusMax        = 0xff;
  RcInfo->FoundMmio32   = FALSE;
  RcInfo->Mmio32CpuBase = 0;
  RcInfo->Mmio32PciBase = 0;
  RcInfo->Mmio32Size    = 0;
  RcInfo->FoundMmio64   = FALSE;
  RcInfo->Mmio64CpuBase = 0;
  RcInfo->Mmio64PciBase = 0;
  RcInfo->Mmio64Size    = 0;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-PCIE-DT: FDT client not available: %r\n", Status));
    return Status;
  }

  for (Status = FdtClient->FindCompatibleNode (FdtClient, "snps,dw-pcie", &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, "snps,dw-pcie", Node, &Node))
  {
    CONST UINT32  *Property;
    UINT32        PropertySize;
    UINT64        DbiBase;

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Property, &PropertySize);
    if (EFI_ERROR (Status) || (PropertySize < 4 * sizeof (UINT32))) {
      continue;
    }

    DbiBase = KmhPcieFdtReadCells (Property, 2);
    if (DbiBase != KMH_PCIE_RC0_DBI_BASE) {
      continue;
    }

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "bus-range", (CONST VOID **)&Property, &PropertySize);
    if (!EFI_ERROR (Status) && (PropertySize == 2 * sizeof (UINT32))) {
      RcInfo->BusMin = (UINT8)SwapBytes32 (Property[0]);
      RcInfo->BusMax = (UINT8)SwapBytes32 (Property[1]);
    } else {
      DEBUG ((DEBUG_WARN, "KMH-PCIE-DT: RC0 node=%d has no bus-range, use 0-255\n", Node));
    }

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "ranges", (CONST VOID **)&Property, &PropertySize);
    if (!EFI_ERROR (Status) && ((PropertySize % (7 * sizeof (UINT32))) == 0)) {
      UINTN  Offset;

      for (Offset = 0; Offset < PropertySize / sizeof (UINT32); Offset += 7) {
        UINT32  Type;
        UINT64  PciBase;
        UINT64  CpuBase;
        UINT64  Size;

        Type    = SwapBytes32 (Property[Offset]) & KMH_PCI_RANGE_TYPE_MASK;
        PciBase = KmhPcieFdtReadCells (&Property[Offset + 1], 2);
        CpuBase = KmhPcieFdtReadCells (&Property[Offset + 3], 2);
        Size    = KmhPcieFdtReadCells (&Property[Offset + 5], 2);

        if (Type == KMH_PCI_RANGE_TYPE_MEM32) {
          RcInfo->FoundMmio32   = TRUE;
          RcInfo->Mmio32PciBase = PciBase;
          RcInfo->Mmio32CpuBase = CpuBase;
          RcInfo->Mmio32Size    = Size;
          RcInfo->McfgBase      = CpuBase + Size;
        } else if (Type == KMH_PCI_RANGE_TYPE_MEM64) {
          RcInfo->FoundMmio64   = TRUE;
          RcInfo->Mmio64PciBase = PciBase;
          RcInfo->Mmio64CpuBase = CpuBase;
          RcInfo->Mmio64Size    = Size;
        }
      }
    }

    if (!RcInfo->FoundMmio32) {
      DEBUG ((DEBUG_WARN, "KMH-PCIE-DT: RC0 node=%d has no MEM32 range\n", Node));
      return EFI_NOT_FOUND;
    }

    RcInfo->Found = TRUE;
    DEBUG ((DEBUG_INFO, "KMH-PCIE-DT: RC0 bus=%u-%u ecam=0x%lx mem32 pci=0x%lx cpu=0x%lx size=0x%lx\n",
      RcInfo->BusMin,
      RcInfo->BusMax,
      RcInfo->McfgBase,
      RcInfo->Mmio32PciBase,
      RcInfo->Mmio32CpuBase,
      RcInfo->Mmio32Size
      ));
    if (RcInfo->FoundMmio64) {
      DEBUG ((DEBUG_INFO, "KMH-PCIE-DT: RC0 mem64 pci=0x%lx cpu=0x%lx size=0x%lx\n",
        RcInfo->Mmio64PciBase,
        RcInfo->Mmio64CpuBase,
        RcInfo->Mmio64Size
        ));
    }

    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_WARN, "KMH-PCIE-DT: RC0 node not found in DT: %r\n", Status));
  return EFI_NOT_FOUND;
}

STATIC
VOID
KmhPcieDbiRoWriteEnable (
  IN BOOLEAN Enable
  )
{
  UINT32 Val;

  Val = MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCIE_MISC_CONTROL_1_OFF);
  if (Enable) {
    Val |= PCIE_DBI_RO_WR_EN;
  } else {
    Val &= ~PCIE_DBI_RO_WR_EN;
  }

  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCIE_MISC_CONTROL_1_OFF, Val);
}

STATIC
VOID
KmhPcieAtuWrite32 (
  IN UINT32 Index,
  IN UINT32 Offset,
  IN UINT32 Value
  )
{
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCIE_ATU_VIEWPORT, Index);
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCIE_ATU_VIEWPORT_BASE + Offset, Value);
}

STATIC
UINT32
KmhPcieAtuRead32 (
  IN UINT32 Index,
  IN UINT32 Offset
  )
{
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCIE_ATU_VIEWPORT, Index);
  return MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCIE_ATU_VIEWPORT_BASE + Offset);
}

STATIC
VOID
KmhPcieProgramOutboundAtu (
  IN UINT32 Index,
  IN UINT32 Type,
  IN UINT64 CpuBase,
  IN UINT64 PciBase,
  IN UINT64 Size,
  IN UINT32 Ctrl2Extra
  )
{
  UINT64 Limit;
  UINT32 Retry;
  UINT32 Ctrl2;

  Limit = CpuBase + Size - 1;

  KmhPcieAtuWrite32 (Index, PCIE_ATU_LOWER_BASE, (UINT32)CpuBase);
  KmhPcieAtuWrite32 (Index, PCIE_ATU_UPPER_BASE, (UINT32)(CpuBase >> 32));
  KmhPcieAtuWrite32 (Index, PCIE_ATU_LIMIT, (UINT32)Limit);
  KmhPcieAtuWrite32 (Index, PCIE_ATU_LOWER_TARGET, (UINT32)PciBase);
  KmhPcieAtuWrite32 (Index, PCIE_ATU_UPPER_TARGET, (UINT32)(PciBase >> 32));
  KmhPcieAtuWrite32 (Index, PCIE_ATU_REGION_CTRL1, Type);
  KmhPcieAtuWrite32 (Index, PCIE_ATU_REGION_CTRL2, PCIE_ATU_ENABLE | Ctrl2Extra);

  for (Retry = 0; Retry < 5; Retry++) {
    Ctrl2 = KmhPcieAtuRead32 (Index, PCIE_ATU_REGION_CTRL2);
    if ((Ctrl2 & PCIE_ATU_ENABLE) != 0) {
      break;
    }

    MicroSecondDelay (9000);
  }

  DEBUG ((DEBUG_INFO, "KMH-PCIE: ATU%u type=0x%x cpu=0x%lx pci=0x%lx size=0x%lx ctrl2=0x%x\n",
    Index,
    Type,
    CpuBase,
    PciBase,
    Size,
    KmhPcieAtuRead32 (Index, PCIE_ATU_REGION_CTRL2)
    ));
}

STATIC
VOID
KmhPcieWaitForLink (
  VOID
  )
{
  UINT32 Retry;
  UINT32 Debug1;

  for (Retry = 0; Retry < 100; Retry++) {
    Debug1 = MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCIE_PORT_DEBUG1);
    if ((Debug1 & PCIE_PORT_DEBUG1_LINK_UP) != 0) {
      DEBUG ((DEBUG_INFO, "KMH-PCIE: link up debug1=0x%x retry=%u\n", Debug1, Retry));
      return;
    }

    MicroSecondDelay (10000);
  }

  DEBUG ((DEBUG_ERROR, "KMH-PCIE: link not up debug1=0x%x\n", Debug1));
}

STATIC
VOID
KmhPcieInitRc0 (
  IN CONST KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  UINT32  Val;
  UINT64  EcamBase;
  UINT64  Mmio32CpuBase;
  UINT64  Mmio32PciBase;
  UINT64  Mmio32Size;
  UINT64  Mmio64CpuBase;
  UINT64  Mmio64PciBase;
  UINT64  Mmio64Size;

  EcamBase       = KMH_PCIE_ECAM_BASE;
  Mmio32CpuBase  = KMH_PCIE_MMIO32_CPU_BASE;
  Mmio32PciBase  = KMH_PCIE_MMIO32_PCI_BASE;
  Mmio32Size     = KMH_PCIE_MMIO32_SIZE;
  Mmio64CpuBase  = KMH_PCIE_MMIO64_CPU_BASE;
  Mmio64PciBase  = KMH_PCIE_MMIO64_PCI_BASE;
  Mmio64Size     = KMH_PCIE_MMIO64_SIZE;

  if ((RcInfo != NULL) && RcInfo->Found && RcInfo->FoundMmio32) {
    EcamBase      = RcInfo->McfgBase;
    Mmio32CpuBase = RcInfo->Mmio32CpuBase;
    Mmio32PciBase = RcInfo->Mmio32PciBase;
    Mmio32Size    = RcInfo->Mmio32Size;
    if (RcInfo->FoundMmio64) {
      Mmio64CpuBase = RcInfo->Mmio64CpuBase;
      Mmio64PciBase = RcInfo->Mmio64PciBase;
      Mmio64Size    = RcInfo->Mmio64Size;
    }
  }

  DEBUG ((DEBUG_INFO, "KMH-PCIE: RC0 init dbi=0x%lx ecam=0x%lx mem32 cpu=0x%lx pci=0x%lx size=0x%lx mem64 cpu=0x%lx pci=0x%lx size=0x%lx\n",
    KMH_PCIE_RC0_DBI_BASE,
    EcamBase,
    Mmio32CpuBase,
    Mmio32PciBase,
    Mmio32Size,
    Mmio64CpuBase,
    Mmio64PciBase,
    Mmio64Size
    ));

  KmhPcieDbiRoWriteEnable (TRUE);

  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCI_BASE_ADDRESS_0, 0x00000004);
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCI_BASE_ADDRESS_1, 0x00000000);

  Val = MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCI_INTERRUPT_LINE);
  Val &= 0xFFFF00FF;
  Val |= 0x00000100;
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCI_INTERRUPT_LINE, Val);

  Val = MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCI_PRIMARY_BUS);
  Val &= 0xFF000000;
  Val |= 0x00FF0100;
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCI_PRIMARY_BUS, Val);

  Val = MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCI_COMMAND_OFFSET);
  Val &= 0xFFFF0000;
  Val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCI_COMMAND_OFFSET, Val);

  MmioWrite16 (KMH_PCIE_RC0_DBI_BASE + PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

  KmhPcieProgramOutboundAtu (0, PCIE_ATU_TYPE_CFG0, EcamBase, 0, KMH_PCIE_CFG0_SIZE, PCIE_ATU_CFG_SHIFT_MODE_ENABLE);
  KmhPcieProgramOutboundAtu (1, PCIE_ATU_TYPE_CFG0, EcamBase + KMH_PCIE_CFG0_CPU_OFFSET, KMH_PCIE_CFG0_PCI_BASE, KMH_PCIE_CFG0_SIZE, PCIE_ATU_CFG_SHIFT_MODE_ENABLE);
  KmhPcieProgramOutboundAtu (2, PCIE_ATU_TYPE_CFG1, EcamBase + KMH_PCIE_CFG1_CPU_OFFSET, KMH_PCIE_CFG1_PCI_BASE, KMH_PCIE_CFG1_SIZE, PCIE_ATU_CFG_SHIFT_MODE_ENABLE);
  KmhPcieProgramOutboundAtu (3, PCIE_ATU_TYPE_MEM, Mmio32CpuBase, Mmio32PciBase, Mmio32Size, 0);
  if (Mmio64Size != 0) {
    DEBUG ((DEBUG_INFO, "KMH-PCIE: MEM64 ATU not programmed during UEFI PCI scan\n"));
  }

  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCI_BASE_ADDRESS_0, 0);
  MmioWrite16 (KMH_PCIE_RC0_DBI_BASE + PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

  Val = MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCIE_LINK_WIDTH_SPEED_CONTROL);
  Val |= PORT_LOGIC_SPEED_CHANGE;
  MmioWrite32 (KMH_PCIE_RC0_DBI_BASE + PCIE_LINK_WIDTH_SPEED_CONTROL, Val);

  KmhPcieDbiRoWriteEnable (FALSE);
  KmhPcieWaitForLink ();

  DEBUG ((DEBUG_INFO, "KMH-PCIE: DBI vendor=0x%x class=0x%x cmd=0x%x bus=0x%x\n",
    MmioRead32 (KMH_PCIE_RC0_DBI_BASE),
    MmioRead32 (KMH_PCIE_RC0_DBI_BASE + 0x08),
    MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCI_COMMAND_OFFSET),
    MmioRead32 (KMH_PCIE_RC0_DBI_BASE + PCI_PRIMARY_BUS)
    ));
}

STATIC PCI_ROOT_BRIDGE mRootBridge = {
  0,                                              // Segment
  0,                                              // Supports
  0,                                              // Attributes
  FALSE,                                          // DmaAbove4G
  FALSE,                                          // NoExtendedConfigSpace
  FALSE,                                          // ResourceAssigned
  EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM,           // AllocationAttributes
  {
    // Bus
    FixedPcdGet32 (PcdPciBusMin),
    FixedPcdGet32 (PcdPciBusMax)
  }, {
    // Io
    FixedPcdGet64 (PcdPciIoBase),
    FixedPcdGet64 (PcdPciIoBase) + FixedPcdGet64 (PcdPciIoSize) - 1
  }, {
    // Mem
    FixedPcdGet32 (PcdPciMmio32Base),
    FixedPcdGet32 (PcdPciMmio32Base) + (FixedPcdGet32 (PcdPciMmio32Size) - 1)
    //0x7FFFFFFF
  }, {
    // MemAbove4G
    FixedPcdGet64 (PcdPciMmio64Size) ? FixedPcdGet64 (PcdPciMmio64Base) : MAX_UINT64,
    FixedPcdGet64 (PcdPciMmio64Size) ? FixedPcdGet64 (PcdPciMmio64Base) + FixedPcdGet64 (PcdPciMmio64Size) - 1 : 0
  }, {
    // PMem
    MAX_UINT64,
    0
  }, {
    // PMemAbove4G
    MAX_UINT64,
    0
  },
  (EFI_DEVICE_PATH_PROTOCOL *)&mEfiPciRootBridgeDevicePath
};

STATIC
VOID
KmhPcieApplyRootBridgeFromDt (
  IN CONST KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  if ((RcInfo == NULL) || !RcInfo->Found || !RcInfo->FoundMmio32) {
    DEBUG ((DEBUG_WARN, "KMH-PCIE-DT: use static PCD root bridge apertures\n"));
    return;
  }

  mRootBridge.AllocationAttributes = EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
  mRootBridge.Bus.Base  = RcInfo->BusMin;
  mRootBridge.Bus.Limit = RcInfo->BusMax;
  if (mRootBridge.Bus.Limit > KMH_UEFI_PCIE_SCAN_BUS_MAX) {
    DEBUG ((
      DEBUG_WARN,
      "KMH-PCIE-DT: cap UEFI PCI scan bus range 0x%lx-0x%lx to 0x%lx-0x%x\n",
      mRootBridge.Bus.Base,
      mRootBridge.Bus.Limit,
      mRootBridge.Bus.Base,
      KMH_UEFI_PCIE_SCAN_BUS_MAX
      ));
    mRootBridge.Bus.Limit = KMH_UEFI_PCIE_SCAN_BUS_MAX;
  }

  mRootBridge.Mem.Base        = RcInfo->Mmio32PciBase;
  mRootBridge.Mem.Limit       = RcInfo->Mmio32PciBase + RcInfo->Mmio32Size - 1;
  mRootBridge.Mem.Translation = RcInfo->Mmio32PciBase - RcInfo->Mmio32CpuBase;

  if (RcInfo->FoundMmio64 && (RcInfo->Mmio64Size != 0)) {
    mRootBridge.AllocationAttributes |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
    mRootBridge.MemAbove4G.Base        = RcInfo->Mmio64PciBase;
    mRootBridge.MemAbove4G.Limit       = RcInfo->Mmio64PciBase + RcInfo->Mmio64Size - 1;
    mRootBridge.MemAbove4G.Translation = RcInfo->Mmio64PciBase - RcInfo->Mmio64CpuBase;
  } else {
    mRootBridge.MemAbove4G.Base        = MAX_UINT64;
    mRootBridge.MemAbove4G.Limit       = 0;
    mRootBridge.MemAbove4G.Translation = 0;
  }

  DEBUG ((DEBUG_INFO, "KMH-PCIE-DT: root bridge aperture attr=0x%lx bus=0x%lx-0x%lx mem=0x%lx-0x%lx trans=0x%lx mem64=0x%lx-0x%lx trans=0x%lx\n",
    mRootBridge.AllocationAttributes,
    mRootBridge.Bus.Base,
    mRootBridge.Bus.Limit,
    mRootBridge.Mem.Base,
    mRootBridge.Mem.Limit,
    mRootBridge.Mem.Translation,
    mRootBridge.MemAbove4G.Base,
    mRootBridge.MemAbove4G.Limit,
    mRootBridge.MemAbove4G.Translation
    ));
}

/**
  Return all the root bridge instances in an array.

  @param[out] Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.

**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  OUT UINTN     *Count
  )
{
  EFI_STATUS           Status;
  KMH_DT_PCIE_RC_INFO  RcInfo;

  Status = KmhPcieGetRcInfoFromDt (&RcInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-PCIE-DT: failed to get RC0 info from DT: %r\n", Status));
  }

  KmhPcieApplyRootBridgeFromDt (EFI_ERROR (Status) ? NULL : &RcInfo);
  KmhPcieInitRc0 (EFI_ERROR (Status) ? NULL : &RcInfo);

  *Count = 1;
  return &mRootBridge;
}


/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param[in] Bridges The root bridge instances array.
  @param[in] Count   The count of the array.

**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  IN PCI_ROOT_BRIDGE *Bridges,
  IN UINTN           Count
  )
{

}


/**
  Inform the platform that the resource conflict happens.

  @param[in] HostBridgeHandle Handle of the Host Bridge.
  @param[in] Configuration    Pointer to PCI I/O and PCI memory resource
                          descriptors. The Configuration contains the resources
                          for all the root bridges. The resource for each root
                          bridge is terminated with END descriptor and an
                          additional END is appended indicating the end of the
                          entire resources. The resource descriptor field
                          values follow the description in
                          EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                          SubmitResources().

**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  IN EFI_HANDLE                        HostBridgeHandle,
  IN VOID                              *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptor;
  BOOLEAN IsPrefetchable;

  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    for (; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (Descriptor->ResType <
              ARRAY_SIZE (mPciHostBridgeLibAcpiAddressSpaceTypeStr));
      DEBUG ((DEBUG_INFO, " %s: Length/Alignment = 0x%lx / 0x%lx\n",
              mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
              Descriptor->AddrLen,
              Descriptor->AddrRangeMax
              ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {

        IsPrefetchable = (Descriptor->SpecificFlag &
          EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0;

        DEBUG ((DEBUG_INFO, "     Granularity/SpecificFlag = %ld / %02x%s\n",
          Descriptor->AddrSpaceGranularity,
          Descriptor->SpecificFlag,
          (IsPrefetchable) ? L" (Prefetchable)" : L""
          ));
      }
    }
    //
    // Skip the end descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) (
                   (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                   );
  }
}
