#include <IndustryStandard/Acpi60.h>

Device (COM0) {
    Name (_HID, "PRP0001")
    Name (_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x310B0000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 40 }
    })
    Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
            Package () { "compatible", "ns16550a" },
            Package () { "clock-frequency", 50000000 },
            Package () { "reg-shift", 2 },
            Package () { "reg-io-width", 4 },
        }
    })
}
