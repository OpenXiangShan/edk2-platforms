/** @file
  PCI host bridge library instance for NanHuDev SOC.

  Copyright (C) 2020, Phytium Technology Co Ltd. All rights reserved.<BR>
  Copyright (c) 2024, Bosc. All rights reserved.<BR>ved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Library/IoLib.h>

#pragma pack(1)

typedef struct {
  ACPI_HID_DEVICE_PATH     AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;

#pragma pack ()

#define BIT(nr)         (1UL << (nr))
#define GENMASK(h, l) \
    (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

/* Register definitions */
#define XILINX_PCIE_REG_VSEC		0x0000012c
#define XILINX_PCIE_REG_BIR		0x00000130
#define XILINX_PCIE_REG_IDR		0x00000138
#define XILINX_PCIE_REG_IMR		0x0000013c
#define XILINX_PCIE_REG_PSCR		0x00000144
#define XILINX_PCIE_REG_RPSC		0x00000148
#define XILINX_PCIE_REG_MSIBASE1	0x0000014c
#define XILINX_PCIE_REG_MSIBASE2	0x00000150
#define XILINX_PCIE_REG_RPEFR		0x00000154
#define XILINX_PCIE_REG_RPIFR1		0x00000158
#define XILINX_PCIE_REG_RPIFR2		0x0000015c
#define XILINX_PCIE_REG_IDRN            0x00000160
#define XILINX_PCIE_REG_IDRN_MASK       0x00000164
#define XILINX_PCIE_REG_MSI_LOW		0x00000170
#define XILINX_PCIE_REG_MSI_HI		0x00000174
#define XILINX_PCIE_REG_MSI_LOW_MASK	0x00000178
#define XILINX_PCIE_REG_MSI_HI_MASK	0x0000017c

/* Interrupt registers definitions */
#define XILINX_PCIE_INTR_LINK_DOWN	BIT(0)
#define XILINX_PCIE_INTR_HOT_RESET	BIT(3)
#define XILINX_PCIE_INTR_CFG_TIMEOUT	BIT(8)
#define XILINX_PCIE_INTR_CORRECTABLE	BIT(9)
#define XILINX_PCIE_INTR_NONFATAL	BIT(10)
#define XILINX_PCIE_INTR_FATAL		BIT(11)
#define XILINX_PCIE_INTR_INTX		BIT(16)
#define XILINX_PCIE_INTR_MSI		BIT(17)
#define XILINX_PCIE_INTR_SLV_UNSUPP	BIT(20)
#define XILINX_PCIE_INTR_SLV_UNEXP	BIT(21)
#define XILINX_PCIE_INTR_SLV_COMPL	BIT(22)
#define XILINX_PCIE_INTR_SLV_ERRP	BIT(23)
#define XILINX_PCIE_INTR_SLV_CMPABT	BIT(24)
#define XILINX_PCIE_INTR_SLV_ILLBUR	BIT(25)
#define XILINX_PCIE_INTR_MST_DECERR	BIT(26)
#define XILINX_PCIE_INTR_MST_SLVERR	BIT(27)
#define XILINX_PCIE_IMR_ALL_MASK	0x0FF30FE9
#define XILINX_PCIE_IDR_ALL_MASK	0xFFFFFFFF
#define XILINX_PCIE_IDRN_MASK           GENMASK(19, 16)

/* Root Port Error FIFO Read Register definitions */
#define XILINX_PCIE_RPEFR_ERR_VALID	BIT(18)
#define XILINX_PCIE_RPEFR_REQ_ID	GENMASK(15, 0)
#define XILINX_PCIE_RPEFR_ALL_MASK	0xFFFFFFFF

/* Root Port Interrupt FIFO Read Register 1 definitions */
#define XILINX_PCIE_RPIFR1_INTR_VALID	BIT(31)
#define XILINX_PCIE_RPIFR1_MSI_INTR	BIT(30)
#define XILINX_PCIE_RPIFR1_INTR_MASK	GENMASK(28, 27)
#define XILINX_PCIE_RPIFR1_ALL_MASK	0xFFFFFFFF
#define XILINX_PCIE_RPIFR1_INTR_SHIFT	27
#define XILINX_PCIE_IDRN_SHIFT          16
#define XILINX_PCIE_VSEC_REV_MASK	GENMASK(19, 16)
#define XILINX_PCIE_VSEC_REV_SHIFT	16
#define XILINX_PCIE_FIFO_SHIFT		5

/* Bridge Info Register definitions */
#define XILINX_PCIE_BIR_ECAM_SZ_MASK	GENMASK(18, 16)
#define XILINX_PCIE_BIR_ECAM_SZ_SHIFT	16

/* Root Port Interrupt FIFO Read Register 2 definitions */
#define XILINX_PCIE_RPIFR2_MSG_DATA	GENMASK(15, 0)

/* Root Port Status/control Register definitions */
#define XILINX_PCIE_REG_RPSC_BEN	BIT(0)

/* Phy Status/Control Register definitions */
#define XILINX_PCIE_REG_PSCR_LNKUP	BIT(11)

/* ECAM definitions */
#define ECAM_BUS_NUM_SHIFT		20
#define ECAM_DEV_NUM_SHIFT		12

/* Number of MSI IRQs */
#define XILINX_NUM_MSI_IRQS		64
#define INTX_NUM                        4

#define DMA_BRIDGE_BASE_OFF		0xCD8


#define END_DEVICE_PATH_DEF { END_DEVICE_PATH_TYPE, \
                              END_ENTIRE_DEVICE_PATH_SUBTYPE, \
                              { END_DEVICE_PATH_LENGTH, 0 } \
                            }

#define ACPI_DEVICE_PATH_DEF(UID) {{ ACPI_DEVICE_PATH, ACPI_DP, \
                                     { (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)), \
                                       (UINT8) (sizeof (ACPI_HID_DEVICE_PATH) >> 8)} \
                                     }, \
                                     EISA_PNP_ID (0x0A03), UID \
                                  }

STATIC CONST EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mEfiPciRootBridgeDevicePath[] = {
  {
    ACPI_DEVICE_PATH_DEF (0),
    END_DEVICE_PATH_DEF
  },
};

GLOBAL_REMOVE_IF_UNREFERENCED
CHAR16 *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

STATIC PCI_ROOT_BRIDGE mRootBridge = {
  0,                                              // Segment
  0,                                              // Supports
  0,                                              // Attributes
  FALSE,                                           // DmaAbove4G
  FALSE,                                          // NoExtendedConfigSpace
  FALSE,                                          // ResourceAssigned
  0,           // AllocationAttributes
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
    FixedPcdGet64 (PcdPciMmio64Base),
    FixedPcdGet64 (PcdPciMmio64Base) + FixedPcdGet64 (PcdPciMmio64Size) - 1
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
  /* Enable the Bridge enable bit */
  UINT64 PciConfigBase = FixedPcdGet64 (PcdPciConfigBase);
  UINT32 Rpsc = MmioRead32 (PciConfigBase + XILINX_PCIE_REG_RPSC);
  MmioWrite32 (PciConfigBase + XILINX_PCIE_REG_RPSC, Rpsc | XILINX_PCIE_REG_RPSC_BEN);
  DEBUG ((DEBUG_INFO, "PciHostBridgeGetRootBridges():%d PciConfigBase:0x%x Rpsc:0x%x XILINX_PCIE_REG_RPSC_BEN:0x%x\n", \
              __LINE__, PciConfigBase, Rpsc, Rpsc | XILINX_PCIE_REG_RPSC_BEN));

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
