#include <IndustryStandard/Acpi60.h>

#define PCIE_ECAM_SIZE             0x0000000010000000
#define PCIE_MEM_BUS_BASE          0x0000000100000000
#define PCIE_MEM_LIMIT             0x0000000101FFFFFF
#define PCIE_WINDOW_SIZE           0x0000000002000000

#define PCIE_ROOT(DevName, ResName, Seg, Pxm, EcamBase, EcamEnd, Mem32Base, Mem32End, MemCpuBase, IrqBase) \
Device (DevName) {                                                                 \
    Name (_HID, "PNP0A08")                                                         \
    Name (_CID, "PNP0A03")                                                         \
    Name (_SEG, Seg)                                                                \
    Name (_BBN, 0)                                                                  \
    Name (_UID, Seg)                                                                \
    Name (_CCA, 1)                                                                  \
    Name (_PXM, Pxm)                                                                \
    Method (_CBA, 0, Serialized) {                                                  \
        Return (EcamBase)                                                           \
    }                                                                               \
    Name (_PRT, Package () {                                                        \
        Package () { 0x0000FFFF, 0, Zero, IrqBase + 0 },                            \
        Package () { 0x0000FFFF, 1, Zero, IrqBase + 1 },                            \
        Package () { 0x0000FFFF, 2, Zero, IrqBase + 2 },                            \
        Package () { 0x0000FFFF, 3, Zero, IrqBase + 3 },                            \
    })                                                                              \
    Name (_CRS, ResourceTemplate () {                                               \
        WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,             \
            0, 0, 0xFF, 0, 0x100)                                                   \
        QWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed,               \
            NonCacheable, ReadWrite, 0, Mem32Base, Mem32End,                        \
            MemCpuBase - Mem32Base, PCIE_WINDOW_SIZE)                               \
        QWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed,               \
            NonCacheable, ReadWrite, 0, PCIE_MEM_BUS_BASE, PCIE_MEM_LIMIT,          \
            MemCpuBase + PCIE_WINDOW_SIZE - PCIE_MEM_BUS_BASE, PCIE_WINDOW_SIZE)    \
    })                                                                              \
    Device (ResName) {                                                              \
        Name (_HID, "PNP0C02")                                                      \
        Name (_UID, Seg)                                                            \
        Name (_CRS, ResourceTemplate () {                                           \
            QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed,           \
                NonCacheable, ReadWrite, 0, EcamBase,                               \
                EcamEnd, 0, PCIE_ECAM_SIZE)                                         \
        })                                                                          \
    }                                                                               \
}

PCIE_ROOT (P000, E000,  0, 0, 0x0000004800000000, 0x000000480FFFFFFF, 0x50000000, 0x51FFFFFF, 0x0000048000000000,  14)
PCIE_ROOT (P001, E001,  1, 0, 0x0000004900000000, 0x000000490FFFFFFF, 0x54000000, 0x55FFFFFF, 0x000004E000000000,  20)
PCIE_ROOT (P002, E002,  2, 0, 0x0000004A00000000, 0x0000004A0FFFFFFF, 0x58000000, 0x59FFFFFF, 0x000004F000000000,  26)
PCIE_ROOT (P003, E003,  3, 1, 0x0000104800000000, 0x000010480FFFFFFF, 0x5C000000, 0x5DFFFFFF, 0x0000148000000000, 110)
PCIE_ROOT (P004, E004,  4, 1, 0x0000104900000000, 0x000010490FFFFFFF, 0x60000000, 0x61FFFFFF, 0x000014E000000000, 116)
PCIE_ROOT (P005, E005,  5, 1, 0x0000104A00000000, 0x0000104A0FFFFFFF, 0x64000000, 0x65FFFFFF, 0x000014F000000000, 122)
PCIE_ROOT (P006, E006,  6, 2, 0x0000204800000000, 0x000020480FFFFFFF, 0x68000000, 0x69FFFFFF, 0x0000248000000000, 206)
PCIE_ROOT (P007, E007,  7, 2, 0x0000204900000000, 0x000020490FFFFFFF, 0x6C000000, 0x6DFFFFFF, 0x000024E000000000, 212)
PCIE_ROOT (P008, E008,  8, 2, 0x0000204A00000000, 0x0000204A0FFFFFFF, 0x70000000, 0x71FFFFFF, 0x000024F000000000, 218)
PCIE_ROOT (P009, E009,  9, 3, 0x0000304800000000, 0x000030480FFFFFFF, 0x74000000, 0x75FFFFFF, 0x0000348000000000, 302)
PCIE_ROOT (P00A, E00A, 10, 3, 0x0000304900000000, 0x000030490FFFFFFF, 0x78000000, 0x79FFFFFF, 0x000034E000000000, 308)
PCIE_ROOT (P00B, E00B, 11, 3, 0x0000304A00000000, 0x0000304A0FFFFFFF, 0x7C000000, 0x7DFFFFFF, 0x000034F000000000, 314)
