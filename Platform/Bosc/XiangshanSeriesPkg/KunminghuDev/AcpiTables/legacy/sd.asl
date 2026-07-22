#include <IndustryStandard/Acpi60.h>

Device (SDH0) {
    Name (_HID, "PRP0001")
    Name (_CRS, ResourceTemplate () {
        Memory32Fixed(ReadWrite, 0x30050000, 0x10000)
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 55 }
    })
    Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
            Package () { "compatible", "snps,sdhci" },
            Package () { "clock-frequency", 50000000 },
            Package () { "max_req_size", 4096},
            Package () { "bus-width", 4},
	}
    })
}
