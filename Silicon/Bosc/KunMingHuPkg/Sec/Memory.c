/** @file
  Memory Detection for SG2042 EVB.

  Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023, Academy of Intelligent Innovation, Shandong Universiy, China.P.R. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

  MemDetect.c

**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/ResourcePublicationLib.h>
#include <Register/RiscV64/RiscVEncoding.h>
#include <Library/PrePiLib.h>
#include <libfdt.h>
#include <Guid/FdtHob.h>

VOID
BuildMemoryTypeInformationHob (
  VOID
  );

/**
  Create memory range resource HOB using the memory base
  address and size.

  @param  MemoryBase     Memory range base address.
  @param  MemorySize     Memory range size.

**/
STATIC
VOID
AddMemoryBaseSizeHob (
  IN EFI_PHYSICAL_ADDRESS  MemoryBase,
  IN UINT64                MemorySize
  )
{
  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_TESTED,
    MemoryBase,
    MemorySize
    );
}

/**
  Create memory range resource HOB using memory base
  address and top address of the memory range.

  @param  MemoryBase     Memory range base address.
  @param  MemoryLimit    Memory range size.

**/
STATIC
VOID
AddMemoryRangeHob (
  IN EFI_PHYSICAL_ADDRESS  MemoryBase,
  IN EFI_PHYSICAL_ADDRESS  MemoryLimit
  )
{
  AddMemoryBaseSizeHob (MemoryBase, (UINT64)(MemoryLimit - MemoryBase));
}

/**
  Publish system RAM and reserve memory regions.

**/
STATIC
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

/** Get the number of cells for a given property

  @param[in]  Fdt   Pointer to Device Tree (DTB)
  @param[in]  Node  Node
  @param[in]  Name  Name of the property

  @return           Number of cells.
**/
STATIC
INT32
GetNumCells (
  IN VOID         *Fdt,
  IN INT32        Node,
  IN CONST CHAR8  *Name
  )
{
  CONST INT32  *Prop;
  INT32        Len;
  UINT32       Val;

  Prop = fdt_getprop (Fdt, Node, Name, &Len);
  if (Prop == NULL) {
    return Len;
  }

  if (Len != sizeof (*Prop)) {
    return -FDT_ERR_BADNCELLS;
  }

  Val = fdt32_to_cpu (*Prop);
  if (Val > FDT_MAX_NCELLS) {
    return -FDT_ERR_BADNCELLS;
  }

  return (INT32)Val;
}

STATIC
UINT64
ReadFdtCells (
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
BOOLEAN
IsMemoryNode (
  IN VOID   *Fdt,
  IN INT32  Node
  )
{
  CONST CHAR8  *Type;
  INT32        Len;

  Type = fdt_getprop (Fdt, Node, "device_type", &Len);
  return (Type != NULL) && (AsciiStrCmp (Type, "memory") == 0);
}

STATIC
BOOLEAN
GetParentAddressSizeCells (
  IN  VOID   *Fdt,
  IN  INT32  Node,
  OUT INT32  *AddressCells,
  OUT INT32  *SizeCells
  )
{
  INT32  Parent;

  Parent = fdt_parent_offset (Fdt, Node);
  if (Parent < 0) {
    Parent = 0;
  }

  *AddressCells = GetNumCells (Fdt, Parent, "#address-cells");
  if (*AddressCells <= 0) {
    *AddressCells = 2;
  }

  *SizeCells = GetNumCells (Fdt, Parent, "#size-cells");
  if (*SizeCells <= 0) {
    *SizeCells = 1;
  }

  return (*AddressCells > 0) && (*SizeCells > 0);
}

/** Mark reserved memory ranges in the EFI memory map

 * As per DT spec v0.4 Section 3.5.4,
 * "Reserved regions with the no-map property must be listed in the
 * memory map with type EfiReservedMemoryType. All other reserved
 * regions must be listed with type EfiBootServicesData."

  @param FdtPointer Pointer to FDT

**/
STATIC
VOID
AddChosenPayloadMemoryMap (
  IN VOID  *FdtPointer
  );

STATIC
VOID
AddReservedMemoryMap (
  IN VOID  *FdtPointer
  )
{
  CONST INT32           *RegProp;
  INT32                 Node;
  INT32                 SubNode;
  INT32                 Len;
  EFI_PHYSICAL_ADDRESS  Addr;
  UINT64                Size;
  INTN                  NumRsv, i;
  INT32                 NumAddrCells, NumSizeCells;

  NumRsv = fdt_num_mem_rsv (FdtPointer);

  /* Look for an existing entry and add it to the efi mem map. */
  for (i = 0; i < NumRsv; i++) {
    if (fdt_get_mem_rsv (FdtPointer, i, &Addr, &Size) != 0) {
      continue;
    }

    BuildMemoryAllocationHob (
      Addr,
      Size,
      EfiReservedMemoryType
      );
  }

  /* process reserved-memory */
  Node = fdt_subnode_offset (FdtPointer, 0, "reserved-memory");
  if (Node >= 0) {
    NumAddrCells = GetNumCells (FdtPointer, Node, "#address-cells");
    if (NumAddrCells <= 0) {
      return;
    }

    NumSizeCells = GetNumCells (FdtPointer, Node, "#size-cells");
    if (NumSizeCells <= 0) {
      return;
    }

    fdt_for_each_subnode (SubNode, FdtPointer, Node) {
      RegProp = fdt_getprop (FdtPointer, SubNode, "reg", &Len);

      if ((RegProp != 0) && (Len == ((NumAddrCells + NumSizeCells) * sizeof (INT32)))) {
        Addr = fdt32_to_cpu (RegProp[0]);

        if (NumAddrCells > 1) {
          Addr = (Addr << 32) | fdt32_to_cpu (RegProp[1]);
        }

        RegProp += NumAddrCells;
        Size     = fdt32_to_cpu (RegProp[0]);

        if (NumSizeCells > 1) {
          Size = (Size << 32) | fdt32_to_cpu (RegProp[1]);
        }

        DEBUG ((
          DEBUG_INFO,
          "%a: Adding Reserved Memory Addr = 0x%llx, Size = 0x%llx\n",
          __func__,
          Addr,
          Size
          ));

        // OpenSBI 1.3/1.3.1 should be used which fixed its no-map issue.
        if (fdt_getprop (FdtPointer, SubNode, "no-map", &Len)) {
          BuildMemoryAllocationHob (
            Addr,
            Size,
            EfiReservedMemoryType
            );
        } else {
          BuildMemoryAllocationHob (
            Addr,
            Size,
            EfiBootServicesData
            );
        }
      }
    }
  }

  AddChosenPayloadMemoryMap (FdtPointer);
}

STATIC
BOOLEAN
ReadChosenU64Property (
  IN  VOID         *FdtPointer,
  IN  INT32        ChosenNode,
  IN  CONST CHAR8  *PropertyName,
  OUT UINT64       *Value
  )
{
  CONST UINT32  *Property;
  INT32         Len;

  if ((FdtPointer == NULL) || (PropertyName == NULL) || (Value == NULL)) {
    return FALSE;
  }

  Property = fdt_getprop (FdtPointer, ChosenNode, PropertyName, &Len);
  if (Property == NULL) {
    return FALSE;
  }

  if (Len == sizeof (UINT64)) {
    *Value = LShiftU64 (fdt32_to_cpu (Property[0]), 32) | fdt32_to_cpu (Property[1]);
    return TRUE;
  }

  if (Len == sizeof (UINT32)) {
    *Value = fdt32_to_cpu (Property[0]);
    return TRUE;
  }

  DEBUG ((DEBUG_WARN, "%a: /chosen/%a has invalid size %d\n", __func__, PropertyName, Len));
  return FALSE;
}

STATIC
BOOLEAN
ReadChosenU32Property (
  IN  VOID         *FdtPointer,
  IN  INT32        ChosenNode,
  IN  CONST CHAR8  *PropertyName,
  OUT UINT32       *Value
  )
{
  CONST UINT32  *Property;
  INT32         Len;

  if ((FdtPointer == NULL) || (PropertyName == NULL) || (Value == NULL)) {
    return FALSE;
  }

  Property = fdt_getprop (FdtPointer, ChosenNode, PropertyName, &Len);
  if (Property == NULL) {
    return FALSE;
  }

  if (Len == sizeof (UINT32)) {
    *Value = fdt32_to_cpu (Property[0]);
    return TRUE;
  }

  DEBUG ((DEBUG_WARN, "%a: /chosen/%a has invalid size %d\n", __func__, PropertyName, Len));
  return FALSE;
}

STATIC
BOOLEAN
AddChosenPayloadRange (
  IN VOID         *FdtPointer,
  IN INT32        ChosenNode,
  IN CONST CHAR8  *Name,
  IN CONST CHAR8  *StartPropertyName,
  IN CONST CHAR8  *EndPropertyName,
  IN CONST CHAR8  *SizePropertyName
  )
{
  UINT64                Base;
  UINT64                End;
  UINT64                Size;
  EFI_PHYSICAL_ADDRESS  Addr;

  if ((Name == NULL) || (StartPropertyName == NULL) || (EndPropertyName == NULL) || (SizePropertyName == NULL)) {
    return FALSE;
  }

  if (!ReadChosenU64Property (FdtPointer, ChosenNode, StartPropertyName, &Base)) {
    return FALSE;
  }

  if (ReadChosenU64Property (FdtPointer, ChosenNode, EndPropertyName, &End)) {
    if (End <= Base) {
      DEBUG ((DEBUG_ERROR, "%a: invalid /chosen %a range base=0x%llx end=0x%llx\n", __func__, Name, Base, End));
      return FALSE;
    }

    Size = End - Base;
  } else if (!ReadChosenU64Property (FdtPointer, ChosenNode, SizePropertyName, &Size) || (Size == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: missing /chosen %a end/size\n", __func__, Name));
    return FALSE;
  }

  Addr = (EFI_PHYSICAL_ADDRESS)Base;
  DEBUG ((DEBUG_INFO, "%a: Adding /chosen payload %a Addr = 0x%llx, Size = 0x%llx\n", __func__, Name, Addr, Size));
  BuildMemoryAllocationHob (
    Addr,
    Size,
    EfiBootServicesData
    );
  return TRUE;
}

STATIC
BOOLEAN
IsFpgaMultistageBurnEnabled (
  IN VOID  *FdtPointer,
  IN INT32  ChosenNode
  )
{
  UINT32  Flag;

  if (!ReadChosenU32Property (FdtPointer, ChosenNode, "kmh,fpga-multistage-burn", &Flag)) {
    return FALSE;
  }

  return Flag != 0;
}

STATIC
VOID
AddChosenPayloadMemoryMap (
  IN VOID  *FdtPointer
  )
{
  INT32    ChosenNode;
  BOOLEAN  FoundInitrd;

  ChosenNode = fdt_path_offset (FdtPointer, "/chosen");
  if (ChosenNode < 0) {
    return;
  }

  if (!IsFpgaMultistageBurnEnabled (FdtPointer, ChosenNode)) {
    return;
  }

  AddChosenPayloadRange (
    FdtPointer,
    ChosenNode,
    "image3",
    "kmh,image3-start",
    "kmh,image3-end",
    "kmh,image3-size"
    );
  FoundInitrd = AddChosenPayloadRange (
    FdtPointer,
    ChosenNode,
    "image4 initrd",
    "linux,initrd-start",
    "linux,initrd-end",
    "linux,initrd-size"
    );
  if (!FoundInitrd) {
    AddChosenPayloadRange (
      FdtPointer,
      ChosenNode,
      "image4 initrd",
      "kmh,image4-start",
      "kmh,image4-end",
      "kmh,image4-size"
      );
  }
}

/**
  Initialize memory hob based on the DTB information.

  NOTE: The memory space size of SG2042 EVB is determined by the number
  and size of DDRs inserted on the board. There is an error with initializing
  the system ram space of each memory node separately using InitializeRamRegions,
  so InitializeRamRegions is only called once for total system ram initialization.

  @param  DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS        The memory hob added successfully.

**/
EFI_STATUS
MemoryPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  )
{
  CONST UINT32                *RegProp;
  CONST UINT32                *Cells;
  UINT64                      UefiMemoryBase;
  UINT64                      CurBase;
  UINT64                      CurSize;
  UINT64                      CurEnd;
  UINT64                      PublishBase;
  UINT64                      PublishSize;
  UINT64                      LowestMemBase;
  INT32                       Node;
  INT32                       Prev;
  INT32                       Len;
  INT32                       AddressCells;
  INT32                       SizeCells;
  INT32                       EntryCells;
  INT32                       EntrySize;
  INT32                       Index;
  INT32                       Entries;
  BOOLEAN                     FoundMemory;
  BOOLEAN                     PublishedMemory;

  UefiMemoryBase = (UINT64)FixedPcdGet32 (PcdTemporaryRamBase) + FixedPcdGet32 (PcdTemporaryRamSize) - SIZE_32MB;
  LowestMemBase = MAX_UINT64;
  FoundMemory = FALSE;

  // Find the boot memory node. Only this node is cropped for the UEFI image.
  for (Prev = 0; ; Prev = Node) {
    Node = fdt_next_node (DeviceTreeAddress, Prev, NULL);
    if (Node < 0) {
      break;
    }

    if (!IsMemoryNode (DeviceTreeAddress, Node)) {
      continue;
    }

    if (!GetParentAddressSizeCells (DeviceTreeAddress, Node, &AddressCells, &SizeCells)) {
      DEBUG ((DEBUG_ERROR, "%a: invalid FDT memory parent cells\n", __func__));
      continue;
    }

    EntryCells = AddressCells + SizeCells;
    EntrySize = EntryCells * sizeof (UINT32);
    RegProp = fdt_getprop (DeviceTreeAddress, Node, "reg", &Len);
    if ((RegProp == NULL) || (Len <= 0) || ((Len % EntrySize) != 0)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to parse FDT memory node\n", __func__));
      continue;
    }

    Cells = RegProp;
    Entries = Len / EntrySize;
    for (Index = 0; Index < Entries; Index++) {
      CurBase = ReadFdtCells (&Cells, AddressCells);
      CurSize = ReadFdtCells (&Cells, SizeCells);
      if ((CurSize == 0) || (CurBase > MAX_UINT64 - CurSize)) {
        continue;
      }

      if (CurBase < LowestMemBase) {
        LowestMemBase = CurBase;
        FoundMemory = TRUE;
      }
    }
  }

  if (!FoundMemory) {
    DEBUG ((DEBUG_ERROR, "%a: no usable FDT memory node found\n", __func__));
    return EFI_NOT_FOUND;
  }

  PublishedMemory = FALSE;
  for (Prev = 0; ; Prev = Node) {
    Node = fdt_next_node (DeviceTreeAddress, Prev, NULL);
    if (Node < 0) {
      break;
    }

    if (!IsMemoryNode (DeviceTreeAddress, Node)) {
      continue;
    }

    if (!GetParentAddressSizeCells (DeviceTreeAddress, Node, &AddressCells, &SizeCells)) {
      continue;
    }

    EntryCells = AddressCells + SizeCells;
    EntrySize = EntryCells * sizeof (UINT32);
    RegProp = fdt_getprop (DeviceTreeAddress, Node, "reg", &Len);
    if ((RegProp == NULL) || (Len <= 0) || ((Len % EntrySize) != 0)) {
      continue;
    }

    Cells = RegProp;
    Entries = Len / EntrySize;
    for (Index = 0; Index < Entries; Index++) {
      CurBase = ReadFdtCells (&Cells, AddressCells);
      CurSize = ReadFdtCells (&Cells, SizeCells);
      if ((CurSize == 0) || (CurBase > MAX_UINT64 - CurSize)) {
        continue;
      }

      CurEnd = CurBase + CurSize;
      PublishBase = CurBase;
      PublishSize = CurSize;

      if (CurBase == LowestMemBase) {
        if (UefiMemoryBase >= CurEnd) {
          DEBUG ((DEBUG_ERROR, "%a: UEFI memory base is outside boot memory\n", __func__));
          continue;
        }

        if (UefiMemoryBase > CurBase) {
          PublishBase = UefiMemoryBase;
          PublishSize = CurEnd - UefiMemoryBase;
        }
      }

      if (PublishSize == 0) {
        continue;
      }

      DEBUG ((
        DEBUG_INFO,
        "%a: System RAM @ 0x%lx - 0x%lx\n",
        __func__,
        PublishBase,
        PublishBase + PublishSize - 1
        ));

      InitializeRamRegions (PublishBase, PublishSize);
      PublishedMemory = TRUE;
    }
  }

  if (!PublishedMemory) {
    return EFI_NOT_FOUND;
  }

  AddReservedMemoryMap (DeviceTreeAddress);

  /* Make sure SEC is booting with bare mode */
  ASSERT ((RiscVGetSupervisorAddressTranslationRegister () & SATP64_MODE) == (SATP_MODE_OFF << SATP64_MODE_SHIFT));

  BuildMemoryTypeInformationHob ();

  return EFI_SUCCESS;
}
