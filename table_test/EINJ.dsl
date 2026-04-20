/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembly of EINJ.aml
 *
 * ACPI Data Table [EINJ]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue (in hex)
 */

[000h 0000 004h]                   Signature : "EINJ"    [Error Injection Table]
[004h 0004 004h]                Table Length : 00000150
[008h 0008 001h]                    Revision : 01
[009h 0009 001h]                    Checksum : E5
[00Ah 0010 006h]                      Oem ID : "OEMID "
[010h 0016 008h]                Oem Table ID : "EINJ_XS"
[018h 0024 004h]                Oem Revision : 00000001
[01Ch 0028 004h]             Asl Compiler ID : "INTL"
[020h 0032 004h]       Asl Compiler Revision : 20251212

[024h 0036 004h]     Injection Header Length : 0000000C
[028h 0040 001h]                       Flags : 00
[029h 0041 003h]                    Reserved : 000000
[02Ch 0044 004h]       Injection Entry Count : 00000009

[030h 0048 001h]                      Action : 00 [Begin Operation]
[031h 0049 001h]                 Instruction : 02 [Write Register]
[032h 0050 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[033h 0051 001h]                    Reserved : 00

[034h 0052 00Ch]             Register Region : [Generic Address Structure]
[034h 0052 001h]                    Space ID : 00 [SystemMemory]
[035h 0053 001h]                   Bit Width : 40
[036h 0054 001h]                  Bit Offset : 00
[037h 0055 001h]        Encoded Access Width : 04 [QWord Access:64]
[038h 0056 008h]                     Address : 0000000087000000

[040h 0064 008h]                       Value : 0000000000000001
[048h 0072 008h]                        Mask : FFFFFFFFFFFFFFFF

[050h 0080 001h]                      Action : 01 [Get Trigger Table]
[051h 0081 001h]                 Instruction : 00 [Read Register]
[052h 0082 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[053h 0083 001h]                    Reserved : 00

[054h 0084 00Ch]             Register Region : [Generic Address Structure]
[054h 0084 001h]                    Space ID : 00 [SystemMemory]
[055h 0085 001h]                   Bit Width : 40
[056h 0086 001h]                  Bit Offset : 00
[057h 0087 001h]        Encoded Access Width : 04 [QWord Access:64]
[058h 0088 008h]                     Address : 0000000087000018

[060h 0096 008h]                       Value : 0000000087001000
[068h 0104 008h]                        Mask : FFFFFFFFFFFFFFFF

[070h 0112 001h]                      Action : 02 [Set Error Type]
[071h 0113 001h]                 Instruction : 02 [Write Register]
[072h 0114 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[073h 0115 001h]                    Reserved : 00

[074h 0116 00Ch]             Register Region : [Generic Address Structure]
[074h 0116 001h]                    Space ID : 00 [SystemMemory]
[075h 0117 001h]                   Bit Width : 20
[076h 0118 001h]                  Bit Offset : 00
[077h 0119 001h]        Encoded Access Width : 03 [DWord Access:32]
[078h 0120 008h]                     Address : 0000000087000008

[080h 0128 008h]                       Value : 0000000000000000
[088h 0136 008h]                        Mask : FFFFFFFFFFFFFFFF

[090h 0144 001h]                      Action : 03 [Get Error Type]
[091h 0145 001h]                 Instruction : 00 [Read Register]
[092h 0146 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[093h 0147 001h]                    Reserved : 00

[094h 0148 00Ch]             Register Region : [Generic Address Structure]
[094h 0148 001h]                    Space ID : 00 [SystemMemory]
[095h 0149 001h]                   Bit Width : 20
[096h 0150 001h]                  Bit Offset : 00
[097h 0151 001h]        Encoded Access Width : 03 [DWord Access:32]
[098h 0152 008h]                     Address : 0000000087000008

[0A0h 0160 008h]                       Value : 0000000000000000
[0A8h 0168 008h]                        Mask : FFFFFFFFFFFFFFFF

[0B0h 0176 001h]                      Action : 04 [End Operation]
[0B1h 0177 001h]                 Instruction : 04 [Noop]
[0B2h 0178 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[0B3h 0179 001h]                    Reserved : 00

[0B4h 0180 00Ch]             Register Region : [Generic Address Structure]
[0B4h 0180 001h]                    Space ID : 00 [SystemMemory]
[0B5h 0181 001h]                   Bit Width : 40
[0B6h 0182 001h]                  Bit Offset : 00
[0B7h 0183 001h]        Encoded Access Width : 04 [QWord Access:64]
[0B8h 0184 008h]                     Address : 0000000087000000

[0C0h 0192 008h]                       Value : 0000000000000000
[0C8h 0200 008h]                        Mask : 0000000000000000

[0D0h 0208 001h]                      Action : 05 [Execute Operation]
[0D1h 0209 001h]                 Instruction : 02 [Write Register]
[0D2h 0210 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[0D3h 0211 001h]                    Reserved : 00

[0D4h 0212 00Ch]             Register Region : [Generic Address Structure]
[0D4h 0212 001h]                    Space ID : 00 [SystemMemory]
[0D5h 0213 001h]                   Bit Width : 08
[0D6h 0214 001h]                  Bit Offset : 00
[0D7h 0215 001h]        Encoded Access Width : 01 [Byte Access:8]
[0D8h 0216 008h]                     Address : 0000000087000010

[0E0h 0224 008h]                       Value : 0000000000000002
[0E8h 0232 008h]                        Mask : FFFFFFFFFFFFFFFF

[0F0h 0240 001h]                      Action : 06 [Check Busy Status]
[0F1h 0241 001h]                 Instruction : 00 [Read Register]
[0F2h 0242 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[0F3h 0243 001h]                    Reserved : 00

[0F4h 0244 00Ch]             Register Region : [Generic Address Structure]
[0F4h 0244 001h]                    Space ID : 00 [SystemMemory]
[0F5h 0245 001h]                   Bit Width : 08
[0F6h 0246 001h]                  Bit Offset : 00
[0F7h 0247 001h]        Encoded Access Width : 01 [Byte Access:8]
[0F8h 0248 008h]                     Address : 0000000087000010

[100h 0256 008h]                       Value : 0000000000000000
[108h 0264 008h]                        Mask : 0000000000000001

[110h 0272 001h]                      Action : 07 [Get Command Status]
[111h 0273 001h]                 Instruction : 00 [Read Register]
[112h 0274 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[113h 0275 001h]                    Reserved : 00

[114h 0276 00Ch]             Register Region : [Generic Address Structure]
[114h 0276 001h]                    Space ID : 00 [SystemMemory]
[115h 0277 001h]                   Bit Width : 08
[116h 0278 001h]                  Bit Offset : 00
[117h 0279 001h]        Encoded Access Width : 01 [Byte Access:8]
[118h 0280 008h]                     Address : 0000000087001200

[120h 0288 008h]                       Value : 0000000000000000
[128h 0296 008h]                        Mask : 0000000000000000

[130h 0304 001h]                      Action : 08 [Set Error Type With Address]
[131h 0305 001h]                 Instruction : 02 [Write Register]
[132h 0306 001h]       Flags (decoded below) : 00
                      Preserve Register Bits : 0
[133h 0307 001h]                    Reserved : 00

[134h 0308 00Ch]             Register Region : [Generic Address Structure]
[134h 0308 001h]                    Space ID : 00 [SystemMemory]
[135h 0309 001h]                   Bit Width : 40
[136h 0310 001h]                  Bit Offset : 00
[137h 0311 001h]        Encoded Access Width : 04 [QWord Access:64]
[138h 0312 008h]                     Address : 0000000087000200

[140h 0320 008h]                       Value : 0000000000000008
[148h 0328 008h]                        Mask : FFFFFFFFFFFFFFFF

Raw Table Data: Length 336 (0x150)

    0000: 45 49 4E 4A 50 01 00 00 01 E5 4F 45 4D 49 44 20  // EINJP.....OEMID 
    0010: 45 49 4E 4A 5F 58 53 00 01 00 00 00 49 4E 54 4C  // EINJ_XS.....INTL
    0020: 12 12 25 20 0C 00 00 00 00 00 00 00 09 00 00 00  // ..% ............
    0030: 00 02 00 00 00 40 00 04 00 00 00 87 00 00 00 00  // .....@..........
    0040: 01 00 00 00 00 00 00 00 FF FF FF FF FF FF FF FF  // ................
    0050: 01 00 00 00 00 40 00 04 18 00 00 87 00 00 00 00  // .....@..........
    0060: 00 10 00 87 00 00 00 00 FF FF FF FF FF FF FF FF  // ................
    0070: 02 02 00 00 00 20 00 03 08 00 00 87 00 00 00 00  // ..... ..........
    0080: 00 00 00 00 00 00 00 00 FF FF FF FF FF FF FF FF  // ................
    0090: 03 00 00 00 00 20 00 03 08 00 00 87 00 00 00 00  // ..... ..........
    00A0: 00 00 00 00 00 00 00 00 FF FF FF FF FF FF FF FF  // ................
    00B0: 04 04 00 00 00 40 00 04 00 00 00 87 00 00 00 00  // .....@..........
    00C0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // ................
    00D0: 05 02 00 00 00 08 00 01 10 00 00 87 00 00 00 00  // ................
    00E0: 02 00 00 00 00 00 00 00 FF FF FF FF FF FF FF FF  // ................
    00F0: 06 00 00 00 00 08 00 01 10 00 00 87 00 00 00 00  // ................
    0100: 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00  // ................
    0110: 07 00 00 00 00 08 00 01 00 12 00 87 00 00 00 00  // ................
    0120: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // ................
    0130: 08 02 00 00 00 40 00 04 00 02 00 87 00 00 00 00  // .....@..........
    0140: 08 00 00 00 00 00 00 00 FF FF FF FF FF FF FF FF  // ................
