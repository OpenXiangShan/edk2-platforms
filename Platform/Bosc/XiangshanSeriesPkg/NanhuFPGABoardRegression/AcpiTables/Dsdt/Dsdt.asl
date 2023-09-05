/** @file
*  Differentiated System Description Table Fields (DSDT) for the PINE Quartz64.
*
*  Copyright (c) 2022, Jared McNeill <jmcneill@invisible.ca>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <IndustryStandard/Acpi60.h>

DefinitionBlock ("DsdtTable.aml", "DSDT",
                 EFI_ACPI_6_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_REVISION,
                 "BOSC  ", "NANHU  ", FixedPcdGet32 (PcdAcpiDefaultOemRevision)) {
  Scope (_SB) {
    include ("Uart.asl")
    //include ("sd.asl")
    include ("Pci.asl")
  } // Scope (_SB)
}
