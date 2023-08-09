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
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>

STATIC EFI_HANDLE              gShellDumpBlockHiiHandle = NULL;
STATIC EFI_BLOCK_IO_PROTOCOL   **gBlockIo = NULL;
STATIC CONST CHAR16            gShellDumpBlockFileName[] = L"DumpBlockShellCommand";
STATIC UINTN                   BlockIoCount = 0;

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"read", TypeFlag},
  {L"list", TypeFlag},
  {L"help", TypeFlag},
  {NULL , TypeMax}
  };

STATIC
VOID
Usage (
  VOID
  )
{
  Print (L"Bosc Block Io Dump:\n"
         "1. DumpBlock read [which] [Address] [Length]\n"
	 "2. DumpBlock list\n"
	 "3. DumpBlock help\n"
  );
}

STATIC
VOID
DumpBlockIOMedia (EFI_BLOCK_IO_PROTOCOL* BlockIo)
{
  if (BlockIo == NULL) {
    Print (L"Invalid BlockIo...\n");
    return;
  }

  Print (L"  MediaID: %x\n",BlockIo->Media->MediaId);
  if (BlockIo->Media->RemovableMedia)
    Print (L"  RemovableMedia: True\n");
  else
    Print (L"  RemovableMedia: False\n");

  if (BlockIo->Media->MediaPresent)
    Print (L"  MediaPresent: True\n");
  else
    Print (L"  MediaPresent: False\n");

  if (BlockIo->Media->LogicalPartition)
    Print (L"  LogicalPartition: True\n");
  else
    Print (L"  LogicalPartition: False\n");

  if (BlockIo->Media->ReadOnly)
    Print (L"  ReadOnly: True\n");
  else
    Print (L"  ReadOnly: False\n");

  Print (L"  BlockSize: %x\n",BlockIo->Media->BlockSize);
  Print (L"  LastBloce(LBA): %lx\n",BlockIo->Media->LastBlock);
}

STATIC
VOID
PrintBlockIoList (
  VOID
  )
{
  UINTN index = 0;

  Print (L"Bosc Block Io List:\n");

  for (index = 0; index < BlockIoCount; index++) {
    Print (L"index : %02d\n", index);
    DumpBlockIOMedia (gBlockIo[index]);
  }
}

STATIC
CONST CHAR16*
EFIAPI
ShellCommandGetManFileNameDumpBlock (
  VOID
  )
{
  return gShellDumpBlockFileName;
}

STATIC
SHELL_STATUS
FindAllBlockIo (
  VOID
  )
{
  EFI_STATUS     Status;
  EFI_HANDLE     *HandleBuffer = NULL;
  UINTN          HandleCount = 0;
  UINTN          index;

  Status = gBS->LocateHandleBuffer (
      ByProtocol,
      &gEfiBlockIoProtocolGuid,
      NULL,
      &HandleCount,
      &HandleBuffer
  );
  Print (L"HandleCount:%d, Status:%d\n", HandleCount, Status);
  if (EFI_ERROR(Status)) {
    Print (
      L"LocateHandleBuffer gEfiBlockIoProtocolGuid failed...\n"
      );

    return Status;
  }

  gBlockIo = AllocateZeroPool (
             sizeof(EFI_BLOCK_IO_PROTOCOL*) * HandleCount
	     );
  if (gBlockIo == NULL) {
    Print (L"Alloc gBlockIo failed...\n");
    return SHELL_ABORTED;
  }

  for (index = 0; index < HandleCount; index++) {
    Status = gBS->HandleProtocol (
        HandleBuffer[index],
	&gEfiBlockIoProtocolGuid,
	(VOID**)&gBlockIo[index]
    );
    if (EFI_ERROR(Status)) {
      Print (
        L"HandleProtocol gEfiBlockIoProtocolGuid failed...\n"
	);

      return Status;
    }
  }
  BlockIoCount = HandleCount;

  if (HandleBuffer != NULL)
    FreePool (HandleBuffer);

  return Status;
}

STATIC
VOID
ReadBlockIo (
  LIST_ENTRY *CheckPackage
  )
{
  EFI_STATUS Status;
  UINT8                   *Buffer = NULL;
  UINTN                   which;
  EFI_LBA                 addr;
  UINTN                   len;
  CONST CHAR16            *ValueStr;
  EFI_BLOCK_IO_PROTOCOL*  BlockIoProtocol;

  ValueStr = ShellCommandLineGetRawValue (CheckPackage, 1);
  which = ShellHexStrToUintn (ValueStr);
  ValueStr = ShellCommandLineGetRawValue (CheckPackage, 2);
  addr = ShellHexStrToUintn (ValueStr);
  ValueStr = ShellCommandLineGetRawValue (CheckPackage, 3);
  len = ShellHexStrToUintn (ValueStr);

  if (which >= BlockIoCount) {
    Print (L"Invalid Index...\n");
    return;
  }
  BlockIoProtocol= gBlockIo[which];

  if (BlockIoProtocol == NULL) {
    Print (L"Invalid BlockIoProtocol...\n");
    return;
  }

  Print (L"ReadBlockIo which:%d addr_LBA:0x%x len:%d\n",
               which, addr, len);

  Buffer = AllocateZeroPool (512 * len);
  if (Buffer == NULL) {
    Print (L"Alloc Buffer failed...\n");
    return;
  }
  Status = BlockIoProtocol->ReadBlocks (
                   BlockIoProtocol,
                   BlockIoProtocol->Media->MediaId,
		   addr,
		   512 * len,
		   Buffer
		   );

  if (EFI_ERROR (Status)) {
    Print (L"Block Io ReadBlock error...\n");
    return;
  }

  //only show the first 512 bytes of data
  for (int i = 0; i < 32/* * len*/; i++) {
    Print (L"0x%03x:  ", i * 16 + addr);
    for (int j = 0; j < 16; j++) {
      Print (L"0x%02x", Buffer[i * 16 + j]);
      if (j < 15)
        Print (L" ");
    }
    Print(L"\n");
  }

  FreePool (Buffer);
}

STATIC
SHELL_STATUS
EFIAPI
ShellCommandRunDumpBlock (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;
  LIST_ENTRY     *CheckPackage;
  CHAR16         *ProblemParam;

  Print (L"Bosc Dump Block...\n");

  if (BlockIoCount == 0) {
    Print (L"Error - BlockIo Count is 0\n");
    return SHELL_ABORTED;
  }

  Status = ShellInitialize ();
  if (EFI_ERROR (Status)) {
    Print (L"Error - failed to initialize shell\n");
    return SHELL_ABORTED;
  }

  Status = ShellCommandLineParse (ParamList, &CheckPackage, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    Print (L"Error - failed to parse command line\n");
    return SHELL_ABORTED;
  }

  if (ShellCommandLineGetFlag (CheckPackage, L"help")) {
    Usage ();
    return SHELL_SUCCESS;
  }
  else if (ShellCommandLineGetFlag (CheckPackage, L"list")) {
    PrintBlockIoList ();
  }
  else if (ShellCommandLineGetFlag (CheckPackage, L"read")) {
    if (ShellCommandLineGetCount(CheckPackage) != 4) {
      Print (L"Not enough arguments given.\n");
      Usage ();
      return SHELL_ABORTED;
    }
    ReadBlockIo (CheckPackage);
  }
  else {
    Print (L"Invalid Command...\n");
    Usage();
    return SHELL_ABORTED;
  }

  return SHELL_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellBoscDumpBlockLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gShellDumpBlockHiiHandle = HiiAddPackages (
                        &gShellDumpBlockHiiGuid, gImageHandle,
                        UefiShellDumpBlockToolLibStrings, NULL
                        );
  if (gShellDumpBlockHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  ShellCommandRegisterCommandName (
     L"DumpBlock", ShellCommandRunDumpBlock, ShellCommandGetManFileNameDumpBlock, 0,
     L"DumpBlock", TRUE , gShellDumpBlockHiiHandle, STRING_TOKEN (STR_GET_HELP_DUMP_BLOCK_IO)
     );

  FindAllBlockIo ();

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellBoscDumpBlockLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gShellDumpBlockHiiHandle != NULL) {
    HiiRemovePackages (gShellDumpBlockHiiHandle);
  }

  if (gBlockIo != NULL) {
    FreePool (gBlockIo);
  }

  return EFI_SUCCESS;
}
