#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/UefiBootServicesTableLib.h>
EFI_STATUS
EFIAPI
Register_Entry (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS            Status = EFI_SUCCESS;
  NON_DISCOVERABLE_DEVICE *Device;
  EFI_HANDLE    LocalHandle = NULL;

  DEBUG ((DEBUG_INFO, "%a() %d \n", __FUNCTION__, __LINE__));
  UINTN AllocSize = sizeof *Device + 2 * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR);
  Device = (NON_DISCOVERABLE_DEVICE *)AllocateZeroPool (AllocSize);
  
  Device->Resources  = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(Device + 1);
  Device->Resources[0].AddrRangeMin = 0x31010000;
  Device->Resources[1].AddrRangeMin = 0xAABBCCDDEEFFAABB;

  Status = gBS->InstallMultipleProtocolInterfaces(
    &LocalHandle,
    &gEdkiiNonDiscoverableDeviceProtocolGuid,
    Device,
    NULL
  );
  return Status;
}
