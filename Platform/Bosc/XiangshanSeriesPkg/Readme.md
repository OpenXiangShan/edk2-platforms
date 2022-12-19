# Introduction of Bosc Xiangshan Series Platforms
XiangshanSeriesPkg provides the common EDK2 libraries and drivers for Bosc Xiangshan series
platforms. Currently the supported platforms are Nanhu platform.

This platforms is built with below common edk2 packages under edk2-platforms
repository,
- [**XiangshanSeriesPkg**](https://github.com/tianocore/edk2-platforms/tree/master/Platform/Bosc/XiangshanSeriesPkg)
- [**RiscVPlatformPkg**](https://github.com/tianocore/edk2-platforms/tree/master/Platform/RISC-V/PlatformPkg)
- [**RiscVProcessorPkg**](https://github.com/tianocore/edk2-platforms/tree/master/Silicon/RISC-V/ProcessorPkg)

## Nanhu Platform
This is a sample platform package used against to Bosc products.

The binary built from Platform/Bosc/XiangshanSeriesPkg/NanhuFPGABoard can run
on S2C FPGA board.
```
build -a RISCV64 -t GCC5 -p Platform/Bosc/XiangshanSeriesPkg/NanhuFPGABoard/Nanhu.dsc
```

## U5SeriesPkg Libraries and Drivers
### PeiCoreInfoHobLib
This is the library to create RISC-V core characteristics for building up RISC-V
related SMBIOS records to support a single boot loader  or OS image on all RISC-V
platforms by discovering RISC-V hart configurations dynamically. This library leverage
the silicon libraries provided in Silicon/SiFive.

### RiscVPlatformTimerLib
This is common platform timer library which has the platform-specific
timer implementation.

### SerialLib
This is platform serial port library.

### TimerDxe
This is platform timer DXE driver which has the platform-specific
timer implementation.

## Nanhu FPGA Platform Libraries and Drivers
### RiscVOpensbiPlatformLib
In order to reduce the dependencies with RISC-V OpenSBI project
(https://github.com/riscv/opensbi) and avoid duplicating code we use it, the
implementation of RISC-V EDK2 platform is leveraging platform source code from OpenSBI
code tree. The "platform.c" under OpenSbiPlatformLib is cloned from
[RISC-V OpenSBI code tree](Silicon/RISC-V/ProcessorPkg/Library/RiscVOpensbiLib/opensbi)
and built based on edk2 build environment.

### PlatformPei
This is the platform-implementation specific library which is executed in early PEI
phase for Nanhu FPGA platform initialization.

## Nanhu FPGA Platform Libraries and Drivers
### RiscVOpensbiPlatformLib
In order to reduce the dependencies with RISC-V OpenSBI project
(https://github.com/riscv/opensbi) and fewer burdens to EDK2 build process, the
implementation of RISC-V EDK2 platform is leveraging platform source code from
OpenSBI code tree. The "platform.c" under OpenSbiPlatformLib is cloned from
[RISC-V OpenSBI code tree](Silicon/RISC-V/ProcessorPkg/Library/RiscVOpensbiLib/opensbi)
and built based on edk2build environment.

### PlatformPei
This is the platform-implementation specific library which is executed in early PEI
phase for Nanhu FPGA platform initialization.

## XiangshanSeriesPkg Platform PCD settings

| **PCD name** |**Usage**|
|----------------|----------|
|PcdU5PlatformSystemClock| Xiangshan series platform system clock|
|PcdNumberofU5Cores| Number of CPU core enabled on Xiangshan series platform|
|PcdU5UartBase|Platform serial port base address|


## Platform Owners
Ran Wang <wangran@bosc.ac.cn>
