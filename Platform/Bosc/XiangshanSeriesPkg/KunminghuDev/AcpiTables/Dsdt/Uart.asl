#include <IndustryStandard/Acpi60.h>

Device (COM0)
{
    Name (_HID, "RSCV0003")  // _HID: Hardware ID
    Name (_UID, Zero)  // _UID: Unique ID
    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
    {
        Memory32Fixed (ReadWrite,
            0x310b0000,         // Address Base
            0x00000100,         // Address Length
            )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
        {
            0x0000000A,
        }
    })
    Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
    {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
        Package ()
        {
            Package (0x02) {"clock-frequency", 0x50000000},
            Package (0x02) {"reg-shift", 2},
            Package (0x02) {"reg-io-width", 4},
            Package (0x02) {"current-speed", 115200},
        }
    })
}
