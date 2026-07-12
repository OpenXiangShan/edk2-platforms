/** @file
  PCI Host Bridge Library for QEMU kmh-bosc-soc DesignWare PCIe roots.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Guid/FdtHob.h>
#include <IndustryStandard/Acpi10.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <libfdt.h>

#define KMH_BOSC_PCIE_MAX_ROOTS             12
#define KMH_BOSC_PCIE_COMPATIBLE            "snps,dw-pcie"

#define FDT_PCI_RANGE_TYPE_MASK             (BIT25 | BIT24)
#define FDT_PCI_RANGE_MMIO                  BIT25
#define FDT_PCI_RANGE_MMIO_64BIT            (BIT25 | BIT24)

#define DW_PCIE_ATU_VIEWPORT                0x900
#define DW_PCIE_ATU_CR1                     0x904
#define DW_PCIE_ATU_CR2                     0x908
#define DW_PCIE_ATU_ENABLE                  BIT31
#define DW_PCIE_ATU_TYPE_MEM                0x0
#define DW_PCIE_ATU_LOWER_BASE              0x90c
#define DW_PCIE_ATU_UPPER_BASE              0x910
#define DW_PCIE_ATU_LIMIT                   0x914
#define DW_PCIE_ATU_LOWER_TARGET            0x918
#define DW_PCIE_ATU_UPPER_TARGET            0x91c

#define DW_PCIE_MEM32_VIEWPORT              1
#define DW_PCIE_MEM64_VIEWPORT              2

#define KMH_BOSC_UEFI_PCIE_MMIO_BASE        0x50000000ULL
#define KMH_BOSC_UEFI_PCIE_ROOT_STRIDE      0x04000000ULL
#define KMH_BOSC_UEFI_PCIE_WINDOW_SIZE      0x02000000ULL

#pragma pack(1)
typedef struct {
  ACPI_HID_DEVICE_PATH        AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    EndDevicePath;
} KMH_BOSC_PCI_ROOT_DEVICE_PATH;
#pragma pack()

typedef struct {
  UINT32                    Segment;
  UINT64                    DbiBase;
  UINT64                    DbiSize;
  UINT64                    CfgBase;
  UINT64                    CfgSize;
  UINT8                     BusBase;
  UINT8                     BusLimit;
  PCI_ROOT_BRIDGE_APERTURE  Mem;
  PCI_ROOT_BRIDGE_APERTURE  MemAbove4G;
} KMH_BOSC_PCIE_ROOT_INFO;

STATIC CONST KMH_BOSC_PCI_ROOT_DEVICE_PATH  mRootBridgeDevicePathTemplate = {
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH) >> 8)
      }
    },
    EISA_PNP_ID (0x0A03),
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      sizeof (EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED
CHAR16  *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

STATIC KMH_BOSC_PCIE_ROOT_INFO  mRootInfo[KMH_BOSC_PCIE_MAX_ROOTS];
STATIC UINTN                    mRootInfoCount;
STATIC BOOLEAN                  mRootInfoValid;

STATIC
UINT64
KmhBoscReadFdtCells (
  IN OUT CONST UINT32  **Cells,
  IN     INT32         NumCells
  )
{
  UINT64  Value;
  INT32   Index;

  Value = 0;
  for (Index = 0; Index < NumCells; Index++) {
    Value = (Value << 32) | fdt32_to_cpu ((*Cells)[Index]);
  }

  *Cells += NumCells;
  return Value;
}

STATIC
VOID *
KmhBoscGetFdt (
  VOID
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  UINT64             *FdtHobData;

  GuidHob = GetFirstGuidHob (&gFdtHobGuid);
  if (GuidHob == NULL) {
    return NULL;
  }

  FdtHobData = GET_GUID_HOB_DATA (GuidHob);
  return (VOID *)(UINTN)(*FdtHobData);
}

STATIC
BOOLEAN
KmhBoscFdtNodeIsOkay (
  IN VOID   *Fdt,
  IN INT32  Node
  )
{
  CONST CHAR8  *Status;
  INT32        Len;

  Status = fdt_getprop (Fdt, Node, "status", &Len);
  if (Status == NULL) {
    return TRUE;
  }

  return (AsciiStrCmp (Status, "okay") == 0) ||
         (AsciiStrCmp (Status, "ok") == 0);
}

STATIC
INT32
KmhBoscGetNumCells (
  IN VOID         *Fdt,
  IN INT32        Node,
  IN CONST CHAR8  *Name,
  IN INT32        DefaultValue
  )
{
  CONST UINT32  *Prop;
  INT32         Len;
  UINT32        Value;

  Prop = fdt_getprop (Fdt, Node, Name, &Len);
  if (Prop == NULL) {
    return DefaultValue;
  }

  if (Len != sizeof (*Prop)) {
    return -FDT_ERR_BADNCELLS;
  }

  Value = fdt32_to_cpu (*Prop);
  if (Value > FDT_MAX_NCELLS) {
    return -FDT_ERR_BADNCELLS;
  }

  return (INT32)Value;
}

STATIC
VOID
KmhBoscUnavailableAperture (
  OUT PCI_ROOT_BRIDGE_APERTURE  *Aperture
  )
{
  Aperture->Base        = MAX_UINT64;
  Aperture->Limit       = 0;
  Aperture->Translation = 0;
}

STATIC
UINT64
KmhBoscUefiCpuWindowBase (
  IN UINT32   Segment,
  IN BOOLEAN  Above4G
  )
{
  return KMH_BOSC_UEFI_PCIE_MMIO_BASE +
         Segment * KMH_BOSC_UEFI_PCIE_ROOT_STRIDE +
         (Above4G ? KMH_BOSC_UEFI_PCIE_WINDOW_SIZE : 0);
}

STATIC
UINT64
KmhBoscClampUefiWindowSize (
  IN UINT64  Size
  )
{
  if (Size > KMH_BOSC_UEFI_PCIE_WINDOW_SIZE) {
    return KMH_BOSC_UEFI_PCIE_WINDOW_SIZE;
  }

  return Size;
}

STATIC
VOID
KmhBoscProgramOutboundAtu (
  IN UINT64  DbiBase,
  IN UINT32  Viewport,
  IN UINT32  Type,
  IN UINT64  CpuBase,
  IN UINT64  Size,
  IN UINT64  PciBase
  )
{
  UINT64  Limit;

  if ((Size == 0) || (CpuBase > MAX_UINT64 - Size)) {
    return;
  }

  Limit = CpuBase + Size - 1;

  MmioWrite32 (DbiBase + DW_PCIE_ATU_VIEWPORT, Viewport);
  MmioWrite32 (DbiBase + DW_PCIE_ATU_LOWER_BASE, (UINT32)CpuBase);
  MmioWrite32 (DbiBase + DW_PCIE_ATU_UPPER_BASE, (UINT32)(CpuBase >> 32));
  MmioWrite32 (DbiBase + DW_PCIE_ATU_LIMIT, (UINT32)Limit);
  MmioWrite32 (DbiBase + DW_PCIE_ATU_LOWER_TARGET, (UINT32)PciBase);
  MmioWrite32 (DbiBase + DW_PCIE_ATU_UPPER_TARGET, (UINT32)(PciBase >> 32));
  MmioWrite32 (DbiBase + DW_PCIE_ATU_CR1, Type);
  MmioWrite32 (DbiBase + DW_PCIE_ATU_CR2, DW_PCIE_ATU_ENABLE);
}

STATIC
VOID
KmhBoscProgramMemAtuWindows (
  IN CONST KMH_BOSC_PCIE_ROOT_INFO  *Root
  )
{
  UINT64  CpuBase;
  UINT64  Size;

  if (Root->Mem.Base <= Root->Mem.Limit) {
    Size = Root->Mem.Limit - Root->Mem.Base + 1;
    CpuBase = Root->Mem.Base - Root->Mem.Translation;
    KmhBoscProgramOutboundAtu (
      Root->DbiBase,
      DW_PCIE_MEM32_VIEWPORT,
      DW_PCIE_ATU_TYPE_MEM,
      CpuBase,
      Size,
      Root->Mem.Base
      );
  }

  if (Root->MemAbove4G.Base <= Root->MemAbove4G.Limit) {
    Size = Root->MemAbove4G.Limit - Root->MemAbove4G.Base + 1;
    CpuBase = Root->MemAbove4G.Base - Root->MemAbove4G.Translation;
    KmhBoscProgramOutboundAtu (
      Root->DbiBase,
      DW_PCIE_MEM64_VIEWPORT,
      DW_PCIE_ATU_TYPE_MEM,
      CpuBase,
      Size,
      Root->MemAbove4G.Base
      );
  }
}

STATIC
BOOLEAN
KmhBoscParsePcieNode (
  IN  VOID                     *Fdt,
  IN  INT32                    Node,
  OUT KMH_BOSC_PCIE_ROOT_INFO  *Root
  )
{
  CONST UINT32  *Prop;
  CONST UINT32  *Cells;
  INT32         Len;
  INT32         Parent;
  INT32         ParentAddressCells;
  INT32         ParentSizeCells;
  INT32         AddressCells;
  INT32         RangeSizeCells;
  INT32         RegEntryCells;
  INT32         RangeEntryCells;
  UINT32        RangeType;
  UINT64        ChildBase;
  UINT64        UefiCpuBase;
  UINT64        Size;
  UINT64        WindowSize;

  ZeroMem (Root, sizeof (*Root));
  KmhBoscUnavailableAperture (&Root->Mem);
  KmhBoscUnavailableAperture (&Root->MemAbove4G);

  Parent = fdt_parent_offset (Fdt, Node);
  if (Parent < 0) {
    Parent = 0;
  }

  ParentAddressCells = KmhBoscGetNumCells (Fdt, Parent, "#address-cells", 2);
  ParentSizeCells = KmhBoscGetNumCells (Fdt, Parent, "#size-cells", 2);
  AddressCells = KmhBoscGetNumCells (Fdt, Node, "#address-cells", 3);
  RangeSizeCells = KmhBoscGetNumCells (Fdt, Node, "#size-cells", 2);
  if ((ParentAddressCells <= 0) || (ParentSizeCells <= 0) ||
      (AddressCells != 3) || (RangeSizeCells <= 0))
  {
    DEBUG ((DEBUG_WARN, "%a: unsupported PCIe DT cell layout\n", __func__));
    return FALSE;
  }

  Prop = fdt_getprop (Fdt, Node, "linux,pci-domain", &Len);
  if ((Prop == NULL) || (Len != sizeof (UINT32))) {
    DEBUG ((DEBUG_WARN, "%a: missing linux,pci-domain\n", __func__));
    return FALSE;
  }

  Root->Segment = fdt32_to_cpu (*Prop);

  Prop = fdt_getprop (Fdt, Node, "bus-range", &Len);
  if ((Prop == NULL) || (Len != 2 * sizeof (UINT32))) {
    DEBUG ((DEBUG_WARN, "%a: missing bus-range\n", __func__));
    return FALSE;
  }

  Root->BusBase = (UINT8)fdt32_to_cpu (Prop[0]);
  Root->BusLimit = (UINT8)fdt32_to_cpu (Prop[1]);
  if (Root->BusBase > Root->BusLimit) {
    return FALSE;
  }

  RegEntryCells = ParentAddressCells + ParentSizeCells;
  Prop = fdt_getprop (Fdt, Node, "reg", &Len);
  if ((Prop == NULL) || (Len < 2 * RegEntryCells * (INT32)sizeof (UINT32))) {
    DEBUG ((DEBUG_WARN, "%a: missing reg\n", __func__));
    return FALSE;
  }

  Cells = Prop;
  Root->DbiBase = KmhBoscReadFdtCells (&Cells, ParentAddressCells);
  Root->DbiSize = KmhBoscReadFdtCells (&Cells, ParentSizeCells);
  Root->CfgBase = KmhBoscReadFdtCells (&Cells, ParentAddressCells);
  Root->CfgSize = KmhBoscReadFdtCells (&Cells, ParentSizeCells);
  if ((Root->DbiSize == 0) || (Root->CfgSize == 0)) {
    return FALSE;
  }

  RangeEntryCells = AddressCells + ParentAddressCells + RangeSizeCells;
  Prop = fdt_getprop (Fdt, Node, "ranges", &Len);
  if ((Prop == NULL) || (Len <= 0) ||
      ((Len % (RangeEntryCells * (INT32)sizeof (UINT32))) != 0))
  {
    DEBUG ((DEBUG_WARN, "%a: missing ranges\n", __func__));
    return FALSE;
  }

  Cells = Prop;
  while ((UINTN)((CONST UINT8 *)Cells - (CONST UINT8 *)Prop) < (UINTN)Len) {
    RangeType = fdt32_to_cpu (Cells[0]) & FDT_PCI_RANGE_TYPE_MASK;
    ChildBase = ((UINT64)fdt32_to_cpu (Cells[1]) << 32) |
                fdt32_to_cpu (Cells[2]);
    Cells += AddressCells;
    KmhBoscReadFdtCells (&Cells, ParentAddressCells);
    Size = KmhBoscReadFdtCells (&Cells, RangeSizeCells);
    WindowSize = KmhBoscClampUefiWindowSize (Size);
    if ((WindowSize == 0) || (ChildBase > MAX_UINT64 - WindowSize)) {
      continue;
    }

    if (RangeType == FDT_PCI_RANGE_MMIO) {
      UefiCpuBase = KmhBoscUefiCpuWindowBase (Root->Segment, FALSE);
      Root->Mem.Base = ChildBase;
      Root->Mem.Limit = ChildBase + WindowSize - 1;
      Root->Mem.Translation = ChildBase - UefiCpuBase;
    } else if (RangeType == FDT_PCI_RANGE_MMIO_64BIT) {
      UefiCpuBase = KmhBoscUefiCpuWindowBase (Root->Segment, TRUE);
      Root->MemAbove4G.Base = ChildBase;
      Root->MemAbove4G.Limit = ChildBase + WindowSize - 1;
      Root->MemAbove4G.Translation = ChildBase - UefiCpuBase;
    }
  }

  KmhBoscProgramMemAtuWindows (Root);

  DEBUG ((
    DEBUG_INFO,
    "%a: segment %u dbi 0x%Lx cfg 0x%Lx bus %u-%u mem 0x%Lx-0x%Lx mem64 0x%Lx-0x%Lx\n",
    __func__,
    Root->Segment,
    Root->DbiBase,
    Root->CfgBase,
    Root->BusBase,
    Root->BusLimit,
    Root->Mem.Base,
    Root->Mem.Limit,
    Root->MemAbove4G.Base,
    Root->MemAbove4G.Limit
    ));

  return TRUE;
}

STATIC
VOID
KmhBoscDiscoverPcieRoots (
  VOID
  )
{
  VOID   *Fdt;
  INT32  Node;

  if (mRootInfoValid) {
    return;
  }

  mRootInfoCount = 0;
  Fdt = KmhBoscGetFdt ();
  if ((Fdt == NULL) || (fdt_check_header (Fdt) != 0)) {
    DEBUG ((DEBUG_WARN, "%a: no valid FDT HOB\n", __func__));
    mRootInfoValid = TRUE;
    return;
  }

  for (Node = fdt_next_node (Fdt, -1, NULL);
       Node >= 0;
       Node = fdt_next_node (Fdt, Node, NULL))
  {
    if (fdt_node_check_compatible (Fdt, Node, KMH_BOSC_PCIE_COMPATIBLE) != 0) {
      continue;
    }

    if (!KmhBoscFdtNodeIsOkay (Fdt, Node)) {
      continue;
    }

    if (mRootInfoCount >= ARRAY_SIZE (mRootInfo)) {
      DEBUG ((DEBUG_WARN, "%a: too many PCIe roots, ignoring the rest\n", __func__));
      break;
    }

    if (KmhBoscParsePcieNode (Fdt, Node, &mRootInfo[mRootInfoCount])) {
      mRootInfoCount++;
    }
  }

  mRootInfoValid = TRUE;
}

STATIC
EFI_DEVICE_PATH_PROTOCOL *
KmhBoscCreateRootDevicePath (
  IN UINT32  Segment
  )
{
  KMH_BOSC_PCI_ROOT_DEVICE_PATH  *DevicePath;

  DevicePath = AllocateCopyPool (
                 sizeof (mRootBridgeDevicePathTemplate),
                 &mRootBridgeDevicePathTemplate
                 );
  if (DevicePath == NULL) {
    return NULL;
  }

  DevicePath->AcpiDevicePath.UID = Segment;
  return (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
}

PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  OUT UINTN  *Count
  )
{
  PCI_ROOT_BRIDGE           *Bridges;
  PCI_ROOT_BRIDGE_APERTURE  Io;
  PCI_ROOT_BRIDGE_APERTURE  PMem;
  PCI_ROOT_BRIDGE_APERTURE  PMemAbove4G;
  UINTN                     Index;

  KmhBoscDiscoverPcieRoots ();
  if (mRootInfoCount == 0) {
    *Count = 0;
    return NULL;
  }

  Bridges = AllocateZeroPool (mRootInfoCount * sizeof (*Bridges));
  if (Bridges == NULL) {
    *Count = 0;
    return NULL;
  }

  KmhBoscUnavailableAperture (&Io);
  KmhBoscUnavailableAperture (&PMem);
  KmhBoscUnavailableAperture (&PMemAbove4G);

  for (Index = 0; Index < mRootInfoCount; Index++) {
    Bridges[Index].Segment = mRootInfo[Index].Segment;
    Bridges[Index].Supports = 0;
    Bridges[Index].Attributes = 0;
    Bridges[Index].DmaAbove4G = TRUE;
    Bridges[Index].NoExtendedConfigSpace = FALSE;
    Bridges[Index].ResourceAssigned = FALSE;
    Bridges[Index].AllocationAttributes = EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
    Bridges[Index].Bus.Base = mRootInfo[Index].BusBase;
    Bridges[Index].Bus.Limit = mRootInfo[Index].BusLimit;
    Bridges[Index].Bus.Translation = 0;
    CopyMem (&Bridges[Index].Io, &Io, sizeof (Io));
    CopyMem (&Bridges[Index].Mem, &mRootInfo[Index].Mem, sizeof (mRootInfo[Index].Mem));
    CopyMem (
      &Bridges[Index].MemAbove4G,
      &mRootInfo[Index].MemAbove4G,
      sizeof (mRootInfo[Index].MemAbove4G)
      );
    CopyMem (&Bridges[Index].PMem, &PMem, sizeof (PMem));
    CopyMem (&Bridges[Index].PMemAbove4G, &PMemAbove4G, sizeof (PMemAbove4G));
    Bridges[Index].DevicePath = KmhBoscCreateRootDevicePath (mRootInfo[Index].Segment);
    if (Bridges[Index].DevicePath == NULL) {
      PciHostBridgeFreeRootBridges (Bridges, Index);
      *Count = 0;
      return NULL;
    }
  }

  *Count = mRootInfoCount;
  return Bridges;
}

VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  IN PCI_ROOT_BRIDGE  *Bridges,
  IN UINTN            Count
  )
{
  UINTN  Index;

  if (Bridges == NULL) {
    return;
  }

  for (Index = 0; Index < Count; Index++) {
    if (Bridges[Index].DevicePath != NULL) {
      FreePool (Bridges[Index].DevicePath);
    }
  }

  FreePool (Bridges);
}

VOID
EFIAPI
PciHostBridgeResourceConflict (
  IN EFI_HANDLE  HostBridgeHandle,
  IN VOID        *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  BOOLEAN                            IsPrefetchable;

  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    for ( ; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (Descriptor->ResType <
              ARRAY_SIZE (mPciHostBridgeLibAcpiAddressSpaceTypeStr));
      DEBUG ((
        DEBUG_INFO,
        " %s: Length/Alignment = 0x%lx / 0x%lx\n",
        mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
        Descriptor->AddrLen,
        Descriptor->AddrRangeMax
        ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
        IsPrefetchable = (Descriptor->SpecificFlag &
                          EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0;
        DEBUG ((
          DEBUG_INFO,
          "     Granularity/SpecificFlag = %ld / %02x%s\n",
          Descriptor->AddrSpaceGranularity,
          Descriptor->SpecificFlag,
          IsPrefetchable ? L" (Prefetchable)" : L""
          ));
      }
    }

    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(
                   (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                   );
  }
}
