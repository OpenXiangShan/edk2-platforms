/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembly of HEST.aml
 *
 * ACPI Data Table [HEST]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue (in hex)
 */

[000h 0000 004h]                   Signature : "HEST"    [Hardware Error Source Table]
[004h 0004 004h]                Table Length : 00000068
[008h 0008 001h]                    Revision : 01
[009h 0009 001h]                    Checksum : 61
[00Ah 0010 006h]                      Oem ID : "OEMID "
[010h 0016 008h]                Oem Table ID : "HESTGEN"
[018h 0024 004h]                Oem Revision : 00000001
[01Ch 0028 004h]             Asl Compiler ID : "INTL"
[020h 0032 004h]       Asl Compiler Revision : 20251212

[024h 0036 004h]          Error Source Count : 00000001

[028h 0040 002h]               Subtable Type : 0009 [Generic Hardware Error Source]
[02Ah 0042 002h]                   Source Id : 0001
[02Ch 0044 002h]           Related Source Id : FFFF
[02Eh 0046 001h]                    Reserved : 00
[02Fh 0047 001h]                     Enabled : 01
[030h 0048 004h]      Records To Preallocate : 00000001
[034h 0052 004h]     Max Sections Per Record : 00000001
[038h 0056 004h]         Max Raw Data Length : 00001000

[03Ch 0060 00Ch]        Error Status Address : [Generic Address Structure]
[03Ch 0060 001h]                    Space ID : 00 [SystemMemory]
[03Dh 0061 001h]                   Bit Width : 40
[03Eh 0062 001h]                  Bit Offset : 00
[03Fh 0063 001h]        Encoded Access Width : 04 [QWord Access:64]
[040h 0064 008h]                     Address : 0000000087001200

[048h 0072 01Ch]                      Notify : [Hardware Error Notification Structure]
[048h 0072 001h]                 Notify Type : 00 [Polled]
[049h 0073 001h]               Notify Length : 1C
[04Ah 0074 002h]  Configuration Write Enable : 0000
[04Ch 0076 004h]                PollInterval : 000000A0
[050h 0080 004h]                      Vector : 00000000
[054h 0084 004h]     Polling Threshold Value : 00000000
[058h 0088 004h]    Polling Threshold Window : 00000000
[05Ch 0092 004h]       Error Threshold Value : 00000000
[060h 0096 004h]      Error Threshold Window : 00000000

[064h 0100 004h]   Error Status Block Length : 00001000

Raw Table Data: Length 104 (0x68)

    0000: 48 45 53 54 68 00 00 00 01 61 4F 45 4D 49 44 20  // HESTh....aOEMID 
    0010: 48 45 53 54 47 45 4E 00 01 00 00 00 49 4E 54 4C  // HESTGEN.....INTL
    0020: 12 12 25 20 01 00 00 00 09 00 01 00 FF FF 00 01  // ..% ............
    0030: 01 00 00 00 01 00 00 00 00 10 00 00 00 40 00 04  // .............@..
    0040: 00 12 00 87 00 00 00 00 00 1C 00 00 A0 00 00 00  // ................
    0050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // ................
    0060: 00 00 00 00 00 10 00 00                          // ........
