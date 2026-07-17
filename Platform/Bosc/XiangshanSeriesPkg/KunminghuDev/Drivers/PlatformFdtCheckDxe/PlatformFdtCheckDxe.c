/** @file
  Lightweight FDT consistency checker for KunminghuDev.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/FdtClient.h>

#define KMH_PCI_RANGE_TYPE_MEM32      0x02000000U
#define KMH_PCI_RANGE_TYPE_MEM64      0x03000000U
#define KMH_PCI_RANGE_TYPE_MASK       0x03000000U
#define KMH_PCIE_RC0_DBI_BASE         0x32000000ULL
#define KMH_PCIE_CFG0_SIZE            0x00100000ULL
#define KMH_BOOTARGS_CHUNK_SIZE       96

STATIC CONST EFI_GUID mKmhFdtBootargsGuid = {
  0x2f4c6f91, 0x8d2a, 0x4a44, { 0x9b, 0x5d, 0x48, 0x7d, 0x2d, 0x35, 0xa4, 0x17 }
};

typedef struct {
  UINT32     BusMin;
  UINT32     BusMax;
  BOOLEAN    FoundBusRange;
  BOOLEAN    FoundMmio32;
  UINT64     Mmio32CpuBase;
  UINT64     Mmio32PciBase;
  UINT64     Mmio32Size;
  BOOLEAN    FoundMmio64;
  UINT64     Mmio64CpuBase;
  UINT64     Mmio64PciBase;
  UINT64     Mmio64Size;
  BOOLEAN    FoundDbiBase;
  UINT64     DbiBase;
} KMH_FDT_PCIE_INFO;

STATIC
UINT64
KmhFdtReadCells (
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
BOOLEAN
KmhFdtStringEquals (
  IN CONST VOID  *Property,
  IN UINT32      PropertySize,
  IN CONST CHAR8 *String
  )
{
  UINTN  StringSize;

  if (Property == NULL) {
    return FALSE;
  }

  StringSize = AsciiStrLen (String) + 1;
  if (PropertySize < StringSize) {
    return FALSE;
  }

  return CompareMem (Property, String, StringSize) == 0;
}

STATIC
VOID
KmhFdtPrintBootargsChunks (
  IN CONST CHAR8  *Bootargs,
  IN UINT32       BootargsSize
  )
{
  CHAR8  Chunk[KMH_BOOTARGS_CHUNK_SIZE + 1];
  UINTN  Offset;
  UINTN  CopySize;
  UINTN  TextSize;

  if ((Bootargs == NULL) || (BootargsSize == 0)) {
    return;
  }

  TextSize = AsciiStrnLenS (Bootargs, BootargsSize);
  for (Offset = 0; Offset < TextSize; Offset += KMH_BOOTARGS_CHUNK_SIZE) {
    CopySize = TextSize - Offset;
    if (CopySize > KMH_BOOTARGS_CHUNK_SIZE) {
      CopySize = KMH_BOOTARGS_CHUNK_SIZE;
    }

    CopyMem (Chunk, Bootargs + Offset, CopySize);
    Chunk[CopySize] = '\0';
    DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: /chosen/bootargs[%u]=\"%a\"\n", Offset, Chunk));
  }
}

STATIC
VOID
KmhFdtPublishBootargsVariable (
  IN CONST CHAR8  *Bootargs,
  IN UINT32       BootargsSize
  )
{
  EFI_STATUS  Status;

  if ((Bootargs == NULL) || (BootargsSize == 0)) {
    return;
  }

  Status = gRT->SetVariable (
                  L"KmhFdtBootargs",
                  (EFI_GUID *)&mKmhFdtBootargsGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  BootargsSize,
                  (VOID *)Bootargs
                  );
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_WARN : DEBUG_INFO,
    "KMH-FDT-CHECK: publish KmhFdtBootargs variable size=%u status=%r\n",
    BootargsSize,
    Status
    ));
}

STATIC
VOID
KmhFdtCheckBootargs (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS   Status;
  INT32        ChosenNode;
  CONST CHAR8  *Bootargs;
  UINT32       BootargsSize;

  Status = FdtClient->GetOrInsertChosenNode (FdtClient, &ChosenNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: /chosen node not available: %r\n", Status));
    return;
  }

  Status = FdtClient->GetNodeProperty (
                        FdtClient,
                        ChosenNode,
                        "bootargs",
                        (CONST VOID **)&Bootargs,
                        &BootargsSize
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: /chosen/bootargs not found: %r\n", Status));
    return;
  }

  DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: /chosen node=%d bootargs-size=%u\n", ChosenNode, BootargsSize));
  KmhFdtPrintBootargsChunks (Bootargs, BootargsSize);
  KmhFdtPublishBootargsVariable (Bootargs, BootargsSize);
}

STATIC
VOID
KmhFdtCheckMemory (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS    FindStatus;
  INT32         Node;
  CONST UINT32  *Reg;
  UINT32        RegSize;
  UINTN         AddressCells;
  UINTN         SizeCells;
  UINTN         EntryCells;
  UINTN         Offset;
  UINTN         MemoryNodeCount;

  MemoryNodeCount = 0;

  for (FindStatus = FdtClient->FindMemoryNodeReg (
                                  FdtClient,
                                  &Node,
                                  (CONST VOID **)&Reg,
                                  &AddressCells,
                                  &SizeCells,
                                  &RegSize
                                  );
       !EFI_ERROR (FindStatus);
       FindStatus = FdtClient->FindNextMemoryNodeReg (
                                  FdtClient,
                                  Node,
                                  &Node,
                                  (CONST VOID **)&Reg,
                                  &AddressCells,
                                  &SizeCells,
                                  &RegSize
                                  ))
  {
    EntryCells = AddressCells + SizeCells;
    if ((EntryCells == 0) || ((RegSize % (EntryCells * sizeof (UINT32))) != 0)) {
      DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: memory node %d has invalid reg size=%u addr-cells=%u size-cells=%u\n", Node, RegSize, AddressCells, SizeCells));
      continue;
    }

    for (Offset = 0; Offset < RegSize / sizeof (UINT32); Offset += EntryCells) {
      UINT64  Base;
      UINT64  Size;

      Base = KmhFdtReadCells (&Reg[Offset], AddressCells);
      Size = KmhFdtReadCells (&Reg[Offset + AddressCells], SizeCells);
      DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: memory[%u] node=%d base=0x%Lx size=0x%Lx\n", MemoryNodeCount, Node, Base, Size));
      MemoryNodeCount++;
    }
  }

  if (MemoryNodeCount == 0) {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: no FDT memory node found\n"));
  }
}

STATIC
VOID
KmhFdtCheckCpuCount (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS  Status;
  INT32       Node;
  UINTN       CpuCount;
  CONST VOID  *Property;
  UINT32      PropertySize;

  CpuCount = 0;

  for (Status = FdtClient->FindCompatibleNode (FdtClient, "riscv", &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, "riscv", Node, &Node))
  {
    Status = FdtClient->GetNodeProperty (FdtClient, Node, "device_type", &Property, &PropertySize);
    if (!EFI_ERROR (Status) && KmhFdtStringEquals (Property, PropertySize, "cpu")) {
      CpuCount++;
    }
  }

  if (CpuCount == 0) {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: no RISC-V CPU node found\n"));
  } else {
    DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: cpu-count=%u\n", CpuCount));
  }
}

STATIC
VOID
KmhFdtPrintPcieAcpiCandidate (
  IN INT32                    Node,
  IN CONST KMH_FDT_PCIE_INFO  *Info
  )
{
  UINT64  Mmio32Translation;
  UINT64  Mmio64Translation;
  UINT64  McfgBase;

  if ((Info == NULL) || !Info->FoundDbiBase || (Info->DbiBase != KMH_PCIE_RC0_DBI_BASE)) {
    return;
  }

  Mmio32Translation = Info->FoundMmio32 ? Info->Mmio32CpuBase - Info->Mmio32PciBase : 0;
  Mmio64Translation = Info->FoundMmio64 ? Info->Mmio64CpuBase - Info->Mmio64PciBase : 0;
  McfgBase          = Info->FoundMmio32 ? Info->Mmio32CpuBase + Info->Mmio32Size : 0;

  DEBUG ((
    DEBUG_INFO,
    "KMH-DT-ACPI: PCIe RC0 node=%d MCFG base=0x%Lx segment=0 bus=%u-%u cfg0-size=0x%Lx\n",
    Node,
    McfgBase,
    Info->FoundBusRange ? Info->BusMin : 0,
    Info->FoundBusRange ? Info->BusMax : 0,
    KMH_PCIE_CFG0_SIZE
    ));

  if (Info->FoundBusRange) {
    DEBUG ((
      DEBUG_INFO,
      "KMH-DT-ACPI: PCIe RC0 _CRS WordBusNumber min=0x%x max=0x%x len=0x%x\n",
      Info->BusMin,
      Info->BusMax,
      Info->BusMax - Info->BusMin + 1
      ));
  }

  if (Info->FoundMmio32) {
    DEBUG ((
      DEBUG_INFO,
      "KMH-DT-ACPI: PCIe RC0 _CRS DWordMemory pci=0x%Lx-0x%Lx cpu=0x%Lx-0x%Lx trans=0x%Lx len=0x%Lx\n",
      Info->Mmio32PciBase,
      Info->Mmio32PciBase + Info->Mmio32Size - 1,
      Info->Mmio32CpuBase,
      Info->Mmio32CpuBase + Info->Mmio32Size - 1,
      Mmio32Translation,
      Info->Mmio32Size
      ));
  }

  if (Info->FoundMmio64) {
    DEBUG ((
      DEBUG_INFO,
      "KMH-DT-ACPI: PCIe RC0 _CRS QWordMemory pci=0x%Lx-0x%Lx cpu=0x%Lx-0x%Lx trans=0x%Lx len=0x%Lx\n",
      Info->Mmio64PciBase,
      Info->Mmio64PciBase + Info->Mmio64Size - 1,
      Info->Mmio64CpuBase,
      Info->Mmio64CpuBase + Info->Mmio64Size - 1,
      Mmio64Translation,
      Info->Mmio64Size
      ));
  }

  DEBUG ((
    DEBUG_INFO,
    "KMH-DT-ACPI: PCIe RC0 current PCD MCFG=0x%Lx bus=%u-%u mmio32-cpu=0x%Lx size=0x%Lx trans=0x%Lx mmio64-cpu=0x%Lx size=0x%Lx\n",
    FixedPcdGet64 (PcdPciConfigBase),
    FixedPcdGet32 (PcdPciBusMin),
    FixedPcdGet32 (PcdPciBusMax),
    FixedPcdGet32 (PcdPciMmio32Base),
    FixedPcdGet32 (PcdPciMmio32Size),
    FixedPcdGet64 (PcdPciMmio32Translation),
    FixedPcdGet64 (PcdPciMmio64Base),
    FixedPcdGet64 (PcdPciMmio64Size)
    ));

  if ((McfgBase != 0) && (McfgBase != FixedPcdGet64 (PcdPciConfigBase))) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: PCIe RC0 MCFG base differs from current ACPI/PCD\n"));
  }

  if (Info->FoundMmio64 && (FixedPcdGet64 (PcdPciMmio64Size) == 0)) {
    DEBUG ((DEBUG_WARN, "KMH-DT-ACPI: PCIe RC0 DT has MEM64 but current ACPI/PCD disables MEM64\n"));
  }
}

STATIC
VOID
KmhFdtValidatePcieNode (
  IN FDT_CLIENT_PROTOCOL  *FdtClient,
  IN INT32                Node
  )
{
  EFI_STATUS         Status;
  CONST UINT32       *Property;
  UINT32             PropertySize;
  KMH_FDT_PCIE_INFO  Info;
  UINTN              RangeCells;
  UINTN              Offset;

  ZeroMem (&Info, sizeof (Info));

  Status = FdtClient->GetNodeProperty (FdtClient, Node, "reg", (CONST VOID **)&Property, &PropertySize);
  if (!EFI_ERROR (Status) && (PropertySize >= 4 * sizeof (UINT32))) {
    Info.DbiBase      = KmhFdtReadCells (Property, 2);
    Info.FoundDbiBase = TRUE;
  }

  Status = FdtClient->GetNodeProperty (FdtClient, Node, "bus-range", (CONST VOID **)&Property, &PropertySize);
  if (!EFI_ERROR (Status) && (PropertySize == 2 * sizeof (UINT32))) {
    Info.BusMin        = SwapBytes32 (Property[0]);
    Info.BusMax        = SwapBytes32 (Property[1]);
    Info.FoundBusRange = TRUE;
  }

  DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: PCIe node=%d dbi=0x%Lx mode=%a\n", Node, Info.DbiBase, (Info.FoundDbiBase && (Info.DbiBase == KMH_PCIE_RC0_DBI_BASE)) ? "strict-rc0" : "info-only"));

  Status = FdtClient->GetNodeProperty (FdtClient, Node, "ranges", (CONST VOID **)&Property, &PropertySize);
  if (!EFI_ERROR (Status)) {
    RangeCells = 7;
    if ((PropertySize % (RangeCells * sizeof (UINT32))) != 0) {
      DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: PCIe node=%d ranges has unexpected size=%u\n", Node, PropertySize));
    } else {
      for (Offset = 0; Offset < PropertySize / sizeof (UINT32); Offset += RangeCells) {
        UINT32  Type;
        UINT64  PciBase;
        UINT64  CpuBase;
        UINT64  Size;

        Type    = SwapBytes32 (Property[Offset]) & KMH_PCI_RANGE_TYPE_MASK;
        PciBase = KmhFdtReadCells (&Property[Offset + 1], 2);
        CpuBase = KmhFdtReadCells (&Property[Offset + 3], 2);
        Size    = KmhFdtReadCells (&Property[Offset + 5], 2);

        DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: PCIe node=%d range type=0x%x pci=0x%Lx cpu=0x%Lx size=0x%Lx\n", Node, Type, PciBase, CpuBase, Size));

        if ((Type == KMH_PCI_RANGE_TYPE_MEM32) && !Info.FoundMmio32) {
          Info.Mmio32PciBase = PciBase;
          Info.Mmio32CpuBase = CpuBase;
          Info.Mmio32Size    = Size;
          Info.FoundMmio32   = TRUE;
        } else if ((Type == KMH_PCI_RANGE_TYPE_MEM64) && !Info.FoundMmio64) {
          Info.Mmio64PciBase = PciBase;
          Info.Mmio64CpuBase = CpuBase;
          Info.Mmio64Size    = Size;
          Info.FoundMmio64   = TRUE;
        }
      }
    }
  }

  if (Info.FoundBusRange) {
    DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: PCIe node=%d bus-range=%u-%u PCD=%u-%u\n", Node, Info.BusMin, Info.BusMax, FixedPcdGet32 (PcdPciBusMin), FixedPcdGet32 (PcdPciBusMax)));
    if (Info.FoundDbiBase && (Info.DbiBase == KMH_PCIE_RC0_DBI_BASE) &&
        ((Info.BusMin != FixedPcdGet32 (PcdPciBusMin)) || (Info.BusMax != FixedPcdGet32 (PcdPciBusMax))))
    {
      DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: PCIe RC0 bus-range mismatch with ACPI/PCD\n"));
    }
  } else {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: PCIe node=%d has no bus-range\n", Node));
  }

  if (Info.FoundMmio32) {
    DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: PCIe node=%d mmio32 cpu=0x%Lx pci=0x%Lx size=0x%Lx PCD cpu=0x%Lx size=0x%Lx trans=0x%Lx\n", Node, Info.Mmio32CpuBase, Info.Mmio32PciBase, Info.Mmio32Size, FixedPcdGet32 (PcdPciMmio32Base), FixedPcdGet32 (PcdPciMmio32Size), FixedPcdGet64 (PcdPciMmio32Translation)));
    if (Info.FoundDbiBase && (Info.DbiBase == KMH_PCIE_RC0_DBI_BASE) &&
        ((Info.Mmio32CpuBase != FixedPcdGet32 (PcdPciMmio32Base)) ||
         (Info.Mmio32Size != FixedPcdGet32 (PcdPciMmio32Size))))
    {
      DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: PCIe RC0 MMIO32 mismatch with ACPI/PCD\n"));
    }
  } else {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: PCIe node=%d has no MEM32 range\n", Node));
  }

  if (Info.FoundMmio64) {
    DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: PCIe node=%d mmio64 cpu=0x%Lx pci=0x%Lx size=0x%Lx PCD cpu=0x%Lx size=0x%Lx\n", Node, Info.Mmio64CpuBase, Info.Mmio64PciBase, Info.Mmio64Size, FixedPcdGet64 (PcdPciMmio64Base), FixedPcdGet64 (PcdPciMmio64Size)));
  } else {
    DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: PCIe node=%d has no MEM64 range\n", Node));
  }

  KmhFdtPrintPcieAcpiCandidate (Node, &Info);
}

STATIC
VOID
KmhFdtCheckPcie (
  IN FDT_CLIENT_PROTOCOL  *FdtClient
  )
{
  EFI_STATUS  Status;
  INT32       Node;
  UINTN       PcieCount;

  PcieCount = 0;

  for (Status = FdtClient->FindCompatibleNode (FdtClient, "snps,dw-pcie", &Node);
       !EFI_ERROR (Status);
       Status = FdtClient->FindNextCompatibleNode (FdtClient, "snps,dw-pcie", Node, &Node))
  {
    PcieCount++;
    KmhFdtValidatePcieNode (FdtClient, Node);
  }

  if (PcieCount == 0) {
    for (Status = FdtClient->FindCompatibleNode (FdtClient, "pci-host-ecam-generic", &Node);
         !EFI_ERROR (Status);
         Status = FdtClient->FindNextCompatibleNode (FdtClient, "pci-host-ecam-generic", Node, &Node))
    {
      PcieCount++;
      KmhFdtValidatePcieNode (FdtClient, Node);
    }
  }

  if (PcieCount == 0) {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: no enabled PCIe node found\n"));
  }
}

EFI_STATUS
EFIAPI
PlatformFdtCheckEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS           Status;
  FDT_CLIENT_PROTOCOL  *FdtClient;

  Status = gBS->LocateProtocol (&gFdtClientProtocolGuid, NULL, (VOID **)&FdtClient);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "KMH-FDT-CHECK: FDT client not available: %r\n", Status));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: begin readonly DTB vs ACPI/PCD validation\n"));
  KmhFdtCheckBootargs (FdtClient);
  KmhFdtCheckMemory (FdtClient);
  KmhFdtCheckCpuCount (FdtClient);
  KmhFdtCheckPcie (FdtClient);
  DEBUG ((DEBUG_INFO, "KMH-FDT-CHECK: done\n"));

  return EFI_SUCCESS;
}
