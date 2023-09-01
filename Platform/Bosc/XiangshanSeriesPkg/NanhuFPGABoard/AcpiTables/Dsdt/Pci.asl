Scope(_SB)
{
  Device (PCI0) {
    Name (_HID, "PNP0A08") // PCI Express Root Bridge
    Name (_CID, "PNP0A03") // Compatible PCI Root Bridge
    Name(_SEG, 0) // Segment of this Root complex
    Name(_BBN, 0) // Base Bus Number
    Name(_CCA, 1) // Cache Coherency Attribute
    Name(RBUF, ResourceTemplate () {
     WordBusNumber ( // Bus numbers assigned to this root
        ResourceProducer, MinFixed, MaxFixed, PosDecode,
        0,        // AddressGranularity
        0,        // AddressMinimum - Minimum Bus Number
        2,     // AddressMaximum - Maximum Bus Number
        0,        // AddressTranslation
        3,    // RangeLength - # of Busses
      )
      QWordMemory (  // 64-bit BAR Windows
        ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
        0,                             // Granularity
        0x0000000060000000,            // Min Base Address
        0x000000007FFFFFFF,            // Max Base Address
        0x0000000000000000,            // Translate
        0x0000000020000000,            // Length
      )
    })

    // Root complex resources
    Method (_CRS, 0, Serialized) {
      Return (RBUF)
    }
  }
}
