Scope(_SB)
{
  Device (XDM0) {
    Name (_HID, "BOSC0200")
    Name (_UID, 0)
    Name (_CCA, 1)

    Name (_DSD, Package () {
      ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () { "compatible", "xlnx,xdma-host-3.00" },
        Package () { "device_type", "pci" },
        Package () { "bus-start", 0 },
      }
    })

    Name (RBUF, ResourceTemplate () {
      Memory32Fixed (ReadWrite, 0x48000000, 0x08000000)
      Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { 14 }
    })

    Method (_CRS, 0, Serialized) {
      Return (RBUF)
    }
  }
}
