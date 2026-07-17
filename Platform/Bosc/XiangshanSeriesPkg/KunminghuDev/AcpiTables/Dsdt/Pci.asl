Scope(_SB)
{
  Device (PCI0) {
    Name (_HID, "PNP0A08") // PCI Express Root Bridge
    Name (_CID, "PNP0A03") // Compatible PCI Root Bridge
    Name(_UID, 0) // Unique ID
    Name(_SEG, 0) // Segment of this Root complex
    Name(_BBN, 0) // Base Bus Number
    Name(_PXM, 0) // Proximity Domain
    Name(_CCA, 1) // Cache Coherency Attribute
    Name(RBUF, ResourceTemplate () {
     WordBusNumber ( // Bus numbers assigned to this root
        ResourceProducer, MinFixed, MaxFixed, PosDecode,
        0,        // AddressGranularity
        0,        // AddressMinimum - Minimum Bus Number
        0xFF,     // AddressMaximum - Maximum Bus Number
        0,        // AddressTranslation
        0x100,    // RangeLength - # of Busses
      )
      DWordMemory (  // RC0 32-bit BAR Window
        ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
        0,                             // Granularity
        0x40000000,                    // Min Base Address
        0x47FEFFFF,                    // Max Base Address
        0x20000000,                    // Translate
        0x07FF0000,                    // Length
      )
      QWordMemory (  // RC0 64-bit BAR Window
        ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
        0x0000000000000000,            // Granularity
        0x0000004000000000,            // Min Base Address
        0x0000004FFFFFFFFF,            // Max Base Address
        0x0000000000000000,            // Translate
        0x0000001000000000,            // Length
      )
    })

    // Root complex resources
    Method (_CRS, 0, Serialized) {
      Return (RBUF)
    }

    Name (SUPP, Zero) // PCI _OSC Support Field value
    Name (CTRL, Zero) // PCI _OSC Control Field value
    Method (_OSC, 4, NotSerialized) {
      CreateDWordField (Arg3, 0, CDW1)

      If (LEqual (Arg0, ToUUID ("33DB4D5B-1FF7-401C-9657-7441C03DD766"))) {
        CreateDWordField (Arg3, 4, CDW2)
        CreateDWordField (Arg3, 8, CDW3)

        Store (CDW2, SUPP)
        Store (CDW3, CTRL)

        // Keep native PCIe feature ownership conservative on KMH for now.
        And (CTRL, 0x10, CTRL)

        If (LNotEqual (Arg1, One)) {
          Or (CDW1, 0x08, CDW1)
        }

        If (LNotEqual (CDW3, CTRL)) {
          Or (CDW1, 0x10, CDW1)
        }

        Store (CTRL, CDW3)
        Return (Arg3)
      } Else {
        Or (CDW1, 0x04, CDW1)
        Return (Arg3)
      }
    }

    Name (_PRT, Package () {
      Package () { 0xFFFF, 0, 0, 13 },
      Package () { 0xFFFF, 1, 0, 13 },
      Package () { 0xFFFF, 2, 0, 13 },
      Package () { 0xFFFF, 3, 0, 13 },
      Package () { 0x0000FFFF, 0, 0, 13 },
      Package () { 0x0000FFFF, 1, 0, 13 },
      Package () { 0x0000FFFF, 2, 0, 13 },
      Package () { 0x0000FFFF, 3, 0, 13 },
    })
  }

  Device (PCI1) {
    Name (_HID, "PNP0A08") // PCI Express Root Bridge
    Name (_CID, "PNP0A03") // Compatible PCI Root Bridge
    Name(_UID, 1) // Unique ID
    Name(_SEG, 1) // Segment of this Root complex
    Name(_BBN, 0) // Base Bus Number
    Name(_PXM, 0) // Proximity Domain
    Name(_CCA, 1) // Cache Coherency Attribute
    Name(RBUF, ResourceTemplate () {
     WordBusNumber ( // Bus numbers assigned to this root
        ResourceProducer, MinFixed, MaxFixed, PosDecode,
        0,        // AddressGranularity
        0,        // AddressMinimum - Minimum Bus Number
        0,        // AddressMaximum - Maximum Bus Number
        0,        // AddressTranslation
        1,        // RangeLength - # of Busses
      )
      DWordMemory (  // RC1 32-bit BAR Window
        ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
        0,                             // Granularity
        0x40000000,                    // Min Base Address
        0x47FEFFFF,                    // Max Base Address
        0x30000000,                    // Translate
        0x07FF0000,                    // Length
      )
      QWordMemory (  // RC1 64-bit BAR Window
        ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
        0x0000000000000000,            // Granularity
        0x0000006000000000,            // Min Base Address
        0x0000006FFFFFFFFF,            // Max Base Address
        0x0000000000000000,            // Translate
        0x0000001000000000,            // Length
      )
    })

    // Root complex resources
    Method (_CRS, 0, Serialized) {
      Return (RBUF)
    }

    Name (SUPP, Zero) // PCI _OSC Support Field value
    Name (CTRL, Zero) // PCI _OSC Control Field value
    Method (_OSC, 4, NotSerialized) {
      CreateDWordField (Arg3, 0, CDW1)

      If (LEqual (Arg0, ToUUID ("33DB4D5B-1FF7-401C-9657-7441C03DD766"))) {
        CreateDWordField (Arg3, 4, CDW2)
        CreateDWordField (Arg3, 8, CDW3)

        Store (CDW2, SUPP)
        Store (CDW3, CTRL)

        And (CTRL, 0x10, CTRL)

        If (LNotEqual (Arg1, One)) {
          Or (CDW1, 0x08, CDW1)
        }

        If (LNotEqual (CDW3, CTRL)) {
          Or (CDW1, 0x10, CDW1)
        }

        Store (CTRL, CDW3)
        Return (Arg3)
      } Else {
        Or (CDW1, 0x04, CDW1)
        Return (Arg3)
      }
    }

    Name (_PRT, Package () {
      Package () { 0xFFFF, 0, 0, 13 },
      Package () { 0xFFFF, 1, 0, 13 },
      Package () { 0xFFFF, 2, 0, 13 },
      Package () { 0xFFFF, 3, 0, 13 },
      Package () { 0x0000FFFF, 0, 0, 13 },
      Package () { 0x0000FFFF, 1, 0, 13 },
      Package () { 0x0000FFFF, 2, 0, 13 },
      Package () { 0x0000FFFF, 3, 0, 13 },
    })
  }
}
