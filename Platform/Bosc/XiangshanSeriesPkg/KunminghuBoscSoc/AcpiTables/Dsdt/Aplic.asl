#include <IndustryStandard/Acpi60.h>

#define APLIC_DEVICE(DevName, Uid, GsiBase, Base, End)    \
Device (DevName) {                                        \
    Name (_HID, "RSCV0002")                               \
    Name (_UID, Uid)                                      \
    Name (_GSB, GsiBase)                                  \
    Name (_CRS, ResourceTemplate () {                     \
        QWordMemory (ResourceConsumer, PosDecode,         \
            MinFixed, MaxFixed, NonCacheable, ReadWrite,  \
            0x0000000000000000,                           \
            Base,                                         \
            End,                                          \
            0x0000000000000000,                           \
            0x0000000000004000)                           \
    })                                                    \
}

APLIC_DEVICE (IC00, 0,   0, 0x000000001E024000, 0x000000001E027FFF)
APLIC_DEVICE (IC01, 1,  96, 0x000010001E024000, 0x000010001E027FFF)
APLIC_DEVICE (IC02, 2, 192, 0x000020001E024000, 0x000020001E027FFF)
APLIC_DEVICE (IC03, 3, 288, 0x000030001E024000, 0x000030001E027FFF)
