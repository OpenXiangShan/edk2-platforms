#include <IndustryStandard/Acpi60.h>
        
Device (IC00) {
    Name (_HID, "RSCV0002")  // _HID: Hardware ID
    Name (_UID, Zero)  // _UID: Unique ID
    Name (_GSB, Zero)  // _GSB: Global System Interrupt Base
    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
    {
        Memory32Fixed (ReadWrite,
            0x31120000,         // Address Base
            0x00008000,         // Address Length
            )
    })
}
