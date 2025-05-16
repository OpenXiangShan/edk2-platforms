#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/HiiLib.h>
#include <Library/DebugLib.h>
#include <Uefi/UefiSpec.h>

#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Core/Dxe/DxeMain.h>

#define EFI_IMAGE_PE_SIGNATURE                 0x00004550 // PE
#define EFI_IMAGE_FILE_MACHINE_RISCV32         0x5032 // RISC-V 32-bit
#define EFI_IMAGE_FILE_MACHINE_RISCV64         0x5064 // RISC-V 64-bit
#define EFI_IMAGE_PE_OPTIONAL_HDR32_MAGIC      0x10b
#define EFI_IMAGE_PE_OPTIONAL_HDR32_PLUS_MAGIC 0x20b

EFI_HANDLE gShellLoaderToolHiiHandle = NULL;
CONST CHAR16 gShellLoaderToolFileName[] = L"LoaderToolShellCommand";

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"efi_stub", TypeFlag},
  {L"jump", TypeFlag},
  {L"help", TypeFlag},
  {NULL , TypeMax}
};

STATIC
VOID
Usage (
  VOID
  )
{
  Print (L"Bosc Linux Loader:\n"
         "To boot linux from payload in uefi.\n"
	 "\n"
         "1. bl jump\n"
	 "   Jump to start address of payload in uefi.\n"
	 "2. bl efi_stub param1 param2 ...\n"
	 "   find efi entry point in payload and start from it, following by kernel params.\n"
	 "   eg. bl efi_stub console=ttyS0,115200 earlycon=sbi acpi=force\n"
  );
}

CONST CHAR16*
EFIAPI
ShellCommandGetManFileNameLoaderTool (
  VOID
  )
{
  return gShellLoaderToolFileName;
}

SHELL_STATUS
EFIAPI
ShellBootFromAddress (unsigned long entry, unsigned long dtb, unsigned long hartid)
{
  ((void(*)(unsigned long, unsigned long))entry)(hartid, dtb);
  
  return EFI_SUCCESS;
}

SHELL_STATUS
EFIAPI
ShellCommandSimpleJump (
  VOID
  )
{
  unsigned long long kernel_entry = FixedPcdGet32(PcdKernelBase);
  unsigned long long dtb_start = FixedPcdGet32(PcdDTBBase);
  int hart_id = FixedPcdGet32 (PcdBootHartId);
 
  return ShellBootFromAddress (kernel_entry, dtb_start, hart_id);
}

STATIC
VOID
ShellCommandSetupCmdLine (
  LIST_ENTRY                *CheckPackage,
  UINTN                     ParamCount,
  EFI_LOADED_IMAGE_PROTOCOL *LoadImage
  )
{
  UINTN       i;
  CHAR16      *ValueStr;
  CHAR16       *CmdLine;
  CHAR16       *Tmp;

  CmdLine = AllocateZeroPool (128);
  Tmp = CmdLine;

  for (i = 0; i < ParamCount;i++) {
    ValueStr = (CHAR16 *)ShellCommandLineGetRawValue (CheckPackage, i + 1);

    while (*ValueStr != 0)
      *Tmp++ = *ValueStr++;

    *Tmp++ = ' ';
  }
  *Tmp = 0;

  Print (L"Cmdline: %s\n", CmdLine);

  LoadImage->LoadOptions = CmdLine;
  LoadImage->LoadOptionsSize = (UINTN)Tmp - (UINTN)CmdLine + 1;
}

STATIC
SHELL_STATUS
ShellCommandConstructImage (
  EFI_IMAGE_ENTRY_POINT     *EntryPoint,
  EFI_SYSTEM_TABLE          *SystemTable,
  LOADED_IMAGE_PRIVATE_DATA *Image
  )
{
  Image->Signature         = LOADED_IMAGE_PRIVATE_DATA_SIGNATURE;
  Image->Info.SystemTable  = SystemTable;
  Image->Info.DeviceHandle = NULL;
  Image->Info.Revision     = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
  Image->Info.FilePath     = NULL;
  Image->NumberOfPages = 0;

  return SHELL_SUCCESS;
}

STATIC
VOID*
ShellCommandGetEfiEntryPoint (
  unsigned long long ImageBase
  )
{
  EFI_IMAGE_DOS_HEADER *DosHeader;
  EFI_IMAGE_FILE_HEADER *CoffHeader;
  EFI_IMAGE_OPTIONAL_HEADER32 *OptionalHeader;
  UINT32 PeMagic;

  DosHeader = (EFI_IMAGE_DOS_HEADER *)ImageBase;

  if (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
    Print (L"DosHeader->e_magic is 0x%x, but expected 0x5A4D\n", DosHeader->e_magic);
    return NULL;
  }

  PeMagic = *(UINT32 *)(ImageBase + sizeof(EFI_IMAGE_DOS_HEADER));
  if (PeMagic != EFI_IMAGE_PE_SIGNATURE) {
    Print (L"PeMagic is 0x%x, but expected 0x00004550\n", PeMagic);
    return NULL;
  }

  //We only support RISCV
  CoffHeader = (EFI_IMAGE_FILE_HEADER *)(ImageBase + sizeof(EFI_IMAGE_DOS_HEADER) + 4);
  if (CoffHeader->Machine != EFI_IMAGE_FILE_MACHINE_RISCV32 &&
        CoffHeader->Machine != EFI_IMAGE_FILE_MACHINE_RISCV64) {
    Print (L"CoffHeader->Machine is 0x%x, but expected is 0x5032 or 0x5064\n", CoffHeader->Machine);
    return NULL;
  }

  OptionalHeader = (EFI_IMAGE_OPTIONAL_HEADER32 *)(ImageBase + sizeof(EFI_IMAGE_DOS_HEADER) + 4 + sizeof(EFI_IMAGE_FILE_HEADER));
  if (OptionalHeader->Magic != EFI_IMAGE_PE_OPTIONAL_HDR32_MAGIC &&
        OptionalHeader->Magic != EFI_IMAGE_PE_OPTIONAL_HDR32_PLUS_MAGIC) {
    Print (L"OptionalHeader->Magic is 0x%x, but expected is 0x010b or 0x020b\n", OptionalHeader->Magic);
    return NULL;
  }

  return (void*)(OptionalHeader->AddressOfEntryPoint + ImageBase);
}

STATIC
SHELL_STATUS
EFIAPI
ShellCommandEfiStub (
  LIST_ENTRY        *CheckPackage,
  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  unsigned long long         ImageBase = FixedPcdGet32(PcdKernelBase);
  VOID                       *EntryPoint;
  EFI_IMAGE_ENTRY_POINT      Entry;
  LOADED_IMAGE_PRIVATE_DATA  *Image = NULL;
  UINTN                       ParamCount;

  Print (L"ImageBase:0x%p\n", ImageBase);

  EntryPoint = ShellCommandGetEfiEntryPoint (ImageBase);
  if (!EntryPoint) {
    Print (L"Can not get EntryPoint\n");
    return SHELL_ABORTED;
  }

  Print (L"EntryPoint:0x%p\n", EntryPoint);

  Image = AllocateZeroPool (sizeof (LOADED_IMAGE_PRIVATE_DATA));
  if (Image == NULL) {
    Print (L"%s Out of Memory\n", __FUNCTION__);
    return SHELL_ABORTED;
  }

  Status = ShellCommandConstructImage (EntryPoint, SystemTable, Image);
  if (EFI_ERROR (Status)) {
    Print (L"ConstructImage failed\n");
    return SHELL_ABORTED;
  }

  //The First param is opcode
  ParamCount = ShellCommandLineGetCount (CheckPackage) - 1;

  if (ParamCount > 1)
    ShellCommandSetupCmdLine (CheckPackage, ParamCount, &Image->Info);

  Status = gBS->InstallProtocolInterface (
			&Image->Handle,
			&gEfiLoadedImageProtocolGuid,
			EFI_NATIVE_INTERFACE,
			&Image->Info);
  if (EFI_ERROR (Status)) {
    Print (L"InstallProtocolInterface Image failed\n");
    return SHELL_ABORTED;
  }

  Entry = (EFI_IMAGE_ENTRY_POINT)EntryPoint;

  Print (L"gogogo... EntryPoint:0x%p EFI_HANDLE:0x%llx SystemTable:0x%llx\n\n",
          Entry,
	  Image->Handle,
	  SystemTable);

  Status = Entry (Image->Handle, SystemTable);
  if (EFI_ERROR (Status)) {
    if (Image->Info.LoadOptionsSize > 0)
	FreePool (Image->Info.LoadOptions);
    FreePool (Image);

    return SHELL_ABORTED;
  }

  if (Image->Info.LoadOptionsSize > 0)
    FreePool (Image->Info.LoadOptions);
  FreePool (Image);

  return EFI_SUCCESS;
}

SHELL_STATUS
EFIAPI
ShellCommandRunBoscLoaderTool (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;
  LIST_ENTRY     *CheckPackage;
  CHAR16         *ProblemParam;

  DEBUG ((DEBUG_INFO, "Bosc Loader...\n"));

  Status = ShellCommandLineParse (ParamList, &CheckPackage, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    Print (L"Error - failed to parse command line\n");
    return SHELL_ABORTED;
  }

  if (ShellCommandLineGetFlag (CheckPackage, L"jump")) {
    return ShellCommandSimpleJump ();
  }
  else if (ShellCommandLineGetFlag (CheckPackage, L"efi_stub")) {
    return ShellCommandEfiStub (CheckPackage, SystemTable);
  }
  else if (ShellCommandLineGetFlag (CheckPackage, L"help")) {
    Usage ();
    return SHELL_SUCCESS;
  }
  else {
    Print (L"Invalid Command...");
    Usage ();
    return SHELL_ABORTED;
  }

  return SHELL_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellBoscLoaderToolLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gShellLoaderToolHiiHandle = HiiAddPackages (
                        &gShellLoaderToolHiiGuid, gImageHandle, 
                        UefiShellLoaderToolLibStrings, NULL
                        );
  if (gShellLoaderToolHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }
  
  ShellCommandRegisterCommandName (
     L"bl", ShellCommandRunBoscLoaderTool, ShellCommandGetManFileNameLoaderTool, 0,
     L"bl", TRUE , gShellLoaderToolHiiHandle, STRING_TOKEN (STR_GET_HELP_LOADER_TOOL)
     );
 
  return EFI_SUCCESS; 
}

EFI_STATUS
EFIAPI
ShellBoscLoaderToolLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gShellLoaderToolHiiHandle != NULL) {
    HiiRemovePackages (gShellLoaderToolHiiHandle);
  }

  return EFI_SUCCESS;
}
