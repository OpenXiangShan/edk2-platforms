/** @file
 *
 *  ACPI support for the Bosc platform
 *
 *  Copyright (c) 2021, Jared McNeill <jmcneill@invisible.ca>
 *  Copyright (c) 2017,2021 Andrey Warkentin <andrey.warkentin@gmail.com>
 *  Copyright (c) 2016, Linaro, Ltd. All rights reserved.
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/AcpiLib.h>
#include <IndustryStandard/Acpi20.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/FdtClient.h>

STATIC CONST EFI_GUID mAcpiTableFile = {
  0x84D13218, 0x81C2, 0x4BD5, { 0xBC, 0x52, 0x84, 0xC0, 0x7D, 0x0C, 0x25, 0x3D }
};

STATIC
EFI_STATUS
FindAcpiRsdpInMemory (
  IN  UINTN                                            StartAddress,
  IN  UINTN                                            EndAddress,
  OUT EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER     **RsdpPtr
  )
{
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp;
  UINT8                                         *AcpiPtr;
  UINTN                                         ScanSize;

  ScanSize = sizeof (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER);
  if ((EndAddress < StartAddress) || (EndAddress - StartAddress < ScanSize)) {
    return EFI_NOT_FOUND;
  }

  for (AcpiPtr = (UINT8 *)StartAddress;
       AcpiPtr <= (UINT8 *)(EndAddress - ScanSize);
       AcpiPtr += 0x10)
  {
    Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)(UINTN)AcpiPtr;
    if (CompareMem (&Rsdp->Signature, "RSD PTR ", 8) != 0) {
      continue;
    }

    if (CalculateSum8 (
          (CONST UINT8 *)Rsdp,
          sizeof (EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER)
          ) != 0)
    {
      continue;
    }

    if ((Rsdp->Revision >= 2) &&
        (CalculateSum8 ((CONST UINT8 *)Rsdp, sizeof (*Rsdp)) != 0))
    {
      continue;
    }

    *RsdpPtr = Rsdp;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
InstallOneAcpiTable (
  IN EFI_ACPI_TABLE_PROTOCOL     *AcpiTableProtocol,
  IN EFI_ACPI_DESCRIPTION_HEADER *Table
  )
{
  UINTN  TableHandle;

  return AcpiTableProtocol->InstallAcpiTable (
                              AcpiTableProtocol,
                              Table,
                              Table->Length,
                              &TableHandle
                              );
}

STATIC
EFI_STATUS
InstallAcpiTablesFromRsdp (
  IN EFI_ACPI_TABLE_PROTOCOL                       *AcpiTableProtocol,
  IN EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp
  )
{
  EFI_STATUS                                  Status;
  EFI_ACPI_DESCRIPTION_HEADER                 *Xsdt;
  EFI_ACPI_DESCRIPTION_HEADER                 *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER                 *CurrentTable;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *Fadt;
  EFI_ACPI_DESCRIPTION_HEADER                 *Dsdt;
  UINTN                                       NumberOfTableEntries;
  UINTN                                       Index;

  Fadt = NULL;
  Dsdt = NULL;

  if (Rsdp->XsdtAddress != 0) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;
    NumberOfTableEntries = (Xsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) /
                           sizeof (UINT64);

    for (Index = 0; Index < NumberOfTableEntries; Index++) {
      CurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)*(UINT64 *)(
                       (UINT8 *)Xsdt + sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
                       Index * sizeof (UINT64)
                       );
      Status = InstallOneAcpiTable (AcpiTableProtocol, CurrentTable);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (CompareMem (&CurrentTable->Signature, "FACP", 4) == 0) {
        Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)CurrentTable;
      }
    }
  } else if (Rsdp->RsdtAddress != 0) {
    Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->RsdtAddress;
    NumberOfTableEntries = (Rsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) /
                           sizeof (UINT32);

    for (Index = 0; Index < NumberOfTableEntries; Index++) {
      CurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)*(UINT32 *)(
                       (UINT8 *)Rsdt + sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
                       Index * sizeof (UINT32)
                       );
      Status = InstallOneAcpiTable (AcpiTableProtocol, CurrentTable);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if (CompareMem (&CurrentTable->Signature, "FACP", 4) == 0) {
        Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)CurrentTable;
      }
    }
  }

  if (Fadt == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no FADT found\n", __func__));
    return EFI_NOT_FOUND;
  }

  if (Fadt->XDsdt != 0) {
    Dsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Fadt->XDsdt;
  } else if (Fadt->Dsdt != 0) {
    Dsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Fadt->Dsdt;
  }

  if (Dsdt == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no DSDT found\n", __func__));
    return EFI_NOT_FOUND;
  }

  return InstallOneAcpiTable (AcpiTableProtocol, Dsdt);
}

STATIC
EFI_STATUS
InstallAcpiFromDdrHandoff (
  VOID
  )
{
  EFI_STATUS                                      Status;
  FDT_CLIENT_PROTOCOL                            *FdtClient;
  EFI_ACPI_TABLE_PROTOCOL                        *AcpiTableProtocol;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER   *Rsdp;
  CONST UINT64                                   *Reg;
  UINTN                                          AddressCells;
  UINTN                                          SizeCells;
  UINT32                                         RegSize;
  UINT64                                         HandoffBase;
  UINT64                                         HandoffSize;

  Status = gBS->LocateProtocol (
                  &gFdtClientProtocolGuid,
                  NULL,
                  (VOID **)&FdtClient
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FdtClient->FindCompatibleNodeReg (
                        FdtClient,
                        "bosc,kmh-acpi-handoff",
                        (CONST VOID **)&Reg,
                        &AddressCells,
                        &SizeCells,
                        &RegSize
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((AddressCells != 2) || (SizeCells != 2) || (RegSize != 2 * sizeof (UINT64))) {
    DEBUG ((DEBUG_ERROR, "%a: invalid ACPI handoff reg\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  HandoffBase = SwapBytes64 (Reg[0]);
  HandoffSize = SwapBytes64 (Reg[1]);
  if ((HandoffSize == 0) || (HandoffBase > MAX_UINTN) ||
      (HandoffSize > MAX_UINTN) || (HandoffBase > MAX_UINTN - HandoffSize))
  {
    DEBUG ((DEBUG_ERROR, "%a: invalid ACPI handoff range\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = FindAcpiRsdpInMemory (
             (UINTN)HandoffBase,
             (UINTN)(HandoffBase + HandoffSize),
             &Rsdp
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: no valid RSDP in ACPI handoff range: %r\n", __func__, Status));
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InstallAcpiTablesFromRsdp (AcpiTableProtocol, Rsdp);
  if (!EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: installed ACPI tables from DDR handoff @ 0x%Lx/0x%Lx\n",
      __func__,
      HandoffBase,
      HandoffSize
      ));
  }

  return Status;
}

EFI_STATUS
EFIAPI
PlatformAcpiDriverEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = InstallAcpiFromDdrHandoff ();
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "%a: falling back to FV ACPI tables: %r\n", __func__, Status));
  return LocateAndInstallAcpiFromFv (&mAcpiTableFile);
}
