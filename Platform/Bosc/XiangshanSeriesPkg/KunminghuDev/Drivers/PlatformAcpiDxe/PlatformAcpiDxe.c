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
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/AcpiLib.h>
#include <IndustryStandard/Acpi20.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/FdtClient.h>

#define EFI_ACPI_OEM_ID                 {'B','O','S','C',' ',' '}
#define EFI_ACPI_OEM_TABLE_ID           SIGNATURE_64 ('K','M','H',' ',' ',' ',' ',' ')
#define EFI_ACPI_OEM_REVISION           0x00000001
#define EFI_ACPI_CREATOR_ID             SIGNATURE_32 ('E','D','K','2')
#define EFI_ACPI_CREATOR_REVISION       0x00000001
#define EFI_ACPI_RHCT_SIGNATURE         SIGNATURE_32 ('R','H','C','T')
#define EFI_ACPI_RHCT_REVISION          0x01
#define KMH_PCI_RANGE_TYPE_MEM32        0x02000000U
#define KMH_PCI_RANGE_TYPE_MEM64        0x03000000U
#define KMH_PCI_RANGE_TYPE_MASK         0x03000000U
#define KMH_UART0_BASE                  0x310B0000ULL
#define KMH_UART0_ACPI_SIZE             0x00000100ULL
#define KMH_UART0_CLOCK_FREQUENCY       50000000U
#define KMH_UART0_REG_SHIFT             2U
#define KMH_UART0_REG_IO_WIDTH          4U
#define KMH_UART0_CURRENT_SPEED         115200U
#define KMH_APLIC_S_BASE                0x31120000ULL
#define KMH_APLIC_S_SIZE                0x00008000ULL
#define KMH_APLIC_S_NUM_SOURCES         96U
#define KMH_IMSIC_S_BASE                0x3B000000ULL
#define KMH_IMSIC_S_SIZE                0x00080000ULL
#define KMH_IMSIC_S_PER_HART_SIZE       0x00008000U
#define KMH_IMSIC_S_STRIDE              0x00008000ULL
#define KMH_IMSIC_S_NUM_IDS             255U
#define KMH_IMSIC_S_NUM_GUEST_IDS       255U
#define KMH_IMSIC_S_GUEST_INDEX_BITS    3U
#define KMH_IMSIC_S_HART_INDEX_BITS     4U
#define KMH_IMSIC_S_GROUP_INDEX_BITS    0U
#define KMH_IMSIC_S_GROUP_INDEX_SHIFT   24U
#define KMH_CLINT_BASE                  0x38000000ULL
#define KMH_CLINT_SIZE                  0x00010000ULL
#define KMH_MADT_MAX_CPU_COUNT          64U
#define KMH_RHCT_BASE_FREQUENCY         1000000ULL
#define KMH_RHCT_ISA_STRING_LENGTH      1024U
#define KMH_RHCT_HART_INFO_TYPE         0xFFFFU
#define KMH_RHCT_ISA_STRING_TYPE        0U
#define KMH_RHCT_CMO_TYPE               1U
#define KMH_RHCT_MMU_TYPE               2U
#define KMH_RHCT_NODE_REVISION          1U
#define KMH_RHCT_MMU_TYPE_SV48          1U
#define KMH_PCIE_MAX_RC_COUNT           4U
#define KMH_PCIE_CFG0_SIZE              0x00100000ULL
#define KMH_PCIE_CFG1_SIZE              0x0FE00000ULL
#define KMH_PCIE_CFG0_PCI_BASE          0x01000000ULL
#define KMH_PCIE_CFG1_PCI_BASE          0x02000000ULL
#define KMH_ACPI_TABLE_CHECKSUM_OFFSET  9
#define KMH_ENABLE_ECAM_RESERVATION_SSDT 0
#define KMH_ENABLE_OPTIONAL_SDH0_ACPI   0
#define KMH_ENABLE_OPTIONAL_XDM0_ACPI   0
#define KMH_ENABLE_OPTIONAL_VIRTIO_MMIO_ACPI 0
#if KMH_ENABLE_ECAM_RESERVATION_SSDT
#define KMH_ECAM_SSDT_BASE_OFFSET       0x4E
#define KMH_ECAM_SSDT_SIZE_OFFSET       0x52
#endif
#define KMH_PCI_ECAM_BUS_SIZE           0x00100000ULL
#define PCI_COMMAND_OFFSET              0x04
#define PCI_COMMAND_IO                  BIT0
#define PCI_COMMAND_MEMORY              BIT1
#define PCI_COMMAND_MASTER              BIT2
#define PCI_COMMAND_SERR                BIT8
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
#define PCIE_ATU_ENABLE                 BIT31
#define PCIE_ATU_CFG_SHIFT_MODE_ENABLE  BIT28

#define PCIE_MISC_CONTROL_1_OFF         0x8BC
#define PCIE_DBI_RO_WR_EN               BIT0
#define PCIE_PORT_DEBUG1                0x72C
#define PCIE_PORT_DEBUG1_LINK_UP        BIT4
#define PCIE_LINK_WIDTH_SPEED_CONTROL   0x80C
#define PORT_LOGIC_SPEED_CHANGE         BIT17
STATIC CONST EFI_GUID mAcpiTableFile = {
  0x84D13218, 0x81C2, 0x4BD5, { 0xBC, 0x52, 0x84, 0xC0, 0x7D, 0x0C, 0x25, 0x3D }
};

STATIC CONST UINT8 mKmhAcpiOemId[6] = EFI_ACPI_OEM_ID;

STATIC EFI_EVENT mKmhAcpiPcieExitBootServicesEvent;
STATIC EFI_STATUS mKmhAcpiPcieRcInfoStatus = EFI_NOT_READY;
#if KMH_ENABLE_ECAM_RESERVATION_SSDT
STATIC CONST UINT8 mKmhEcamReservationSsdtTemplate[] = {
  0x53, 0x53, 0x44, 0x54, 0x58, 0x00, 0x00, 0x00, 0x02, 0xd6, 0x42, 0x4f,
  0x53, 0x43, 0x20, 0x20, 0x4b, 0x4d, 0x48, 0x45, 0x43, 0x41, 0x4d, 0x20,
  0x01, 0x00, 0x00, 0x00, 0x49, 0x4e, 0x54, 0x4c, 0x28, 0x06, 0x23, 0x20,
  0x10, 0x33, 0x5f, 0x53, 0x42, 0x5f, 0x5b, 0x82, 0x2c, 0x45, 0x43, 0x41,
  0x4d, 0x08, 0x5f, 0x48, 0x49, 0x44, 0x0c, 0x41, 0xd0, 0x0c, 0x02, 0x08,
  0x5f, 0x55, 0x49, 0x44, 0x00, 0x08, 0x5f, 0x43, 0x52, 0x53, 0x11, 0x11,
  0x0a, 0x0e, 0x86, 0x09, 0x00, 0x00, 0x00, 0x00, 0xff, 0x67, 0x00, 0x00,
  0x00, 0x10, 0x79, 0x00
};
#endif
#pragma pack (1)

typedef struct {
  EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER                         Header;
  EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE  Allocation[KMH_PCIE_MAX_RC_COUNT];
} KMH_ACPI_MCFG_TABLE;

typedef struct {
  UINT8   Type;
  UINT8   Length;
  UINT8   Version;
  UINT8   Id;
  UINT32  Flags;
  UINT8   HwId[8];
  UINT16  NumIdcs;
  UINT16  NumSrcs;
  UINT32  GsiBase;
  UINT64  Base;
  UINT32  Size;
} KMH_ACPI_MADT_APLIC_STRUCTURE;

typedef struct {
  UINT8   Type;
  UINT8   Length;
  UINT8   Version;
  UINT8   AplicId;
  UINT32  Flags;
  UINT16  NumIds;
  UINT16  NumGuestIds;
  UINT8   GuestIdxBits;
  UINT8   HartIdxBits;
  UINT8   GroupIdxBits;
  UINT8   GroupIdxShift;
} KMH_ACPI_MADT_IMSIC_STRUCTURE;
typedef struct {
  UINT8   Type;
  UINT8   Length;
  UINT8   Version;
  UINT8   Reserved;
  UINT32  Flags;
  UINT64  HartId;
  UINT32  Uid;
  UINT32  ExtIntcId;
  UINT64  ImsicBase;
  UINT32  ImsicSize;
} KMH_ACPI_MADT_INTC_STRUCTURE;
typedef struct {
  UINT16  Type;
  UINT16  Length;
  UINT16  Revision;
} KMH_ACPI_RHCT_NODE_HEADER;

typedef struct {
  UINT16  NumOffsets;
  UINT32  Uid;
} KMH_ACPI_RHCT_HART_INFO;
typedef struct {
  KMH_ACPI_RHCT_NODE_HEADER  Header;
  KMH_ACPI_RHCT_HART_INFO    HartInfo;
} KMH_ACPI_RHCT_HART_INFO_STRUCT;

typedef struct {
  UINT32  Offset[3];
} KMH_ACPI_RHCT_HART_NODE_INFO_OFFSET;

typedef struct {
  UINT16  IsaLength;
  UINT8   IsaString[KMH_RHCT_ISA_STRING_LENGTH];
} KMH_ACPI_RHCT_ISA_STRING;
typedef struct {
  UINT8  Reserved;
  UINT8  CbomBlockSize;
  UINT8  CbopBlockSize;
  UINT8  CbozBlockSize;
} KMH_ACPI_RHCT_CMO;

typedef struct {
  UINT8  Reserved;
  UINT8  MmuType;
} KMH_ACPI_RHCT_MMU;

typedef struct {
  KMH_ACPI_RHCT_NODE_HEADER  Header;
  KMH_ACPI_RHCT_ISA_STRING   IsaData;
} KMH_ACPI_RHCT_ISA_STRING_STRUCT;
typedef struct {
  KMH_ACPI_RHCT_NODE_HEADER  Header;
  KMH_ACPI_RHCT_CMO          CmoData;
} KMH_ACPI_RHCT_CMO_STRUCT;

typedef struct {
  KMH_ACPI_RHCT_NODE_HEADER  Header;
  KMH_ACPI_RHCT_MMU          MmuData;
} KMH_ACPI_RHCT_MMU_STRUCT;

typedef struct {
  KMH_ACPI_RHCT_HART_INFO_STRUCT       HartInfo;
  KMH_ACPI_RHCT_HART_NODE_INFO_OFFSET  Offset;
  KMH_ACPI_RHCT_ISA_STRING_STRUCT      IsaNode;
  KMH_ACPI_RHCT_CMO_STRUCT             CmoNode;
  KMH_ACPI_RHCT_MMU_STRUCT             MmuNode;
} KMH_ACPI_RHCT_NODE_DATA;
typedef struct {
  UINT32  Reserved;
  UINT64  BaseFrequency;
  UINT32  NodeNumber;
  UINT32  OffsetToNodeArray;
} KMH_ACPI_RHCT_HEADER_STRUCTURE;

typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER       Header;
  KMH_ACPI_RHCT_HEADER_STRUCTURE    RhctHeader;
} KMH_ACPI_RHCT_DESCRIPTION_TABLE;

#pragma pack ()
typedef struct {
  BOOLEAN  Found;
  INT32    Node;
  UINT64   DbiBase;
  UINT64   DbiSize;
  UINT64   ConfigBase;
  UINT64   ConfigSize;
  UINT64   McfgBase;
  UINT8    BusMin;
  UINT8    BusMax;
  BOOLEAN  FoundBusRange;
  BOOLEAN  FoundMmio32;
  UINT64   Mmio32CpuBase;
  UINT64   Mmio32PciBase;
  UINT64   Mmio32Size;
  BOOLEAN  FoundMmio64;
  UINT64   Mmio64CpuBase;
  UINT64   Mmio64PciBase;
  UINT64   Mmio64Size;
} KMH_DT_PCIE_RC_INFO;
STATIC KMH_DT_PCIE_RC_INFO mKmhAcpiPcieRcInfo;
STATIC KMH_DT_PCIE_RC_INFO mKmhAcpiPcieRcInfoArray[KMH_PCIE_MAX_RC_COUNT];
STATIC UINTN mKmhAcpiPcieRcCount;
#if KMH_ENABLE_ECAM_RESERVATION_SSDT
STATIC
VOID
KmhAcpiWriteUnaligned32 (
  OUT UINT8  *Buffer,
  IN  UINT32 Value
  )
{
  Buffer[0] = (UINT8)Value;
  Buffer[1] = (UINT8)(Value >> 8);
  Buffer[2] = (UINT8)(Value >> 16);
  Buffer[3] = (UINT8)(Value >> 24);
}

STATIC
UINT8
KmhAcpiChecksum8 (
  IN CONST UINT8  *Buffer,
  IN UINTN        Size
  )
{
  UINT8  Sum;
  UINTN  Index;

  Sum = 0;
  for (Index = 0; Index < Size; Index++) {
    Sum = (UINT8)(Sum + Buffer[Index]);
  }

  return (UINT8)(0 - Sum);
}
#endif
 

 


STATIC
UINT64
KmhAcpiFdtReadCells (
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
KmhAcpiParsePcieRcNodeFromDt (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  INT32                Node,
  OUT KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  EFI_STATUS    Status;
  CONST UINT32  *Property;
  UINT32        PropertySize;
  UINTN         Offset;

  if ((FdtClient == NULL) || (Node < 0) || (RcInfo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (RcInfo, sizeof (*RcInfo));
  RcInfo->Node   = Node;
  RcInfo->BusMin = 0;
  RcInfo->BusMax = 0xff;

  Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Property, &PropertySize);
  if (EFI_ERROR (Status) || (PropertySize < 4 * sizeof (UINT32))) {
    return Status;
  }

  RcInfo->DbiBase = KmhAcpiFdtReadCells (Property, 2);
  RcInfo->DbiSize = KmhAcpiFdtReadCells (&Property[2], 2);
  if (PropertySize >= 8 * sizeof (UINT32)) {
    RcInfo->ConfigBase = KmhAcpiFdtReadCells (&Property[4], 2);
    RcInfo->ConfigSize = KmhAcpiFdtReadCells (&Property[6], 2);
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, "bus-range", (CONST VOID **)&Property, &PropertySize);
  if (!EFI_ERROR (Status) && (PropertySize == 2 * sizeof (UINT32))) {
    UINT32  BusMin;
    UINT32  BusMax;

    BusMin = SwapBytes32 (Property[0]);
    BusMax = SwapBytes32 (Property[1]);
    if ((BusMin > MAX_UINT8) || (BusMax > MAX_UINT8) || (BusMax < BusMin)) {
      return EFI_INVALID_PARAMETER;
    }

    RcInfo->BusMin        = (UINT8)BusMin;
    RcInfo->BusMax        = (UINT8)BusMax;
    RcInfo->FoundBusRange = TRUE;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, "ranges", (CONST VOID **)&Property, &PropertySize);
  if (!EFI_ERROR (Status) && ((PropertySize % (7 * sizeof (UINT32))) == 0)) {
    for (Offset = 0; Offset < PropertySize / sizeof (UINT32); Offset += 7) {
      UINT32  Type;
      UINT64  PciBase;
      UINT64  CpuBase;
      UINT64  Size;

      Type    = SwapBytes32 (Property[Offset]) & KMH_PCI_RANGE_TYPE_MASK;
      PciBase = KmhAcpiFdtReadCells (&Property[Offset + 1], 2);
      CpuBase = KmhAcpiFdtReadCells (&Property[Offset + 3], 2);
      Size    = KmhAcpiFdtReadCells (&Property[Offset + 5], 2);

      if (Type == KMH_PCI_RANGE_TYPE_MEM32) {
        RcInfo->FoundMmio32   = TRUE;
        RcInfo->Mmio32PciBase = PciBase;
        RcInfo->Mmio32CpuBase = CpuBase;
        RcInfo->Mmio32Size    = Size;
      } else if (Type == KMH_PCI_RANGE_TYPE_MEM64) {
        RcInfo->FoundMmio64   = TRUE;
        RcInfo->Mmio64PciBase = PciBase;
        RcInfo->Mmio64CpuBase = CpuBase;
        RcInfo->Mmio64Size    = Size;
      }
    }
  }

  if (RcInfo->ConfigBase != 0) {
    RcInfo->McfgBase = RcInfo->ConfigBase;
  }

  RcInfo->Found = TRUE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
KmhAcpiCollectPcieRcInfoFromDt (
  IN  FDT_CLIENT_PROTOCOL   *FdtClient,
  OUT KMH_DT_PCIE_RC_INFO   *RcInfo,
  IN  UINTN                 MaxRcCount,
  OUT UINTN                 *RcCount
  )
{
  EFI_STATUS           Status;
  INT32                Node;
  KMH_DT_PCIE_RC_INFO  Candidate;
  UINTN                Count;

  if ((FdtClient == NULL) || (RcInfo == NULL) || (RcCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *RcCount = 0;
  Count    = 0;
  for (Status = FdtClient->FindCompatibleNode (FdtClient, "snps,dw-pcie", &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, "snps,dw-pcie", Node, &Node))
  {
    Status = KmhAcpiParsePcieRcNodeFromDt (FdtClient, Node, &Candidate);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "KMH-DT-PCIE: collect node=%d parse failed: %r\n", Node, Status));
      continue;
    }

    if (Count >= MaxRcCount) {
      DEBUG ((DEBUG_WARN, "KMH-DT-PCIE: collect reached max RC count %u, node=%d ignored\n",
        (UINT32)MaxRcCount,
        Node
        ));
      continue;
    }

    CopyMem (&RcInfo[Count], &Candidate, sizeof (RcInfo[Count]));
    Count++;
  }

  *RcCount = Count;
  DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: collect enabled RC count=%u max=%u\n",
    (UINT32)Count,
    (UINT32)MaxRcCount
    ));
  return (Count > 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
KmhAcpiGetPcieRcInfoFromDt (
  OUT KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  KMH_DT_PCIE_RC_INFO  RcInfoArray[KMH_PCIE_MAX_RC_COUNT];
  UINTN                RcCount;
  UINTN                Index;

  if (RcInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (RcInfo, sizeof (*RcInfo));

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-PCIE: FDT client not available: %r\n", Status));
    return Status;
  }

  Status = KmhAcpiCollectPcieRcInfoFromDt (FdtClient, RcInfoArray, KMH_PCIE_MAX_RC_COUNT, &RcCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < RcCount; Index++) {
    if (!RcInfoArray[Index].FoundMmio32 || (RcInfoArray[Index].McfgBase == 0)) {
      continue;
    }

    CopyMem (RcInfo, &RcInfoArray[Index], sizeof (*RcInfo));

    if (!RcInfo->FoundMmio32) {
      DEBUG ((DEBUG_WARN, "KMH-DT-MCFG: RC0 node=%d has no MEM32-derived MCFG base\n", RcInfo->Node));
      return EFI_NOT_FOUND;
    }

    if (!RcInfo->FoundBusRange) {
      DEBUG ((DEBUG_WARN, "KMH-DT-MCFG: RC0 node=%d has no bus-range, use 0-255\n", RcInfo->Node));
    }

    DEBUG ((DEBUG_INFO, "KMH-DT-MCFG: RC0 from DT node=%d base=0x%Lx bus=%u-%u ecam-size=0x%Lx\n",
      RcInfo->Node,
      RcInfo->McfgBase,
      RcInfo->BusMin,
      RcInfo->BusMax,
      MultU64x32 ((UINT64)(RcInfo->BusMax - RcInfo->BusMin + 1), 0x100000)
      ));
    DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: RC0 MEM32 pci=0x%Lx cpu=0x%Lx size=0x%Lx trans=0x%Lx\n",
      RcInfo->Mmio32PciBase,
      RcInfo->Mmio32CpuBase,
      RcInfo->Mmio32Size,
      RcInfo->Mmio32CpuBase - RcInfo->Mmio32PciBase
      ));
    if (RcInfo->FoundMmio64) {
      DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: RC0 MEM64 pci=0x%Lx cpu=0x%Lx size=0x%Lx trans=0x%Lx\n",
        RcInfo->Mmio64PciBase,
        RcInfo->Mmio64CpuBase,
        RcInfo->Mmio64Size,
        RcInfo->Mmio64CpuBase - RcInfo->Mmio64PciBase
        ));
    } else {
      DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: RC0 MEM64 range not present in DT\n"));
    }
    DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: RC0 ACPI CRS candidate bus=%u-%u mem32=[pci 0x%Lx-0x%Lx cpu 0x%Lx-0x%Lx]\n",
      RcInfo->BusMin,
      RcInfo->BusMax,
      RcInfo->Mmio32PciBase,
      RcInfo->Mmio32PciBase + RcInfo->Mmio32Size - 1,
      RcInfo->Mmio32CpuBase,
      RcInfo->Mmio32CpuBase + RcInfo->Mmio32Size - 1
      ));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_WARN, "KMH-DT-PCIE: RC0 node not found in DT\n"));
  return EFI_NOT_FOUND;
}

STATIC
VOID
KmhAcpiValidatePciCrsAgainstDt (
  IN CONST KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  if ((RcInfo == NULL) || !RcInfo->Found || !RcInfo->FoundMmio32) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: skip PCI0._CRS validation because cached DT RC0 info is unavailable\n"));
    return;
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: PCI0._CRS source is DT node=%d dbi=0x%Lx bus=%u-%u mem32 pci=0x%Lx cpu=0x%Lx size=0x%Lx\n",
    RcInfo->Node,
    RcInfo->DbiBase,
    RcInfo->BusMin,
    RcInfo->BusMax,
    RcInfo->Mmio32PciBase,
    RcInfo->Mmio32CpuBase,
    RcInfo->Mmio32Size
    ));
  if (RcInfo->FoundMmio64) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: PCI0._CRS DT MEM64 pci=0x%Lx cpu=0x%Lx size=0x%Lx\n",
      RcInfo->Mmio64PciBase,
      RcInfo->Mmio64CpuBase,
      RcInfo->Mmio64Size
      ));
  }
}

STATIC
VOID
KmhAcpiValidatePci1CrsAgainstDt (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS           Status;
  KMH_DT_PCIE_RC_INFO  RcInfoArray[KMH_PCIE_MAX_RC_COUNT];
  UINTN                RcCount;

  if (FdtClient == NULL) {
    return;
  }

  Status = KmhAcpiCollectPcieRcInfoFromDt (FdtClient, RcInfoArray, KMH_PCIE_MAX_RC_COUNT, &RcCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: skip PCI1._CRS validation because PCIe RC collection failed: %r\n", Status));
    return;
  }

  if (RcCount < 2) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: PCI1._CRS validation skipped, runtime DT has %u PCIe RC\n", (UINT32)RcCount));
    return;
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: PCI1._CRS source is DT node=%d dbi=0x%Lx bus=%u-%u mem32 found=%u pci=0x%Lx cpu=0x%Lx size=0x%Lx mem64 found=%u pci=0x%Lx cpu=0x%Lx size=0x%Lx\n",
    RcInfoArray[1].Node,
    RcInfoArray[1].DbiBase,
    RcInfoArray[1].BusMin,
    RcInfoArray[1].BusMax,
    RcInfoArray[1].FoundMmio32,
    RcInfoArray[1].Mmio32PciBase,
    RcInfoArray[1].Mmio32CpuBase,
    RcInfoArray[1].Mmio32Size,
    RcInfoArray[1].FoundMmio64,
    RcInfoArray[1].Mmio64PciBase,
    RcInfoArray[1].Mmio64CpuBase,
    RcInfoArray[1].Mmio64Size
    ));
}

STATIC
EFI_STATUS
KmhAcpiFindCompatibleReg (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  CONST CHAR8          *Compatible,
  IN  UINT64               ExpectedBase,
  OUT INT32                *MatchedNode,
  OUT UINT64               *RegBase,
  OUT UINT64               *RegSize
  )
{
  EFI_STATUS  Status;
  INT32       Node;

  if ((FdtClient == NULL) || (Compatible == NULL) || (RegBase == NULL) || (RegSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Status = FdtClient->FindCompatibleNode (FdtClient, Compatible, &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, Compatible, Node, &Node))
  {
    CONST UINT32  *Property;
    UINT32        PropertySize;
    UINT64        Base;
    UINT64        Size;

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Property, &PropertySize);
    if (EFI_ERROR (Status) || (PropertySize < 4 * sizeof (UINT32))) {
      continue;
    }

    Base = KmhAcpiFdtReadCells (Property, 2);
    Size = KmhAcpiFdtReadCells (&Property[2], 2);
    if ((ExpectedBase != MAX_UINT64) && (Base != ExpectedBase)) {
      continue;
    }

    if (MatchedNode != NULL) {
      *MatchedNode = Node;
    }

    *RegBase = Base;
    *RegSize = Size;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

STATIC
UINT32
KmhAcpiGetU32Property (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  INT32                Node,
  IN  CONST CHAR8          *PropertyName,
  IN  UINT32               Fallback
  )
{
  EFI_STATUS    Status;
  CONST UINT32  *Property;
  UINT32        PropertySize;

  if ((FdtClient == NULL) || (Node < 0) || (PropertyName == NULL)) {
    return Fallback;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, PropertyName, (CONST VOID **)&Property, &PropertySize);
  if (EFI_ERROR (Status) || (PropertySize < sizeof (UINT32))) {
    return Fallback;
  }

  return SwapBytes32 (Property[0]);
}

STATIC
VOID
KmhAcpiLogDtStringProperty (
  IN FDT_CLIENT_PROTOCOL  *FdtClient,
  IN INT32                Node,
  IN CONST CHAR8          *PropertyName,
  IN CONST CHAR8          *LogName
  )
{
  EFI_STATUS  Status;
  CONST VOID  *Property;
  UINT32      PropertySize;
  CHAR8       Preview[97];
  UINTN       CopySize;

  if ((FdtClient == NULL) || (PropertyName == NULL) || (LogName == NULL)) {
    return;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, PropertyName, &Property, &PropertySize);
  if (EFI_ERROR (Status) || (Property == NULL) || (PropertySize == 0)) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: %a unavailable node=%d status=%r\n",
      LogName,
      Node,
      Status
      ));
    return;
  }

  CopySize = PropertySize;
  if (CopySize >= sizeof (Preview)) {
    CopySize = sizeof (Preview) - 1;
  }

  CopyMem (Preview, Property, CopySize);
  Preview[CopySize] = '\0';

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: %a node=%d len=%u prefix=\"%a\"\n",
    LogName,
    Node,
    PropertySize,
    Preview
    ));
}

STATIC
BOOLEAN
KmhAcpiLogOptionalCompatibleReg (
  IN FDT_CLIENT_PROTOCOL  *FdtClient,
  IN CONST CHAR8          *Compatible,
  IN CONST CHAR8          *AcpiName,
  OUT INT32               *FoundNode OPTIONAL,
  OUT UINT64              *FoundBase OPTIONAL,
  OUT UINT64              *FoundSize OPTIONAL
  )
{
  EFI_STATUS  Status;
  INT32       Node;
  UINT64      RegBase;
  UINT64      RegSize;

  Status = KmhAcpiFindCompatibleReg (FdtClient, Compatible, MAX_UINT64, &Node, &RegBase, &RegSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: optional %a has no enabled DT compatible \"%a\"\n",
      AcpiName,
      Compatible
      ));
    return FALSE;
  }

  if (FoundNode != NULL) {
    *FoundNode = Node;
  }

  if (FoundBase != NULL) {
    *FoundBase = RegBase;
  }

  if (FoundSize != NULL) {
    *FoundSize = RegSize;
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: optional %a enabled DT node=%d compatible=\"%a\" base=0x%Lx size=0x%Lx\n",
    AcpiName,
    Node,
    Compatible,
    RegBase,
    RegSize
    ));
  return TRUE;
}

STATIC
VOID
KmhAcpiInstallOptionalDeviceGuarded (
  IN CONST CHAR8  *AcpiName,
  IN CONST CHAR8  *Compatible,
  IN INT32        Node,
  IN UINT64       RegBase,
  IN UINT64       RegSize,
  IN BOOLEAN      InstallEnabled
  )
{
  if ((AcpiName == NULL) || (Compatible == NULL)) {
    return;
  }

  if (!InstallEnabled) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: optional %a DT node=%d compatible=\"%a\" base=0x%Lx size=0x%Lx ACPI install skipped by policy\n",
      AcpiName,
      Node,
      Compatible,
      RegBase,
      RegSize
      ));
    return;
  }

  DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: optional %a DT node=%d compatible=\"%a\" ACPI install enabled but installer is not implemented yet\n",
    AcpiName,
    Node,
    Compatible
    ));
}

STATIC
VOID
KmhAcpiLogCurrentAcpiPolicy (
  IN UINTN  PcieRcCount,
  IN BOOLEAN Sdh0Present,
  IN BOOLEAN Xdm0Present,
  IN UINTN  VirtioMmioCount
  )
{
  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: policy dynamic RHCT=1 MADT=1 MCFG=1 MCFG-multi=1 ECAM-SSDT=%u DSDT-PCI-root-bridges=2 RC1-init=0\n",
    KMH_ENABLE_ECAM_RESERVATION_SSDT
    ));
  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: policy optional devices SDH0-dt=%u SDH0-install=%u XDM0-dt=%u XDM0-install=%u virtio-mmio-dt-count=%u virtio-mmio-install=%u\n",
    Sdh0Present,
    KMH_ENABLE_OPTIONAL_SDH0_ACPI,
    Xdm0Present,
    KMH_ENABLE_OPTIONAL_XDM0_ACPI,
    (UINT32)VirtioMmioCount,
    KMH_ENABLE_OPTIONAL_VIRTIO_MMIO_ACPI
    ));

  if (PcieRcCount > 1) {
    DEBUG ((DEBUG_WARN, "KMH-DT-PCIE: RC1+ present in runtime DT; DSDT exposes PCI1 and RC1 init policy=0\n"));
  }
}

STATIC
VOID
KmhAcpiLogCompatibleRegInventory (
  IN FDT_CLIENT_PROTOCOL  *FdtClient,
  IN CONST CHAR8          *Compatible,
  IN CONST CHAR8          *LogName
  )
{
  EFI_STATUS    Status;
  INT32         Node;
  CONST UINT32  *Property;
  UINT32        PropertySize;
  UINT64        RegBase;
  UINT64        RegSize;
  UINTN         Count;

  if ((FdtClient == NULL) || (Compatible == NULL) || (LogName == NULL)) {
    return;
  }

  Count = 0;
  for (Status = FdtClient->FindCompatibleNode (FdtClient, Compatible, &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, Compatible, Node, &Node))
  {
    Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Property, &PropertySize);
    if (EFI_ERROR (Status) || (PropertySize < 4 * sizeof (UINT32))) {
      DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: inventory %a node=%d compatible=\"%a\" has no valid reg: %r\n",
        LogName,
        Node,
        Compatible,
        Status
        ));
      continue;
    }

    RegBase = KmhAcpiFdtReadCells (Property, 2);
    RegSize = KmhAcpiFdtReadCells (&Property[2], 2);
    Count++;
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: inventory %a node=%d compatible=\"%a\" base=0x%Lx size=0x%Lx\n",
      LogName,
      Node,
      Compatible,
      RegBase,
      RegSize
      ));
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: inventory %a compatible=\"%a\" enabled-count=%u\n",
    LogName,
    Compatible,
    (UINT32)Count
    ));
}

STATIC
UINTN
KmhAcpiLogPcieDetailedInventory (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS           Status;
  KMH_DT_PCIE_RC_INFO  RcInfoArray[KMH_PCIE_MAX_RC_COUNT];
  KMH_DT_PCIE_RC_INFO  *RcInfo;
  UINTN                Count;
  UINTN                Index;

  if (FdtClient == NULL) {
    return 0;
  }

  Status = KmhAcpiCollectPcieRcInfoFromDt (FdtClient, RcInfoArray, KMH_PCIE_MAX_RC_COUNT, &Count);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-PCIE: detailed inventory collect failed: %r\n", Status));
    return 0;
  }

  for (Index = 0; Index < Count; Index++) {
    RcInfo = &RcInfoArray[Index];
    DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: inventory RC%u node=%d dbi=0x%Lx/0x%Lx config=0x%Lx/0x%Lx bus=%u-%u bus-range=%u mcfg-candidate=0x%Lx\n",
      (UINT32)Index,
      RcInfo->Node,
      RcInfo->DbiBase,
      RcInfo->DbiSize,
      RcInfo->ConfigBase,
      RcInfo->ConfigSize,
      RcInfo->BusMin,
      RcInfo->BusMax,
      RcInfo->FoundBusRange,
      RcInfo->FoundMmio32 ? RcInfo->McfgBase : 0
      ));
    DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: inventory RC%u MEM32 found=%u pci=0x%Lx cpu=0x%Lx size=0x%Lx trans=0x%Lx\n",
      (UINT32)Index,
      RcInfo->FoundMmio32,
      RcInfo->Mmio32PciBase,
      RcInfo->Mmio32CpuBase,
      RcInfo->Mmio32Size,
      RcInfo->FoundMmio32 ? (RcInfo->Mmio32CpuBase - RcInfo->Mmio32PciBase) : 0
      ));
    DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: inventory RC%u MEM64 found=%u pci=0x%Lx cpu=0x%Lx size=0x%Lx trans=0x%Lx\n",
      (UINT32)Index,
      RcInfo->FoundMmio64,
      RcInfo->Mmio64PciBase,
      RcInfo->Mmio64CpuBase,
      RcInfo->Mmio64Size,
      RcInfo->FoundMmio64 ? (RcInfo->Mmio64CpuBase - RcInfo->Mmio64PciBase) : 0
      ));
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: detailed enabled RC count=%u\n", (UINT32)Count));
  return Count;
}

STATIC
VOID
KmhAcpiLogDtU32ArrayProperty (
  IN FDT_CLIENT_PROTOCOL  *FdtClient,
  IN INT32                Node,
  IN CONST CHAR8          *PropertyName,
  IN CONST CHAR8          *LogName
  )
{
  EFI_STATUS    Status;
  CONST UINT32  *Property;
  UINT32        PropertySize;
  UINTN         CellCount;
  UINTN         PrintCount;
  UINTN         Index;

  if ((FdtClient == NULL) || (Node < 0) || (PropertyName == NULL) || (LogName == NULL)) {
    return;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, PropertyName, (CONST VOID **)&Property, &PropertySize);
  if (EFI_ERROR (Status) || (Property == NULL) || (PropertySize == 0)) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: %a node=%d property=%a unavailable status=%r\n",
      LogName,
      Node,
      PropertyName,
      Status
      ));
    return;
  }

  if ((PropertySize % sizeof (UINT32)) != 0) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: %a node=%d property=%a non-u32 bytes=%u\n",
      LogName,
      Node,
      PropertyName,
      PropertySize
      ));
    return;
  }

  CellCount = PropertySize / sizeof (UINT32);
  PrintCount = CellCount;
  if (PrintCount > 8) {
    PrintCount = 8;
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: %a node=%d property=%a cells=%u",
    LogName,
    Node,
    PropertyName,
    (UINT32)CellCount
    ));
  for (Index = 0; Index < PrintCount; Index++) {
    DEBUG ((DEBUG_INFO, " [%u]=0x%x",
      (UINT32)Index,
      SwapBytes32 (Property[Index])
      ));
  }
  if (PrintCount < CellCount) {
    DEBUG ((DEBUG_INFO, " ..."));
  }
  DEBUG ((DEBUG_INFO, "\n"));
}

STATIC
VOID
KmhAcpiValidatePlatformMetadataFromDt (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS    Status;
  INT32         ChosenNode;
  INT32         MemoryNode;
  INT32         PrevMemoryNode;
  CONST VOID    *RegProperty;
  UINTN         AddressCells;
  UINTN         SizeCells;
  UINT32        RegSize;
  UINTN         CellCount;
  UINTN         EntryCells;
  UINTN         Offset;
  UINTN         MemoryRangeCount;
  UINT64        TotalMemory;
  CONST UINT32  *Reg;
  UINT64        Base;
  UINT64        Size;

  if (FdtClient == NULL) {
    return;
  }

  KmhAcpiLogDtStringProperty (FdtClient, 0, "model", "root model");
  KmhAcpiLogDtStringProperty (FdtClient, 0, "compatible", "root compatible");

  Status = FdtClient->GetOrInsertChosenNode (FdtClient, &ChosenNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: chosen node unavailable: %r\n", Status));
  } else {
    KmhAcpiLogDtStringProperty (FdtClient, ChosenNode, "stdout-path", "chosen stdout-path");
    KmhAcpiLogDtStringProperty (FdtClient, ChosenNode, "bootargs", "chosen bootargs");
  }

  MemoryRangeCount = 0;
  TotalMemory      = 0;
  PrevMemoryNode   = 0;
  for (Status = FdtClient->FindMemoryNodeReg (
                         FdtClient,
                         &MemoryNode,
                         &RegProperty,
                         &AddressCells,
                         &SizeCells,
                         &RegSize
                         );
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextMemoryNodeReg (
                            FdtClient,
                            PrevMemoryNode,
                            &MemoryNode,
                            &RegProperty,
                            &AddressCells,
                            &SizeCells,
                            &RegSize
                            ))
  {
    CellCount = RegSize / sizeof (UINT32);
    EntryCells = AddressCells + SizeCells;
    if ((RegProperty == NULL) || (EntryCells == 0) || ((RegSize % sizeof (UINT32)) != 0) || ((CellCount % EntryCells) != 0)) {
      DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: memory node=%d has invalid reg cells addr=%u size=%u bytes=%u\n",
        MemoryNode,
        (UINT32)AddressCells,
        (UINT32)SizeCells,
        RegSize
        ));
      continue;
    }

    Reg = (CONST UINT32 *)RegProperty;
    for (Offset = 0; Offset < CellCount; Offset += EntryCells) {
      Base = KmhAcpiFdtReadCells (&Reg[Offset], AddressCells);
      Size = KmhAcpiFdtReadCells (&Reg[Offset + AddressCells], SizeCells);
      MemoryRangeCount++;
      TotalMemory += Size;
      DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: memory node=%d range=%u base=0x%Lx size=0x%Lx end=0x%Lx\n",
        MemoryNode,
        (UINT32)MemoryRangeCount,
        Base,
        Size,
        Base + Size - 1
        ));
    }

    PrevMemoryNode = MemoryNode;
  }

  DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: enabled memory ranges=%u total-size=0x%Lx\n",
    (UINT32)MemoryRangeCount,
    TotalMemory
    ));
}

STATIC
BOOLEAN
KmhAcpiFdtStringEquals (
  IN CONST VOID  *Property,
  IN UINT32      PropertySize,
  IN CONST CHAR8 *Expected
  );

STATIC
EFI_STATUS
KmhAcpiFindCpuNodeByHartId (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  UINT64               HartId,
  OUT INT32                *CpuNode
  )
{
  EFI_STATUS    Status;
  INT32         Node;
  CONST VOID    *Property;
  UINT32        PropertySize;
  CONST UINT32  *Reg;
  UINT64        NodeHartId;

  if ((FdtClient == NULL) || (CpuNode == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Status = FdtClient->FindCompatibleNode (FdtClient, "riscv", &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, "riscv", Node, &Node))
  {
    Status = FdtClient->GetNodeProperty (FdtClient, Node, "device_type", &Property, &PropertySize);
    if (EFI_ERROR (Status) || !KmhAcpiFdtStringEquals (Property, PropertySize, "cpu")) {
      continue;
    }

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Reg, &PropertySize);
    if (EFI_ERROR (Status) || (PropertySize == 0) || ((PropertySize % sizeof (UINT32)) != 0)) {
      continue;
    }

    NodeHartId = KmhAcpiFdtReadCells (Reg, PropertySize / sizeof (UINT32));
    if (NodeHartId == HartId) {
      *CpuNode = Node;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

STATIC
UINT16
KmhAcpiGetDtStringPropertySize (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  INT32                Node,
  IN  CONST CHAR8          *PropertyName,
  OUT CONST UINT8          **Property
  )
{
  EFI_STATUS  Status;
  CONST VOID  *DtProperty;
  UINT32      PropertySize;

  if (Property != NULL) {
    *Property = NULL;
  }

  if ((FdtClient == NULL) || (Node < 0) || (PropertyName == NULL)) {
    return 0;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, PropertyName, &DtProperty, &PropertySize);
  if (EFI_ERROR (Status) || (DtProperty == NULL) || (PropertySize == 0)) {
    return 0;
  }

  if (Property != NULL) {
    *Property = DtProperty;
  }

  if (PropertySize > MAX_UINT16) {
    return MAX_UINT16;
  }

  return (UINT16)PropertySize;
}

STATIC
UINT16
KmhAcpiCopyDtStringProperty (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  IN  INT32                Node,
  IN  CONST CHAR8          *PropertyName,
  IN  CONST CHAR8          *Fallback,
  OUT UINT8                *Buffer,
  IN  UINTN                BufferSize
  )
{
  EFI_STATUS  Status;
  CONST VOID  *Property;
  UINT32      PropertySize;
  UINTN       CopySize;

  if ((Buffer == NULL) || (BufferSize == 0)) {
    return 0;
  }

  ZeroMem (Buffer, BufferSize);
  Status = EFI_NOT_FOUND;
  Property = NULL;
  PropertySize = 0;
  if ((FdtClient != NULL) && (Node >= 0) && (PropertyName != NULL)) {
    Status = FdtClient->GetNodeProperty (FdtClient, Node, PropertyName, &Property, &PropertySize);
  }

  if (EFI_ERROR (Status) || (Property == NULL) || (PropertySize == 0)) {
    Property = Fallback;
    PropertySize = (Fallback == NULL) ? 0 : (UINT32)(AsciiStrLen (Fallback) + 1);
  }

  CopySize = PropertySize;
  if (CopySize > BufferSize) {
    CopySize = BufferSize;
  }

  if (CopySize > 0) {
    CopyMem (Buffer, Property, CopySize);
    Buffer[BufferSize - 1] = 0;
  }

  if (CopySize > MAX_UINT16) {
    return MAX_UINT16;
  }

  return (UINT16)CopySize;
}

STATIC
BOOLEAN
KmhAcpiRhctDtIsaHasRequiredTokens (
  IN CONST UINT8  *IsaString
  )
{
  CONST CHAR8  *Isa;

  if (IsaString == NULL) {
    return FALSE;
  }

  Isa = (CONST CHAR8 *)IsaString;

  return (AsciiStrStr (Isa, "zicsr") != NULL) &&
         (AsciiStrStr (Isa, "zifencei") != NULL) &&
         (AsciiStrStr (Isa, "zicbom") != NULL) &&
         (AsciiStrStr (Isa, "zicbop") != NULL) &&
         (AsciiStrStr (Isa, "zicboz") != NULL);
}

STATIC
UINT64
KmhAcpiGetTimebaseFrequencyFromDt (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  (VOID)FdtClient;

  return KMH_RHCT_BASE_FREQUENCY;
}

STATIC
UINT8
KmhAcpiGetRhctMmuTypeFromDt (
  IN FDT_CLIENT_PROTOCOL  *FdtClient,
  IN INT32                CpuNode
  )
{
  EFI_STATUS  Status;
  CONST VOID  *Property;
  UINT32      PropertySize;

  if ((FdtClient == NULL) || (CpuNode < 0)) {
    return KMH_RHCT_MMU_TYPE_SV48;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, CpuNode, "mmu-type", &Property, &PropertySize);
  if (!EFI_ERROR (Status) && KmhAcpiFdtStringEquals (Property, PropertySize, "riscv,sv48")) {
    return KMH_RHCT_MMU_TYPE_SV48;
  }

  return KMH_RHCT_MMU_TYPE_SV48;
}

STATIC
UINT8
KmhAcpiRhctCmoBlockSizeToExponent (
  IN UINT32      BlockSize,
  IN UINT8       FallbackExponent,
  IN CONST CHAR8 *Name
  )
{
  UINT8   Exponent;
  UINT32  Value;

  if ((BlockSize == 0) || ((BlockSize & (BlockSize - 1)) != 0)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-RHCT: invalid %a block-size=%u, use exponent=%u\n",
      Name,
      BlockSize,
      FallbackExponent
      ));
    return FallbackExponent;
  }

  Exponent = 0;
  Value    = BlockSize;
  while (Value > 1) {
    Value >>= 1;
    Exponent++;
  }

  if (Exponent > 30) {
    DEBUG ((DEBUG_WARN, "KMH-DT-RHCT: %a block-size=%u exponent=%u exceeds Linux RHCT limit, use exponent=%u\n",
      Name,
      BlockSize,
      Exponent,
      FallbackExponent
      ));
    return FallbackExponent;
  }

  return Exponent;
}

STATIC
BOOLEAN
KmhAcpiFdtStringEquals (
  IN CONST VOID  *Property,
  IN UINT32      PropertySize,
  IN CONST CHAR8 *Expected
  )
{
  UINTN  ExpectedSize;

  if ((Property == NULL) || (Expected == NULL)) {
    return FALSE;
  }

  ExpectedSize = AsciiStrLen (Expected) + 1;
  if (PropertySize < ExpectedSize) {
    return FALSE;
  }

  return CompareMem (Property, Expected, ExpectedSize) == 0;
}

STATIC
EFI_STATUS
KmhAcpiGetCpuHartIdsFromDt (
  IN  FDT_CLIENT_PROTOCOL  *FdtClient,
  OUT UINT64               *HartIds,
  IN  UINTN                MaxHartIds,
  OUT UINTN                *CpuCount
  )
{
  EFI_STATUS    Status;
  INT32         Node;
  CONST VOID    *Property;
  UINT32        PropertySize;
  CONST UINT32  *Reg;

  if ((FdtClient == NULL) || (HartIds == NULL) || (CpuCount == NULL) || (MaxHartIds == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  *CpuCount = 0;
  for (Status = FdtClient->FindCompatibleNode (FdtClient, "riscv", &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, "riscv", Node, &Node))
  {
    Status = FdtClient->GetNodeProperty (FdtClient, Node, "device_type", &Property, &PropertySize);
    if (EFI_ERROR (Status) || !KmhAcpiFdtStringEquals (Property, PropertySize, "cpu")) {
      continue;
    }

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Reg, &PropertySize);
    if (EFI_ERROR (Status) || (PropertySize == 0) || ((PropertySize % sizeof (UINT32)) != 0)) {
      DEBUG ((DEBUG_WARN, "KMH-DT-MADT: CPU node=%d has no valid reg property: %r\n", Node, Status));
      continue;
    }

    if (*CpuCount >= MaxHartIds) {
      DEBUG ((DEBUG_ERROR, "KMH-DT-MADT: CPU count exceeds local limit %u\n", (UINT32)MaxHartIds));
      return EFI_BUFFER_TOO_SMALL;
    }

    HartIds[*CpuCount] = KmhAcpiFdtReadCells (Reg, PropertySize / sizeof (UINT32));
    DEBUG ((DEBUG_INFO, "KMH-DT-MADT: CPU node=%d hart=0x%Lx\n", Node, HartIds[*CpuCount]));
    (*CpuCount)++;
  }

  if (*CpuCount == 0) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
KmhAcpiInstallMadtFromDt (
  VOID
  )
{
  EFI_STATUS                                                  Status;
  FDT_CLIENT_PROTOCOL                                         *FdtClient;
  EFI_ACPI_TABLE_PROTOCOL                                     *AcpiTable;
  EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER         *Madt;
  KMH_ACPI_MADT_INTC_STRUCTURE                                *Intc;
  KMH_ACPI_MADT_IMSIC_STRUCTURE                               *Imsic;
  KMH_ACPI_MADT_APLIC_STRUCTURE                               *Aplic;
  EFI_PHYSICAL_ADDRESS                                        PageAddress;
  UINT64                                                      HartIds[KMH_MADT_MAX_CPU_COUNT];
  UINTN                                                       CpuCount;
  UINTN                                                       Index;
  UINTN                                                       TableSize;
  UINTN                                                       TableKey;
  INT32                                                       AplicNode;
  INT32                                                       ImsicNode;
  UINT64                                                      RegBase;
  UINT64                                                      RegSize;
  UINT32                                                      AplicNumSources;
  UINT32                                                      ImsicNumIds;
  UINT32                                                      ImsicNumGuestIds;
  UINT32                                                      ImsicGuestIndexBits;
  UINT32                                                      ImsicHartIndexBits;
  UINT32                                                      ImsicGroupIndexBits;
  UINT32                                                      ImsicGroupIndexShift;
  UINT64                                                      AplicBase;
  UINT32                                                      AplicSize;
  UINT64                                                      ImsicBase;
  UINT32                                                      ImsicSize;
  UINT32                                                      ImsicPerHartSize;

  AplicNumSources     = KMH_APLIC_S_NUM_SOURCES;
  ImsicNumIds         = KMH_IMSIC_S_NUM_IDS;
  ImsicNumGuestIds    = KMH_IMSIC_S_NUM_GUEST_IDS;
  ImsicGuestIndexBits = KMH_IMSIC_S_GUEST_INDEX_BITS;
  ImsicHartIndexBits  = KMH_IMSIC_S_HART_INDEX_BITS;
  ImsicGroupIndexBits = KMH_IMSIC_S_GROUP_INDEX_BITS;
  ImsicGroupIndexShift = KMH_IMSIC_S_GROUP_INDEX_SHIFT;
  AplicBase           = KMH_APLIC_S_BASE;
  AplicSize           = KMH_APLIC_S_SIZE;
  ImsicBase           = KMH_IMSIC_S_BASE;
  ImsicSize           = KMH_IMSIC_S_SIZE;
  ImsicPerHartSize    = KMH_IMSIC_S_PER_HART_SIZE;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MADT: FDT client not available: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MADT: ACPI table protocol not available: %r\n", Status));
    return Status;
  }

  Status = KmhAcpiGetCpuHartIdsFromDt (FdtClient, HartIds, KMH_MADT_MAX_CPU_COUNT, &CpuCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MADT: failed to get CPU hart IDs from DT: %r\n", Status));
    return Status;
  }

  Status = KmhAcpiFindCompatibleReg (FdtClient, "riscv,aplic", KMH_APLIC_S_BASE, &AplicNode, &RegBase, &RegSize);
  if (!EFI_ERROR (Status)) {
    AplicBase       = RegBase;
    AplicSize       = (RegSize > MAX_UINT32) ? MAX_UINT32 : (UINT32)RegSize;
    if (RegSize > MAX_UINT32) {
      DEBUG ((DEBUG_WARN, "KMH-DT-MADT: APLIC size=0x%Lx exceeds UINT32, clamp to 0x%x\n",
        RegSize,
        AplicSize
        ));
    }
    AplicNumSources = KmhAcpiGetU32Property (FdtClient, AplicNode, "riscv,num-sources", KMH_APLIC_S_NUM_SOURCES);
    DEBUG ((DEBUG_INFO, "KMH-DT-MADT: APLIC from DT node=%d base=0x%Lx size=0x%Lx sources=%u\n",
      AplicNode,
      RegBase,
      RegSize,
      AplicNumSources
      ));
  } else {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: use fallback APLIC sources=%u: %r\n",
      AplicNumSources,
      Status
      ));
  }

  Status = KmhAcpiFindCompatibleReg (FdtClient, "riscv,imsics", KMH_IMSIC_S_BASE, &ImsicNode, &RegBase, &RegSize);
  if (!EFI_ERROR (Status)) {
    ImsicBase = RegBase;
    ImsicSize = (RegSize > MAX_UINT32) ? MAX_UINT32 : (UINT32)RegSize;
    if (RegSize > MAX_UINT32) {
      DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC size=0x%Lx exceeds UINT32, clamp to 0x%x\n",
        RegSize,
        ImsicSize
        ));
    }
    ImsicNumIds = KmhAcpiGetU32Property (FdtClient, ImsicNode, "riscv,num-ids", KMH_IMSIC_S_NUM_IDS);
    ImsicNumGuestIds = KmhAcpiGetU32Property (FdtClient, ImsicNode, "riscv,num-guest-ids", KMH_IMSIC_S_NUM_GUEST_IDS);
    ImsicGuestIndexBits = KmhAcpiGetU32Property (FdtClient, ImsicNode, "riscv,guest-index-bits", KMH_IMSIC_S_GUEST_INDEX_BITS);
    ImsicHartIndexBits = KmhAcpiGetU32Property (FdtClient, ImsicNode, "riscv,hart-index-bits", KMH_IMSIC_S_HART_INDEX_BITS);
    ImsicGroupIndexBits = KmhAcpiGetU32Property (FdtClient, ImsicNode, "riscv,group-index-bits", KMH_IMSIC_S_GROUP_INDEX_BITS);
    ImsicGroupIndexShift = KmhAcpiGetU32Property (FdtClient, ImsicNode, "riscv,group-index-shift", KMH_IMSIC_S_GROUP_INDEX_SHIFT);
    DEBUG ((DEBUG_INFO, "KMH-DT-MADT: IMSIC from DT node=%d base=0x%Lx size=0x%Lx ids=%u guest-ids=%u guest-bits=%u hart-bits=%u group-bits=%u group-shift=%u\n",
      ImsicNode,
      RegBase,
      RegSize,
      ImsicNumIds,
      ImsicNumGuestIds,
      ImsicGuestIndexBits,
      ImsicHartIndexBits,
      ImsicGroupIndexBits,
      ImsicGroupIndexShift
      ));
  } else {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: use fallback IMSIC ids=%u guest-ids=%u guest-bits=%u hart-bits=%u group-bits=%u group-shift=%u: %r\n",
      ImsicNumIds,
      ImsicNumGuestIds,
      ImsicGuestIndexBits,
      ImsicHartIndexBits,
      ImsicGroupIndexBits,
      ImsicGroupIndexShift,
      Status
      ));
  }

  if (ImsicNumIds > MAX_UINT16) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC num-ids=%u exceeds UINT16, clamp to %u\n",
      ImsicNumIds,
      (UINT32)MAX_UINT16
      ));
    ImsicNumIds = MAX_UINT16;
  }

  if (ImsicNumGuestIds > MAX_UINT16) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC num-guest-ids=%u exceeds UINT16, clamp to %u\n",
      ImsicNumGuestIds,
      (UINT32)MAX_UINT16
      ));
    ImsicNumGuestIds = MAX_UINT16;
  }

  if (ImsicGuestIndexBits > MAX_UINT8) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC guest-index-bits=%u exceeds UINT8, clamp to %u\n",
      ImsicGuestIndexBits,
      (UINT32)MAX_UINT8
      ));
    ImsicGuestIndexBits = MAX_UINT8;
  }

  if (ImsicHartIndexBits > MAX_UINT8) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC hart-index-bits=%u exceeds UINT8, clamp to %u\n",
      ImsicHartIndexBits,
      (UINT32)MAX_UINT8
      ));
    ImsicHartIndexBits = MAX_UINT8;
  }

  if (ImsicGroupIndexBits > MAX_UINT8) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC group-index-bits=%u exceeds UINT8, clamp to %u\n",
      ImsicGroupIndexBits,
      (UINT32)MAX_UINT8
      ));
    ImsicGroupIndexBits = MAX_UINT8;
  }

  if (ImsicGroupIndexShift > MAX_UINT8) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MADT: IMSIC group-index-shift=%u exceeds UINT8, clamp to %u\n",
      ImsicGroupIndexShift,
      (UINT32)MAX_UINT8
      ));
    ImsicGroupIndexShift = MAX_UINT8;
  }

  if (ImsicHartIndexBits < 31) {
    UINT32  HartWindowCount;

    HartWindowCount = 1U << ImsicHartIndexBits;
    if ((HartWindowCount != 0) && ((ImsicSize % HartWindowCount) == 0)) {
      ImsicPerHartSize = ImsicSize / HartWindowCount;
    } else {
      DEBUG ((DEBUG_WARN, "KMH-DT-MADT: keep fallback IMSIC per-hart size=0x%x total=0x%x windows=%u\n",
        ImsicPerHartSize,
        ImsicSize,
        HartWindowCount
        ));
    }
  }

  TableSize = sizeof (EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER) +
              (sizeof (KMH_ACPI_MADT_INTC_STRUCTURE) * CpuCount) +
              sizeof (KMH_ACPI_MADT_IMSIC_STRUCTURE) +
              sizeof (KMH_ACPI_MADT_APLIC_STRUCTURE);

  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiACPIReclaimMemory,
                  EFI_SIZE_TO_PAGES (TableSize),
                  &PageAddress
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MADT: failed to allocate MADT size=0x%Lx: %r\n", (UINT64)TableSize, Status));
    return Status;
  }

  Madt = (EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *)(UINTN)PageAddress;
  ZeroMem (Madt, TableSize);

  Madt->Header.Signature       = EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE;
  Madt->Header.Length          = (UINT32)TableSize;
  Madt->Header.Revision        = EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION;
  Madt->Header.Checksum        = 0;
  CopyMem (Madt->Header.OemId, mKmhAcpiOemId, sizeof (Madt->Header.OemId));
  Madt->Header.OemTableId      = EFI_ACPI_OEM_TABLE_ID;
  Madt->Header.OemRevision     = EFI_ACPI_OEM_REVISION;
  Madt->Header.CreatorId       = EFI_ACPI_CREATOR_ID;
  Madt->Header.CreatorRevision = EFI_ACPI_CREATOR_REVISION;
  Madt->LocalApicAddress       = 0;
  Madt->Flags                  = 0;

  Intc = (KMH_ACPI_MADT_INTC_STRUCTURE *)(Madt + 1);
  for (Index = 0; Index < CpuCount; Index++) {
    Intc[Index].Type       = 0x18;
    Intc[Index].Length     = sizeof (KMH_ACPI_MADT_INTC_STRUCTURE);
    Intc[Index].Version    = 1;
    Intc[Index].Reserved   = 0;
    Intc[Index].Flags      = 1;
    Intc[Index].HartId     = HartIds[Index];
    Intc[Index].Uid        = (UINT32)HartIds[Index];
    Intc[Index].ExtIntcId  = 0;
    Intc[Index].ImsicBase  = ImsicBase + (HartIds[Index] * KMH_IMSIC_S_STRIDE);
    Intc[Index].ImsicSize  = ImsicPerHartSize;
  }

  Imsic = (KMH_ACPI_MADT_IMSIC_STRUCTURE *)&Intc[CpuCount];
  Imsic->Type          = 0x19;
  Imsic->Length        = sizeof (KMH_ACPI_MADT_IMSIC_STRUCTURE);
  Imsic->Version       = 1;
  Imsic->AplicId       = 0;
  Imsic->Flags         = 0;
  Imsic->NumIds        = (UINT16)ImsicNumIds;
  Imsic->NumGuestIds   = (UINT16)ImsicNumGuestIds;
  Imsic->GuestIdxBits  = (UINT8)ImsicGuestIndexBits;
  Imsic->HartIdxBits   = (UINT8)ImsicHartIndexBits;
  Imsic->GroupIdxBits  = (UINT8)ImsicGroupIndexBits;
  Imsic->GroupIdxShift = (UINT8)ImsicGroupIndexShift;

  Aplic = (KMH_ACPI_MADT_APLIC_STRUCTURE *)(Imsic + 1);
  Aplic->Type    = 0x1A;
  Aplic->Length  = sizeof (KMH_ACPI_MADT_APLIC_STRUCTURE);
  Aplic->Version = 1;
  Aplic->Id      = 0;
  Aplic->Flags   = 0;
  Aplic->NumIdcs = 0;
  Aplic->NumSrcs = AplicNumSources;
  Aplic->GsiBase = 0;
  Aplic->Base    = AplicBase;
  Aplic->Size    = AplicSize;

  TableKey = 0;
  Status = AcpiTable->InstallAcpiTable (AcpiTable, Madt, TableSize, &TableKey);
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_INFO,
    "KMH-DT-MADT: install dynamic MADT cpu-count=%u size=0x%Lx aplic=0x%Lx/0x%x imsic=0x%Lx/0x%x per-hart=0x%x status=%r key=0x%Lx\n",
    (UINT32)CpuCount,
    (UINT64)TableSize,
    AplicBase,
    AplicSize,
    ImsicBase,
    ImsicSize,
    ImsicPerHartSize,
    Status,
    (UINT64)TableKey
    ));

  return Status;
}

STATIC
VOID
KmhAcpiInitRhctNode (
  IN  FDT_CLIENT_PROTOCOL      *FdtClient,
  OUT KMH_ACPI_RHCT_NODE_DATA  *Node,
  IN  UINT32                   NodeBase,
  IN  UINT32                   HartUid,
  IN  UINT64                   HartId
  )
{
  STATIC CONST CHAR8  RhctIsaString[] =
    "rv64imafdcvh_sdring_sha_shcounternw_shgatpa_shlcofideleg_shtvala_shvsatpa_shvstvala_shvstvecd_smaia_smcsrind_smdbltrp_smmpm_smnpm_smrnmi_smstateen_sspl13_ssaia_ssccptr_ssconfpmf_sscounterenw_sscsrind_ssdbltrp_ssnpm_sspm_ssstateen_ssstrict_sstc_sstvala_sstvecd_ssu64xl_supm_svade_svbare_svinval_svnapot_za64rs_zacas_zawrs_zba_zbb_zbc_zbkb_zbkc_zbkx_zbs_zcb_zcmop_zfa_zfh_zfhmin_zic64b_zicbom_zicbop_zicboz_ziccamoa_ziccif_zicclsm_ziccrse_zicntr_zicond_zicsr_zifencei_zihintntl_zihintpause_zihpm_zimop_zkn_zknd_zkne_zknh_zksed_zksh_zkt_zvbb_zvfn_zvfhmin_zvkt_zv1128b_zv132b_zv164b";

  UINT32  IsaOffset;
  UINT32  CmoOffset;
  UINT32  MmuOffset;
  INT32   CpuNode;
  UINT8   ProbeIsaString[KMH_RHCT_ISA_STRING_LENGTH];
  CONST UINT8 *ProbeIsaDtString;
  UINT16  ProbeIsaLength;
  UINT16  ProbeIsaDtLength;
  BOOLEAN UseDtIsaString;
  UINT32  CbomBlockSize;
  UINT32  CbopBlockSize;
  UINT32  CbozBlockSize;
  UINT8   CbomExponent;
  UINT8   CbopExponent;
  UINT8   CbozExponent;
  UINT8   MmuType;

  ZeroMem (Node, sizeof (*Node));

  IsaOffset = NodeBase + (UINT32)sizeof (KMH_ACPI_RHCT_HART_INFO_STRUCT) + (UINT32)sizeof (KMH_ACPI_RHCT_HART_NODE_INFO_OFFSET);
  CmoOffset = IsaOffset + (UINT32)sizeof (KMH_ACPI_RHCT_ISA_STRING_STRUCT);
  MmuOffset = CmoOffset + (UINT32)sizeof (KMH_ACPI_RHCT_CMO_STRUCT);

  Node->HartInfo.Header.Type     = KMH_RHCT_HART_INFO_TYPE;
  Node->HartInfo.Header.Length   = (UINT16)(sizeof (KMH_ACPI_RHCT_HART_INFO_STRUCT) +
                                            sizeof (KMH_ACPI_RHCT_HART_NODE_INFO_OFFSET) +
                                            sizeof (KMH_ACPI_RHCT_ISA_STRING_STRUCT));
  Node->HartInfo.Header.Revision = KMH_RHCT_NODE_REVISION;
  Node->HartInfo.HartInfo.NumOffsets = (UINT16)(sizeof (Node->Offset.Offset) / sizeof (Node->Offset.Offset[0]));
  Node->HartInfo.HartInfo.Uid    = HartUid;
  Node->Offset.Offset[0]         = IsaOffset;
  Node->Offset.Offset[1]         = CmoOffset;
  Node->Offset.Offset[2]         = MmuOffset;

  Node->IsaNode.Header.Type      = KMH_RHCT_ISA_STRING_TYPE;
  Node->IsaNode.Header.Length    = (UINT16)(sizeof (KMH_ACPI_RHCT_NODE_HEADER) + sizeof (KMH_ACPI_RHCT_ISA_STRING));
  Node->IsaNode.Header.Revision  = KMH_RHCT_NODE_REVISION;
  CpuNode = -1;
  if (EFI_ERROR (KmhAcpiFindCpuNodeByHartId (FdtClient, HartId, &CpuNode))) {
    DEBUG ((DEBUG_WARN, "KMH-DT-RHCT: CPU hart=0x%Lx has no matching DT node, use RHCT fallback properties\n", HartId));
  }

  ProbeIsaDtString = NULL;
  ProbeIsaDtLength = KmhAcpiGetDtStringPropertySize (
    FdtClient,
    CpuNode,
    "riscv,isa",
    &ProbeIsaDtString
    );
  ProbeIsaLength = KmhAcpiCopyDtStringProperty (
    FdtClient,
    CpuNode,
    "riscv,isa",
    RhctIsaString,
    ProbeIsaString,
    sizeof (ProbeIsaString)
    );
  UseDtIsaString = KmhAcpiRhctDtIsaHasRequiredTokens (ProbeIsaString);

  if (UseDtIsaString) {
    CopyMem (
      Node->IsaNode.IsaData.IsaString,
      ProbeIsaString,
      ProbeIsaLength
      );
    Node->IsaNode.IsaData.IsaLength = ProbeIsaLength;
  } else {
    Node->IsaNode.IsaData.IsaLength = (UINT16)(AsciiStrLen (RhctIsaString) + 1);
    CopyMem (
      Node->IsaNode.IsaData.IsaString,
      RhctIsaString,
      Node->IsaNode.IsaData.IsaLength
      );
  }
  DEBUG ((DEBUG_INFO, "KMH-DT-RHCT: CPU hart=0x%Lx node=%d isa-source=%a isa-len=%u raw-dt-isa-len=%u prepared-isa-len=%u\n",
    HartId,
    CpuNode,
    UseDtIsaString ? "dt" : "fallback",
    Node->IsaNode.IsaData.IsaLength,
    ProbeIsaDtLength,
    ProbeIsaLength
    ));

  CbomBlockSize = KmhAcpiGetU32Property (FdtClient, CpuNode, "riscv,cbom-block-size", 64);
  CbopBlockSize = KmhAcpiGetU32Property (FdtClient, CpuNode, "riscv,cbop-block-size", CbomBlockSize);
  if (CbopBlockSize == 0) {
    CbopBlockSize = CbomBlockSize;
  }
  CbozBlockSize = KmhAcpiGetU32Property (FdtClient, CpuNode, "riscv,cboz-block-size", 64);
  CbomExponent  = KmhAcpiRhctCmoBlockSizeToExponent (CbomBlockSize, 6, "cbom");
  CbopExponent  = KmhAcpiRhctCmoBlockSizeToExponent (CbopBlockSize, CbomExponent, "cbop");
  CbozExponent  = KmhAcpiRhctCmoBlockSizeToExponent (CbozBlockSize, 6, "cboz");

  MmuType = KmhAcpiGetRhctMmuTypeFromDt (FdtClient, CpuNode);

  Node->CmoNode.Header.Type      = KMH_RHCT_CMO_TYPE;
  Node->CmoNode.Header.Length    = (UINT16)sizeof (KMH_ACPI_RHCT_CMO_STRUCT);
  Node->CmoNode.Header.Revision  = KMH_RHCT_NODE_REVISION;
  Node->CmoNode.CmoData.Reserved = 0;
  Node->CmoNode.CmoData.CbomBlockSize = CbomExponent;
  Node->CmoNode.CmoData.CbopBlockSize = CbopExponent;
  Node->CmoNode.CmoData.CbozBlockSize = CbozExponent;

  Node->MmuNode.Header.Type      = KMH_RHCT_MMU_TYPE;
  Node->MmuNode.Header.Length    = (UINT16)sizeof (KMH_ACPI_RHCT_MMU_STRUCT);
  Node->MmuNode.Header.Revision  = KMH_RHCT_NODE_REVISION;
  Node->MmuNode.MmuData.Reserved = 0;
  Node->MmuNode.MmuData.MmuType  = MmuType;

  DEBUG ((DEBUG_INFO, "KMH-DT-RHCT: CPU hart=0x%Lx node=%d cbom=%u/e%u cbop=%u/e%u cboz=%u/e%u mmu=%u\n",
    HartId,
    CpuNode,
    CbomBlockSize,
    CbomExponent,
    CbopBlockSize,
    CbopExponent,
    CbozBlockSize,
    CbozExponent,
    MmuType
    ));
}

STATIC
EFI_STATUS
KmhAcpiInstallRhctFromDt (
  VOID
  )
{
  EFI_STATUS                       Status;
  FDT_CLIENT_PROTOCOL              *FdtClient;
  EFI_ACPI_TABLE_PROTOCOL          *AcpiTable;
  KMH_ACPI_RHCT_DESCRIPTION_TABLE  *Rhct;
  KMH_ACPI_RHCT_NODE_DATA          *Nodes;
  EFI_PHYSICAL_ADDRESS             PageAddress;
  UINT64                           HartIds[KMH_MADT_MAX_CPU_COUNT];
  UINTN                            CpuCount;
  UINTN                            Index;
  UINTN                            TableSize;
  UINTN                            TableKey;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-RHCT: FDT client not available: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-RHCT: ACPI table protocol not available: %r\n", Status));
    return Status;
  }

  Status = KmhAcpiGetCpuHartIdsFromDt (FdtClient, HartIds, KMH_MADT_MAX_CPU_COUNT, &CpuCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-RHCT: failed to get CPU hart IDs from DT: %r\n", Status));
    return Status;
  }

  TableSize = sizeof (KMH_ACPI_RHCT_DESCRIPTION_TABLE) +
              (sizeof (KMH_ACPI_RHCT_NODE_DATA) * CpuCount);

  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiACPIReclaimMemory,
                  EFI_SIZE_TO_PAGES (TableSize),
                  &PageAddress
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-RHCT: failed to allocate RHCT size=0x%Lx: %r\n", (UINT64)TableSize, Status));
    return Status;
  }

  Rhct = (KMH_ACPI_RHCT_DESCRIPTION_TABLE *)(UINTN)PageAddress;
  ZeroMem (Rhct, TableSize);

  Rhct->Header.Signature       = EFI_ACPI_RHCT_SIGNATURE;
  Rhct->Header.Length          = (UINT32)TableSize;
  Rhct->Header.Revision        = EFI_ACPI_RHCT_REVISION;
  Rhct->Header.Checksum        = 0;
  CopyMem (Rhct->Header.OemId, mKmhAcpiOemId, sizeof (Rhct->Header.OemId));
  Rhct->Header.OemTableId      = EFI_ACPI_OEM_TABLE_ID;
  Rhct->Header.OemRevision     = EFI_ACPI_OEM_REVISION;
  Rhct->Header.CreatorId       = EFI_ACPI_CREATOR_ID;
  Rhct->Header.CreatorRevision = EFI_ACPI_CREATOR_REVISION;
  Rhct->RhctHeader.Reserved    = 0;
  Rhct->RhctHeader.BaseFrequency = KmhAcpiGetTimebaseFrequencyFromDt (FdtClient);
  Rhct->RhctHeader.NodeNumber  = (UINT32)CpuCount;
  Rhct->RhctHeader.OffsetToNodeArray = (UINT32)sizeof (KMH_ACPI_RHCT_DESCRIPTION_TABLE);
  DEBUG ((DEBUG_INFO, "KMH-DT-RHCT: base-frequency=0x%Lx\n", Rhct->RhctHeader.BaseFrequency));

  Nodes = (KMH_ACPI_RHCT_NODE_DATA *)(Rhct + 1);
  for (Index = 0; Index < CpuCount; Index++) {
    KmhAcpiInitRhctNode (
      FdtClient,
      &Nodes[Index],
      sizeof (KMH_ACPI_RHCT_DESCRIPTION_TABLE) + (UINT32)(sizeof (KMH_ACPI_RHCT_NODE_DATA) * Index),
      (UINT32)HartIds[Index],
      HartIds[Index]
      );
    DEBUG ((DEBUG_INFO, "KMH-DT-RHCT: CPU hart=0x%Lx uid=%u\n", HartIds[Index], (UINT32)HartIds[Index]));
  }

  TableKey = 0;
  Status = AcpiTable->InstallAcpiTable (AcpiTable, Rhct, TableSize, &TableKey);
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_INFO,
    "KMH-DT-RHCT: install dynamic RHCT cpu-count=%u size=0x%Lx status=%r key=0x%Lx\n",
    (UINT32)CpuCount,
    (UINT64)TableSize,
    Status,
    (UINT64)TableKey
    ));

  return Status;
}

STATIC
VOID
KmhAcpiValidateStaticDevicesAgainstDt (
  VOID
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  INT32                Node;
  UINT64               RegBase;
  UINT64               RegSize;
  UINT32               ClockFrequency;
  UINT32               RegShift;
  UINT32               RegIoWidth;
  UINT32               CurrentSpeed;
  UINT32               NumSources;
  UINT32               NumIds;
  UINT32               GuestIndexBits;
  EFI_STATUS           FindStatus;
  UINTN                VirtioMmioCount;
  UINTN                PcieRcCount;
  BOOLEAN              Sdh0Present;
  BOOLEAN              Xdm0Present;
  INT32                Sdh0Node;
  INT32                Xdm0Node;
  UINT64               Sdh0Base;
  UINT64               Xdm0Base;
  UINT64               Sdh0Size;
  UINT64               Xdm0Size;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: skip static device validation because FDT client is unavailable: %r\n", Status));
    return;
  }

  KmhAcpiValidatePlatformMetadataFromDt (FdtClient);
  KmhAcpiLogCompatibleRegInventory (FdtClient, "riscv,aplic", "APLIC");
  KmhAcpiLogCompatibleRegInventory (FdtClient, "riscv,imsics", "IMSIC");
  KmhAcpiLogCompatibleRegInventory (FdtClient, "snps,dw-pcie", "PCIe");
  PcieRcCount = KmhAcpiLogPcieDetailedInventory (FdtClient);
  KmhAcpiValidatePci1CrsAgainstDt (FdtClient);
  if (PcieRcCount > 1) {
    DEBUG ((DEBUG_INFO, "KMH-DT-PCIE: runtime DT exposes %u enabled RCs; RC1 hardware init is guarded by DBI presence\n",
      (UINT32)PcieRcCount
      ));
  }

  Status = KmhAcpiFindCompatibleReg (FdtClient, "ns16550a", KMH_UART0_BASE, &Node, &RegBase, &RegSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: COM0 DSDT device has no matching DT ns16550a node at 0x%Lx: %r\n",
      KMH_UART0_BASE,
      Status
      ));
  } else {
    ClockFrequency = KmhAcpiGetU32Property (FdtClient, Node, "clock-frequency", 0);
    RegShift       = KmhAcpiGetU32Property (FdtClient, Node, "reg-shift", 0);
    RegIoWidth     = KmhAcpiGetU32Property (FdtClient, Node, "reg-io-width", 0);
    CurrentSpeed   = KmhAcpiGetU32Property (FdtClient, Node, "current-speed", 0);

    if (RegSize < KMH_UART0_ACPI_SIZE) {
      DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: COM0 _CRS size exceeds DT reg node=%d DT=0x%Lx ACPI=0x%Lx\n",
        Node,
        RegSize,
        KMH_UART0_ACPI_SIZE
        ));
    }

    if ((ClockFrequency != KMH_UART0_CLOCK_FREQUENCY) ||
        (RegShift != KMH_UART0_REG_SHIFT) ||
        (RegIoWidth != KMH_UART0_REG_IO_WIDTH) ||
        (CurrentSpeed != KMH_UART0_CURRENT_SPEED))
    {
      DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: COM0 _DSD mismatch node=%d clock=%u shift=%u width=%u speed=%u\n",
        Node,
        ClockFrequency,
        RegShift,
        RegIoWidth,
        CurrentSpeed
        ));
    } else {
      DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: COM0 matches DT node=%d base=0x%Lx dt-size=0x%Lx acpi-size=0x%Lx\n",
        Node,
        RegBase,
        RegSize,
        KMH_UART0_ACPI_SIZE
        ));
    }
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "interrupt-parent", "COM0 irq-parent");
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "interrupts", "COM0 interrupts");
  }

  Status = KmhAcpiFindCompatibleReg (FdtClient, "riscv,clint0", KMH_CLINT_BASE, &Node, &RegBase, &RegSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: CLINT timer has no matching DT riscv,clint0 node at 0x%Lx: %r\n",
      KMH_CLINT_BASE,
      Status
      ));
  } else if (RegSize != KMH_CLINT_SIZE) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: CLINT timer size mismatch node=%d DT size=0x%Lx ACPI expected=0x%Lx\n",
      Node,
      RegSize,
      KMH_CLINT_SIZE
      ));
  } else {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: CLINT timer matches DT node=%d base=0x%Lx size=0x%Lx\n",
      Node,
      RegBase,
      RegSize
      ));
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "interrupts-extended", "CLINT interrupts-extended");
  }

  Status = KmhAcpiFindCompatibleReg (FdtClient, "riscv,aplic", KMH_APLIC_S_BASE, &Node, &RegBase, &RegSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: IC00 DSDT device has no matching DT riscv,aplic node at 0x%Lx: %r\n",
      KMH_APLIC_S_BASE,
      Status
      ));
  } else {
    NumSources = KmhAcpiGetU32Property (FdtClient, Node, "riscv,num-sources", 0);

    if ((RegSize != KMH_APLIC_S_SIZE) || (NumSources != KMH_APLIC_S_NUM_SOURCES)) {
      DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: IC00 APLIC mismatch node=%d DT size=0x%Lx sources=%u ACPI size=0x%Lx sources=%u\n",
        Node,
        RegSize,
        NumSources,
        KMH_APLIC_S_SIZE,
        KMH_APLIC_S_NUM_SOURCES
        ));
    } else {
      DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: IC00 matches DT APLIC node=%d base=0x%Lx size=0x%Lx sources=%u\n",
        Node,
        RegBase,
        RegSize,
        NumSources
        ));
    }
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "msi-parent", "APLIC msi-parent");
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "#interrupt-cells", "APLIC interrupt-cells");
  }

  Status = KmhAcpiFindCompatibleReg (FdtClient, "riscv,imsics", KMH_IMSIC_S_BASE, &Node, &RegBase, &RegSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: MADT IMSIC has no matching DT riscv,imsics node at 0x%Lx: %r\n",
      KMH_IMSIC_S_BASE,
      Status
      ));
  } else {
    NumIds         = KmhAcpiGetU32Property (FdtClient, Node, "riscv,num-ids", 0);
    GuestIndexBits = KmhAcpiGetU32Property (FdtClient, Node, "riscv,guest-index-bits", 0);

    if ((RegSize != KMH_IMSIC_S_SIZE) ||
        (NumIds != KMH_IMSIC_S_NUM_IDS) ||
        (GuestIndexBits != KMH_IMSIC_S_GUEST_INDEX_BITS))
    {
      DEBUG ((DEBUG_ERROR, "KMH-DT-ACPI: MADT IMSIC mismatch node=%d DT size=0x%Lx ids=%u guest-bits=%u ACPI size=0x%Lx ids=%u guest-bits=%u\n",
        Node,
        RegSize,
        NumIds,
        GuestIndexBits,
        KMH_IMSIC_S_SIZE,
        KMH_IMSIC_S_NUM_IDS,
        KMH_IMSIC_S_GUEST_INDEX_BITS
        ));
    } else {
      DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: MADT IMSIC matches DT node=%d base=0x%Lx size=0x%Lx ids=%u guest-bits=%u\n",
        Node,
        RegBase,
        RegSize,
        NumIds,
        GuestIndexBits
        ));
    }
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "interrupts-extended", "IMSIC interrupts-extended");
    KmhAcpiLogDtU32ArrayProperty (FdtClient, Node, "#interrupt-cells", "IMSIC interrupt-cells");
  }

  Sdh0Present = KmhAcpiLogOptionalCompatibleReg (FdtClient, "snps,sdhci", "SDH0", &Sdh0Node, &Sdh0Base, &Sdh0Size);
  if (Sdh0Present) {
    KmhAcpiInstallOptionalDeviceGuarded (
      "SDH0",
      "snps,sdhci",
      Sdh0Node,
      Sdh0Base,
      Sdh0Size,
      KMH_ENABLE_OPTIONAL_SDH0_ACPI
      );
  }

  Xdm0Present = KmhAcpiLogOptionalCompatibleReg (FdtClient, "xlnx,xdma-host-3.00", "XDM0", &Xdm0Node, &Xdm0Base, &Xdm0Size);
  if (Xdm0Present) {
    KmhAcpiInstallOptionalDeviceGuarded (
      "XDM0",
      "xlnx,xdma-host-3.00",
      Xdm0Node,
      Xdm0Base,
      Xdm0Size,
      KMH_ENABLE_OPTIONAL_XDM0_ACPI
      );
  }

  VirtioMmioCount = 0;
  for (FindStatus = FdtClient->FindCompatibleNode (FdtClient, "virtio,mmio", &Node);
       !EFI_ERROR (FindStatus);
       FindStatus = FdtClient->FindNextCompatibleNode (FdtClient, "virtio,mmio", Node, &Node))
  {
    CONST UINT32  *Reg;
    UINT32        PropertySize;

    Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Reg, &PropertySize);
    if (EFI_ERROR (Status) || (PropertySize < 4 * sizeof (UINT32))) {
      DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: DT-only virtio-mmio node=%d has no valid reg property: %r\n",
        Node,
        Status
        ));
      continue;
    }

    RegBase = KmhAcpiFdtReadCells (Reg, 2);
    RegSize = KmhAcpiFdtReadCells (&Reg[2], 2);
    VirtioMmioCount++;
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: DT-only virtio-mmio node=%d base=0x%Lx size=0x%Lx not installed as ACPI device\n",
      Node,
      RegBase,
      RegSize
      ));
    KmhAcpiInstallOptionalDeviceGuarded (
      "VMMO",
      "virtio,mmio",
      Node,
      RegBase,
      RegSize,
      KMH_ENABLE_OPTIONAL_VIRTIO_MMIO_ACPI
      );
  }

  if (VirtioMmioCount == 0) {
    DEBUG ((DEBUG_INFO, "KMH-DT-ACPI: no enabled virtio-mmio DT node discovered by FdtClient\n"));
  }

  KmhAcpiLogCurrentAcpiPolicy (PcieRcCount, Sdh0Present, Xdm0Present, VirtioMmioCount);
}

STATIC
EFI_STATUS
KmhAcpiInstallMcfgFromDt (
  IN CONST KMH_DT_PCIE_RC_INFO  *CachedRcInfo
  )
{
  EFI_STATUS               Status;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTable;
  FDT_CLIENT_PROTOCOL      *FdtClient;
  KMH_DT_PCIE_RC_INFO      RcInfoArray[KMH_PCIE_MAX_RC_COUNT];
  KMH_ACPI_MCFG_TABLE      Mcfg;
  UINTN                    TableKey;
  UINTN                    RcCount;
  UINTN                    Index;
  UINTN                    AllocationCount;
  UINTN                    TableSize;

  TableKey = 0;
  RcCount  = 0;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MCFG: ACPI table protocol not available: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (!EFI_ERROR (Status)) {
    Status = KmhAcpiCollectPcieRcInfoFromDt (FdtClient, RcInfoArray, KMH_PCIE_MAX_RC_COUNT, &RcCount);
  }

  if (EFI_ERROR (Status) || (RcCount == 0)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-MCFG: multi-allocation collection failed, fall back to cached RC0: %r\n", Status));
    RcCount = 0;
  }

  if ((RcCount == 0) && (CachedRcInfo != NULL) && CachedRcInfo->Found && CachedRcInfo->FoundMmio32) {
    CopyMem (&RcInfoArray[0], CachedRcInfo, sizeof (RcInfoArray[0]));
    RcCount = 1;
  }

  if (RcCount == 0) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MCFG: no DT PCIe RC available for MCFG\n"));
    return EFI_NOT_FOUND;
  }

  ZeroMem (&Mcfg, sizeof (Mcfg));
  AllocationCount = 0;
  for (Index = 0; (Index < RcCount) && (AllocationCount < KMH_PCIE_MAX_RC_COUNT); Index++) {
    UINT8   BusMin;
    UINT8   BusMax;
    UINT64  ConfigBusCount;

    if (!RcInfoArray[Index].Found || !RcInfoArray[Index].FoundMmio32) {
      DEBUG ((DEBUG_WARN, "KMH-DT-MCFG: skip RC%u node=%d because MCFG base is unavailable\n",
        (UINT32)Index,
        RcInfoArray[Index].Node
        ));
      continue;
    }

    BusMin = RcInfoArray[Index].BusMin;
    BusMax = RcInfoArray[Index].BusMax;
    if ((Index > 0) &&
        (RcInfoArray[Index].ConfigSize > 0) &&
        (RcInfoArray[Index].ConfigSize < MultU64x32 ((UINT64)(BusMax - BusMin + 1), KMH_PCI_ECAM_BUS_SIZE)))
    {
      ConfigBusCount = (RcInfoArray[Index].ConfigSize + KMH_PCI_ECAM_BUS_SIZE - 1) / KMH_PCI_ECAM_BUS_SIZE;
      if (ConfigBusCount == 0) {
        ConfigBusCount = 1;
      }

      BusMax = (UINT8)(BusMin + (UINT8)ConfigBusCount - 1);
      DEBUG ((DEBUG_WARN, "KMH-DT-MCFG: cap RC%u node=%d bus range to %u-%u because config-size=0x%Lx\n",
        (UINT32)Index,
        RcInfoArray[Index].Node,
        BusMin,
        BusMax,
        RcInfoArray[Index].ConfigSize
        ));
    }

    Mcfg.Allocation[AllocationCount].BaseAddress      = RcInfoArray[Index].McfgBase;
    Mcfg.Allocation[AllocationCount].PciSegmentGroupNumber = (UINT16)AllocationCount;
    Mcfg.Allocation[AllocationCount].StartBusNumber   = BusMin;
    Mcfg.Allocation[AllocationCount].EndBusNumber     = BusMax;
    Mcfg.Allocation[AllocationCount].Reserved         = EFI_ACPI_RESERVED_DWORD;
    DEBUG ((DEBUG_INFO, "KMH-DT-MCFG: allocation %u node=%d segment=%u base=0x%Lx bus=%u-%u ecam-end=0x%Lx\n",
      (UINT32)AllocationCount,
      RcInfoArray[Index].Node,
      (UINT32)Mcfg.Allocation[AllocationCount].PciSegmentGroupNumber,
      RcInfoArray[Index].McfgBase,
      BusMin,
      BusMax,
      RcInfoArray[Index].McfgBase + MultU64x32 ((UINT64)(BusMax - BusMin + 1), KMH_PCI_ECAM_BUS_SIZE) - 1
      ));
    AllocationCount++;
  }

  if (AllocationCount == 0) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MCFG: no usable allocation after filtering RC count=%u\n", (UINT32)RcCount));
    return EFI_NOT_FOUND;
  }
  if (AllocationCount > 1) {
    DEBUG ((DEBUG_INFO, "KMH-DT-MCFG: multi-allocation MCFG paired with DSDT PCI0/PCI1 root bridges\n"));
  }

  TableSize = sizeof (Mcfg.Header) +
              (AllocationCount * sizeof (EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE));
  Mcfg.Header.Header.Signature        = EFI_ACPI_6_1_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE;
  Mcfg.Header.Header.Length           = (UINT32)TableSize;
  Mcfg.Header.Header.Revision         = EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION;
  Mcfg.Header.Header.Checksum         = 0;
  CopyMem (Mcfg.Header.Header.OemId, mKmhAcpiOemId, sizeof (Mcfg.Header.Header.OemId));
  Mcfg.Header.Header.OemTableId       = EFI_ACPI_OEM_TABLE_ID;
  Mcfg.Header.Header.OemRevision      = EFI_ACPI_OEM_REVISION;
  Mcfg.Header.Header.CreatorId        = EFI_ACPI_CREATOR_ID;
  Mcfg.Header.Header.CreatorRevision  = EFI_ACPI_CREATOR_REVISION;
  Mcfg.Header.Reserved                = EFI_ACPI_RESERVED_QWORD;

  Status = AcpiTable->InstallAcpiTable (AcpiTable, &Mcfg, TableSize, &TableKey);
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_INFO,
    "KMH-DT-MCFG: install MCFG allocations=%u size=0x%Lx status=%r key=0x%Lx\n",
    (UINT32)AllocationCount,
    (UINT64)TableSize,
    Status,
    (UINT64)TableKey
    ));

  return Status;
}

#if KMH_ENABLE_ECAM_RESERVATION_SSDT
STATIC
EFI_STATUS
KmhAcpiInstallEcamReservationFromDt (
  VOID
  )
{
  EFI_STATUS               Status;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTable;
  KMH_DT_PCIE_RC_INFO      RcInfo;
  UINT8                    *Ssdt;
  UINTN                    SsdtSize;
  UINTN                    TableKey;
  UINT64                   EcamSize;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ECAM: ACPI table protocol not available: %r\n", Status));
    return Status;
  }

  Status = KmhAcpiGetPcieRcInfoFromDt (&RcInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ECAM: no DT ECAM resource for reservation SSDT: %r\n", Status));
    return Status;
  }

  if (RcInfo.BusMax < RcInfo.BusMin) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ECAM: invalid bus range %u-%u\n", RcInfo.BusMin, RcInfo.BusMax));
    return EFI_INVALID_PARAMETER;
  }

  EcamSize = MultU64x32 (
               KMH_PCI_ECAM_BUS_SIZE,
               (UINT32)RcInfo.BusMax - (UINT32)RcInfo.BusMin + 1
               );
  if ((RcInfo.McfgBase > MAX_UINT32) || (EcamSize > MAX_UINT32)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ECAM: ECAM does not fit Memory32Fixed base=0x%Lx size=0x%Lx\n",
      RcInfo.McfgBase,
      EcamSize
      ));
    return EFI_UNSUPPORTED;
  }

  SsdtSize = sizeof (mKmhEcamReservationSsdtTemplate);
  Ssdt = AllocateCopyPool (SsdtSize, mKmhEcamReservationSsdtTemplate);
  if (Ssdt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  KmhAcpiWriteUnaligned32 (&Ssdt[KMH_ECAM_SSDT_BASE_OFFSET], (UINT32)RcInfo.McfgBase);
  KmhAcpiWriteUnaligned32 (&Ssdt[KMH_ECAM_SSDT_SIZE_OFFSET], (UINT32)EcamSize);
  Ssdt[KMH_ACPI_TABLE_CHECKSUM_OFFSET] = 0;
  Ssdt[KMH_ACPI_TABLE_CHECKSUM_OFFSET] = KmhAcpiChecksum8 (Ssdt, SsdtSize);

  TableKey = 0;
  Status = AcpiTable->InstallAcpiTable (AcpiTable, Ssdt, SsdtSize, &TableKey);
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_INFO,
    "KMH-DT-ECAM: install ECAM reservation base=0x%Lx size=0x%Lx bus=%u-%u status=%r key=0x%Lx\n",
    RcInfo.McfgBase,
    EcamSize,
    RcInfo.BusMin,
    RcInfo.BusMax,
    Status,
    (UINT64)TableKey
    ));

  FreePool (Ssdt);
  return Status;
}
#endif

STATIC
VOID
KmhAcpiPcieDbiRoWriteEnable (
  IN UINT64  DbiBase,
  IN BOOLEAN Enable
  )
{
  UINT32 Val;

  Val = MmioRead32 (DbiBase + PCIE_MISC_CONTROL_1_OFF);
  if (Enable) {
    Val |= PCIE_DBI_RO_WR_EN;
  } else {
    Val &= ~PCIE_DBI_RO_WR_EN;
  }

  MmioWrite32 (DbiBase + PCIE_MISC_CONTROL_1_OFF, Val);
}

STATIC
VOID
KmhAcpiPcieAtuWrite32 (
  IN UINT64 DbiBase,
  IN UINT32 Index,
  IN UINT32 Offset,
  IN UINT32 Value
  )
{
  MmioWrite32 (DbiBase + PCIE_ATU_VIEWPORT, Index);
  MmioWrite32 (DbiBase + PCIE_ATU_VIEWPORT_BASE + Offset, Value);
}

STATIC
UINT32
KmhAcpiPcieAtuRead32 (
  IN UINT64 DbiBase,
  IN UINT32 Index,
  IN UINT32 Offset
  )
{
  MmioWrite32 (DbiBase + PCIE_ATU_VIEWPORT, Index);
  return MmioRead32 (DbiBase + PCIE_ATU_VIEWPORT_BASE + Offset);
}

STATIC
VOID
KmhAcpiPcieProgramOutboundAtu (
  IN UINT64 DbiBase,
  IN CONST CHAR8 *RcName,
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

  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_LOWER_BASE, (UINT32)CpuBase);
  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_UPPER_BASE, (UINT32)(CpuBase >> 32));
  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_LIMIT, (UINT32)Limit);
  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_LOWER_TARGET, (UINT32)PciBase);
  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_UPPER_TARGET, (UINT32)(PciBase >> 32));
  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_REGION_CTRL1, Type);
  KmhAcpiPcieAtuWrite32 (DbiBase, Index, PCIE_ATU_REGION_CTRL2, PCIE_ATU_ENABLE | Ctrl2Extra);

  for (Retry = 0; Retry < 5; Retry++) {
    Ctrl2 = KmhAcpiPcieAtuRead32 (DbiBase, Index, PCIE_ATU_REGION_CTRL2);
    if ((Ctrl2 & PCIE_ATU_ENABLE) != 0) {
      break;
    }

    MicroSecondDelay (9000);
  }

  DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a ATU%u type=0x%x cpu=0x%lx pci=0x%lx size=0x%lx ctrl2=0x%x\n",
    RcName,
    Index,
    Type,
    CpuBase,
    PciBase,
    Size,
    KmhAcpiPcieAtuRead32 (DbiBase, Index, PCIE_ATU_REGION_CTRL2)
    ));
}

STATIC
VOID
KmhAcpiPcieWaitForLink (
  IN UINT64 DbiBase,
  IN CONST CHAR8 *RcName
  )
{
  UINT32 Retry;
  UINT32 Debug1;

  Debug1 = 0;
  for (Retry = 0; Retry < 100; Retry++) {
    Debug1 = MmioRead32 (DbiBase + PCIE_PORT_DEBUG1);
    if ((Debug1 & PCIE_PORT_DEBUG1_LINK_UP) != 0) {
      DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a link up debug1=0x%x retry=%u\n", RcName, Debug1, Retry));
      return;
    }

    MicroSecondDelay (10000);
  }

  DEBUG ((DEBUG_ERROR, "KMH-ACPI-PCIE: %a link not up debug1=0x%x\n", RcName, Debug1));
}

STATIC
VOID
KmhAcpiPcieInitRc (
  IN CONST CHAR8                *RcName,
  IN CONST KMH_DT_PCIE_RC_INFO  *RcInfo,
  IN BOOLEAN                    EnableCfg1
  )
{
  UINT32  Val;
  UINT64  DbiBase;
  UINT64  EcamBase;
  UINT64  Mmio32CpuBase;
  UINT64  Mmio32PciBase;
  UINT64  Mmio32Size;
  UINT64  Mmio64CpuBase;
  UINT64  Mmio64PciBase;
  UINT64  Mmio64Size;
  UINT32  Vendor;

  if ((RcInfo == NULL) || !RcInfo->Found || !RcInfo->FoundMmio32 || (RcInfo->DbiBase == 0) || (RcInfo->McfgBase == 0)) {
    DEBUG ((DEBUG_ERROR, "KMH-ACPI-PCIE: %a skip hardware init, DT PCIe resource incomplete\n", RcName));
    return;
  }

  DbiBase       = RcInfo->DbiBase;
  EcamBase      = RcInfo->McfgBase;
  Mmio32CpuBase = RcInfo->Mmio32CpuBase;
  Mmio32PciBase = RcInfo->Mmio32PciBase;
  Mmio32Size    = RcInfo->Mmio32Size;
  Mmio64CpuBase = RcInfo->FoundMmio64 ? RcInfo->Mmio64CpuBase : 0;
  Mmio64PciBase = RcInfo->FoundMmio64 ? RcInfo->Mmio64PciBase : 0;
  Mmio64Size    = RcInfo->FoundMmio64 ? RcInfo->Mmio64Size : 0;

  DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a init dbi=0x%lx ecam=0x%lx mem32 cpu=0x%lx pci=0x%lx size=0x%lx mem64 cpu=0x%lx pci=0x%lx size=0x%lx\n",
    RcName,
    DbiBase,
    EcamBase,
    Mmio32CpuBase,
    Mmio32PciBase,
    Mmio32Size,
    Mmio64CpuBase,
    Mmio64PciBase,
    Mmio64Size
    ));

  Vendor = MmioRead32 (DbiBase);
  if ((Vendor == 0) || (Vendor == MAX_UINT32)) {
    DEBUG ((DEBUG_WARN, "KMH-ACPI-PCIE: %a DBI absent at 0x%lx vendor=0x%x, skip hardware init\n",
      RcName,
      DbiBase,
      Vendor
      ));
    return;
  }

  KmhAcpiPcieDbiRoWriteEnable (DbiBase, TRUE);

  MmioWrite32 (DbiBase + PCI_BASE_ADDRESS_0, 0x00000004);
  MmioWrite32 (DbiBase + PCI_BASE_ADDRESS_1, 0x00000000);

  Val = MmioRead32 (DbiBase + PCI_INTERRUPT_LINE);
  Val &= 0xFFFF00FF;
  Val |= 0x00000100;
  MmioWrite32 (DbiBase + PCI_INTERRUPT_LINE, Val);

  Val = MmioRead32 (DbiBase + PCI_PRIMARY_BUS);
  Val &= 0xFF000000;
  Val |= 0x00FF0100;
  MmioWrite32 (DbiBase + PCI_PRIMARY_BUS, Val);

  Val = MmioRead32 (DbiBase + PCI_COMMAND_OFFSET);
  Val &= 0xFFFF0000;
  Val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
  MmioWrite32 (DbiBase + PCI_COMMAND_OFFSET, Val);

  MmioWrite16 (DbiBase + PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

  KmhAcpiPcieProgramOutboundAtu (DbiBase, RcName, 0, PCIE_ATU_TYPE_CFG0, EcamBase, 0, 0x00100000, PCIE_ATU_CFG_SHIFT_MODE_ENABLE);
  if (EnableCfg1) {
    KmhAcpiPcieProgramOutboundAtu (DbiBase, RcName, 1, PCIE_ATU_TYPE_CFG0, EcamBase + 0x00100000ULL, KMH_PCIE_CFG0_PCI_BASE, KMH_PCIE_CFG0_SIZE, PCIE_ATU_CFG_SHIFT_MODE_ENABLE);
  } else {
    DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a ATU1/CFG0 bus1 disabled by platform policy\n", RcName));
  }

  if (EnableCfg1) {
    KmhAcpiPcieProgramOutboundAtu (DbiBase, RcName, 2, PCIE_ATU_TYPE_CFG1, EcamBase + 0x00200000ULL, KMH_PCIE_CFG1_PCI_BASE, KMH_PCIE_CFG1_SIZE, PCIE_ATU_CFG_SHIFT_MODE_ENABLE);
  } else {
    DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a ATU2/CFG1 disabled by platform policy\n", RcName));
  }

  KmhAcpiPcieProgramOutboundAtu (DbiBase, RcName, 3, PCIE_ATU_TYPE_MEM, Mmio32CpuBase, Mmio32PciBase, Mmio32Size, 0);
  DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a MEM64 ATU not programmed; ACPI _CRS still advertises MEM64\n", RcName));

  MmioWrite32 (DbiBase + PCI_BASE_ADDRESS_0, 0);
  MmioWrite16 (DbiBase + PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

  Val = MmioRead32 (DbiBase + PCIE_LINK_WIDTH_SPEED_CONTROL);
  Val |= PORT_LOGIC_SPEED_CHANGE;
  MmioWrite32 (DbiBase + PCIE_LINK_WIDTH_SPEED_CONTROL, Val);

  KmhAcpiPcieDbiRoWriteEnable (DbiBase, FALSE);
  KmhAcpiPcieWaitForLink (DbiBase, RcName);

  DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: %a DBI vendor=0x%x class=0x%x cmd=0x%x bus=0x%x\n",
    RcName,
    MmioRead32 (DbiBase),
    MmioRead32 (DbiBase + 0x08),
    MmioRead32 (DbiBase + PCI_COMMAND_OFFSET),
    MmioRead32 (DbiBase + PCI_PRIMARY_BUS)
    ));
}

STATIC
VOID
KmhAcpiPcieInitRc0 (
  IN CONST KMH_DT_PCIE_RC_INFO  *RcInfo
  )
{
  KmhAcpiPcieInitRc (
    "RC0",
    RcInfo,
    TRUE
    );
}

STATIC
VOID
EFIAPI
KmhAcpiPcieExitBootServicesNotify (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  if (EFI_ERROR (mKmhAcpiPcieRcInfoStatus)) {
    DEBUG ((DEBUG_ERROR, "KMH-ACPI-PCIE: skip RC0 init at ExitBootServices, DT RC info unavailable: %r\n", mKmhAcpiPcieRcInfoStatus));
    return;
  }

  KmhAcpiPcieInitRc0 (&mKmhAcpiPcieRcInfo);
  DEBUG ((DEBUG_INFO, "KMH-ACPI-PCIE: RC1 init disabled by platform policy\n"));
}




STATIC
EFI_STATUS
FindAcpiRsdpInMemory (
  IN  UINTN                                            StartAddress,
  IN  UINTN                                            EndAddress,
  OUT EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER     **RsdpPtr
  )
{
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp;
  UINT8                                         *AcpiPtr;
  UINTN                                         ScanSize;

  ScanSize = sizeof (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER);
  if ((EndAddress < StartAddress) || (EndAddress - StartAddress < ScanSize)) {
    return EFI_NOT_FOUND;
  }

  for (AcpiPtr = (UINT8 *)StartAddress;
       AcpiPtr <= (UINT8 *)(EndAddress - ScanSize);
       AcpiPtr += 0x10)
  {
    Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)(UINTN)AcpiPtr;
    if (CompareMem (&Rsdp->Signature, "RSD PTR ", 8) != 0) {
      continue;
    }

    if (CalculateSum8 (
          (CONST UINT8 *)Rsdp,
          sizeof (EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER)
          ) != 0)
    {
      continue;
    }

    if ((Rsdp->Revision >= 2) &&
        (CalculateSum8 ((CONST UINT8 *)Rsdp, sizeof (*Rsdp)) != 0))
    {
      continue;
    }

    *RsdpPtr = Rsdp;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
InstallOneAcpiTable (
  IN EFI_ACPI_TABLE_PROTOCOL     *AcpiTableProtocol,
  IN EFI_ACPI_DESCRIPTION_HEADER *Table
  )
{
  UINTN  TableHandle;

  return AcpiTableProtocol->InstallAcpiTable (
                              AcpiTableProtocol,
                              Table,
                              Table->Length,
                              &TableHandle
                              );
}

STATIC
EFI_STATUS
InstallAcpiTablesFromRsdp (
  IN EFI_ACPI_TABLE_PROTOCOL                       *AcpiTableProtocol,
  IN EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp
  )
{
  EFI_STATUS                                  Status;
  EFI_ACPI_DESCRIPTION_HEADER                 *Xsdt;
  EFI_ACPI_DESCRIPTION_HEADER                 *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER                 *CurrentTable;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *Fadt;
  EFI_ACPI_DESCRIPTION_HEADER                 *Dsdt;
  UINTN                                       NumberOfTableEntries;
  UINTN                                       Index;

  Fadt = NULL;
  Dsdt = NULL;

  if (Rsdp->XsdtAddress != 0) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;
    NumberOfTableEntries = (Xsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) /
                           sizeof (UINT64);

    for (Index = 0; Index < NumberOfTableEntries; Index++) {
      CurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)*(UINT64 *)(
                       (UINT8 *)Xsdt + sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
                       Index * sizeof (UINT64)
                       );
      Status = InstallOneAcpiTable (AcpiTableProtocol, CurrentTable);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (CompareMem (&CurrentTable->Signature, "FACP", 4) == 0) {
        Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)CurrentTable;
      }
    }
  } else if (Rsdp->RsdtAddress != 0) {
    Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->RsdtAddress;
    NumberOfTableEntries = (Rsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) /
                           sizeof (UINT32);

    for (Index = 0; Index < NumberOfTableEntries; Index++) {
      CurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)*(UINT32 *)(
                       (UINT8 *)Rsdt + sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
                       Index * sizeof (UINT32)
                       );
      Status = InstallOneAcpiTable (AcpiTableProtocol, CurrentTable);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (CompareMem (&CurrentTable->Signature, "FACP", 4) == 0) {
        Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)CurrentTable;
      }
    }
  }

  if (Fadt == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no FADT found\n", __func__));
    return EFI_NOT_FOUND;
  }

  if (Fadt->XDsdt != 0) {
    Dsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Fadt->XDsdt;
  } else if (Fadt->Dsdt != 0) {
    Dsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Fadt->Dsdt;
  }

  if (Dsdt == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no DSDT found\n", __func__));
    return EFI_NOT_FOUND;
  }

  return InstallOneAcpiTable (AcpiTableProtocol, Dsdt);
}

STATIC
EFI_STATUS
InstallAcpiFromDdrHandoff (
  VOID
  )
{
  EFI_STATUS                                      Status;
  FDT_CLIENT_PROTOCOL                            *FdtClient;
  EFI_ACPI_TABLE_PROTOCOL                        *AcpiTableProtocol;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER   *Rsdp;
  CONST UINT64                                   *Reg;
  UINTN                                          AddressCells;
  UINTN                                          SizeCells;
  UINT32                                         RegSize;
  UINT64                                         HandoffBase;
  UINT64                                         HandoffSize;

  Status = gBS->LocateProtocol (
                  &gFdtClientProtocolGuid,
                  NULL,
                  (VOID **)&FdtClient
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FdtClient->FindCompatibleNodeReg (
                        FdtClient,
                        "bosc,kmh-acpi-handoff",
                        (CONST VOID **)&Reg,
                        &AddressCells,
                        &SizeCells,
                        &RegSize
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((AddressCells != 2) || (SizeCells != 2) || (RegSize != 2 * sizeof (UINT64))) {
    DEBUG ((DEBUG_ERROR, "%a: invalid ACPI handoff reg\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  HandoffBase = SwapBytes64 (Reg[0]);
  HandoffSize = SwapBytes64 (Reg[1]);
  if ((HandoffSize == 0) || (HandoffBase > MAX_UINTN) ||
      (HandoffSize > MAX_UINTN) || (HandoffBase > MAX_UINTN - HandoffSize))
  {
    DEBUG ((DEBUG_ERROR, "%a: invalid ACPI handoff range\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = FindAcpiRsdpInMemory (
             (UINTN)HandoffBase,
             (UINTN)(HandoffBase + HandoffSize),
             &Rsdp
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: no valid RSDP in ACPI handoff range: %r\n", __func__, Status));
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InstallAcpiTablesFromRsdp (AcpiTableProtocol, Rsdp);
  if (!EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: installed ACPI tables from DDR handoff @ 0x%Lx/0x%Lx\n",
      __func__,
      HandoffBase,
      HandoffSize
      ));
  }

  return Status;
}

EFI_STATUS
EFIAPI
PlatformAcpiDriverEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;
  EFI_STATUS RhctStatus;
  EFI_STATUS MadtStatus;
  EFI_STATUS McfgStatus;
#if KMH_ENABLE_ECAM_RESERVATION_SSDT
  EFI_STATUS EcamStatus;
#endif

  Status = InstallAcpiFromDdrHandoff ();
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }
  
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_CALLBACK,
                  KmhAcpiPcieExitBootServicesNotify,
                  NULL,
                  &mKmhAcpiPcieExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "KMH-ACPI-PCIE: failed to create ExitBootServices event: %r\n", Status));
  }

  mKmhAcpiPcieRcInfoStatus = KmhAcpiGetPcieRcInfoFromDt (&mKmhAcpiPcieRcInfo);
  if (EFI_ERROR (mKmhAcpiPcieRcInfoStatus)) {
    DEBUG ((DEBUG_WARN, "KMH-ACPI-PCIE: failed to cache DT RC0 info: %r\n", mKmhAcpiPcieRcInfoStatus));
  } else {
    KmhAcpiValidatePciCrsAgainstDt (&mKmhAcpiPcieRcInfo);
  }
  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (!EFI_ERROR (Status)) {
    Status = KmhAcpiCollectPcieRcInfoFromDt (FdtClient, mKmhAcpiPcieRcInfoArray, KMH_PCIE_MAX_RC_COUNT, &mKmhAcpiPcieRcCount);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "KMH-ACPI-PCIE: failed to cache PCIe RC array: %r\n", Status));
      mKmhAcpiPcieRcCount = 0;
    }
  } else {
    DEBUG ((DEBUG_WARN, "KMH-ACPI-PCIE: FDT client unavailable for PCIe RC array cache: %r\n", Status));
  }
  KmhAcpiValidateStaticDevicesAgainstDt ();

  Status = LocateAndInstallAcpiFromFv (&mAcpiTableFile);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RhctStatus = KmhAcpiInstallRhctFromDt ();
  if (EFI_ERROR (RhctStatus)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-RHCT: failed to install dynamic RHCT: %r\n", RhctStatus));
  }

  MadtStatus = KmhAcpiInstallMadtFromDt ();
  if (EFI_ERROR (MadtStatus)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MADT: failed to install dynamic MADT: %r\n", MadtStatus));
  }

  McfgStatus = KmhAcpiInstallMcfgFromDt (EFI_ERROR (mKmhAcpiPcieRcInfoStatus) ? NULL : &mKmhAcpiPcieRcInfo);
  if (EFI_ERROR (McfgStatus)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-MCFG: failed to install dynamic MCFG: %r\n", McfgStatus));
  }

#if KMH_ENABLE_ECAM_RESERVATION_SSDT
  EcamStatus = KmhAcpiInstallEcamReservationFromDt ();
  if (EFI_ERROR (EcamStatus)) {
    DEBUG ((DEBUG_ERROR, "KMH-DT-ECAM: failed to install ECAM reservation SSDT: %r\n", EcamStatus));
  }
#else
  DEBUG ((DEBUG_INFO, "KMH-DT-ECAM: ECAM reservation SSDT disabled\n"));
#endif

  return Status;
}
