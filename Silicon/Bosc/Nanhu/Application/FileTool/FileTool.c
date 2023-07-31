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

STATIC EFI_FILE_PROTOCOL               *gFileRoot = NULL;
STATIC EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *gSimpleFileSystem = NULL;
STATIC EFI_HANDLE                      gShellFileToolHiiHandle = NULL;
STATIC CONST CHAR16                    gShellFileTool[] = L"FileToolShellCommand";

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"WriteFromMem", TypeFlag},
  {L"help", TypeFlag},
  {NULL , TypeMax}
  };

STATIC
VOID
Usage (
  VOID
  )
{
  Print (L"Bosc File Tool:\n"
         "1. FileIo WriteFromMem [Address] [Len] [To FileName]\n"
  );
}

STATIC
CONST CHAR16*
EFIAPI
ShellCommandGetManFileNameFileTool (
  VOID
  )
{
  return gShellFileTool;
}

EFI_STATUS
OpenFile (
  EFI_FILE_PROTOCOL **fileHandle,
  CHAR16 * fileName,
  UINT64 OpenMode
  )
{
  return gFileRoot ->Open (
            gFileRoot,
            fileHandle,
            (CHAR16*) fileName,
            OpenMode,
            0
	    );
}

VOID
CloseFile (
  EFI_FILE_PROTOCOL *fileHandle
  )
{
  fileHandle->Close (fileHandle);
}

EFI_STATUS
ReadFile (
  EFI_FILE_PROTOCOL *fileHandle,
  UINTN *bufSize,
  VOID *buffer
  )
{
  return fileHandle->Read (fileHandle, bufSize, buffer);
}

EFI_STATUS
WriteFile (
  EFI_FILE_PROTOCOL *fileHandle,
  UINTN *bufSize,
  VOID *buffer
  )
{
  return fileHandle->Write (fileHandle, bufSize, buffer);
}

EFI_STATUS
SetFilePosition (
  EFI_FILE_PROTOCOL *fileHandle,
  UINT64 position
  )
{
  return fileHandle->SetPosition (fileHandle, position);
}

EFI_STATUS
GetFilePosition(
  EFI_FILE_PROTOCOL *fileHandle,
  UINT64 *position
  )
{
  return fileHandle->GetPosition (fileHandle, position);
}

STATIC
VOID
WriteFromMem (
  LIST_ENTRY *CheckPackage
  )
{
  EFI_STATUS        Status;
  EFI_FILE_PROTOCOL *FileHandle;
  CONST CHAR16      *ValueStr;
  CHAR16            *FileName;
  EFI_LBA           addr;
  INTN              len;
  UINTN             WriteLen;

  ValueStr = ShellCommandLineGetRawValue (CheckPackage, 1);
  addr = ShellHexStrToUintn (ValueStr);
  ValueStr = ShellCommandLineGetRawValue (CheckPackage, 2);
  len = ShellHexStrToUintn (ValueStr);
  FileName = (CHAR16 *)ShellCommandLineGetRawValue (CheckPackage, 3);

  Status = OpenFile (&FileHandle, FileName, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE);
  if (Status != EFI_SUCCESS) {
    Print (L"Open file: %s failed\n", FileName);
    return;
  }

  Print (L"Write From Memory start. FileName: %s addr: 0x%lx len: %d\n", FileName, addr, len);

  while (len > 0) {
    if (len > 512)
      WriteLen = 512;
    else
      WriteLen = len;

    Status = WriteFile (FileHandle, &WriteLen, (void *)addr);
    if (Status != EFI_SUCCESS) {
      Print (L"Write file failed, addr:0x%lx\n", addr);
      CloseFile (FileHandle);
      return;
    }

    addr += WriteLen;
    len -= WriteLen;
  }

  CloseFile (FileHandle);

  Print (L"Write From Memory success. FileName: %s end addr: 0x%lx remain len: %d", FileName, addr, len);

  return;
}

STATIC
SHELL_STATUS
EFIAPI
ShellCommandRunFileTool (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  LIST_ENTRY     *CheckPackage;
  CHAR16         *ProblemParam;

  Print (L"Run File Tool...\n");

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
  else if (ShellCommandLineGetFlag (CheckPackage, L"WriteFromMem")) {
    WriteFromMem (CheckPackage);
  }
  else {
    Print (L"Invalid Command\n");
    return SHELL_ABORTED;
  }

  return SHELL_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellBoscFileToolConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;

  gShellFileToolHiiHandle = HiiAddPackages (
                        &gShellFileToolHiiGuid, gImageHandle,
                        UefiShellFileToolStrings, NULL
                        );
  if (gShellFileToolHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  ShellCommandRegisterCommandName (
     L"FileIo", ShellCommandRunFileTool, ShellCommandGetManFileNameFileTool, 0,
     L"FileIo", TRUE , gShellFileToolHiiHandle, STRING_TOKEN (STR_GET_HELP_FILE_TOOL)
     );

  Status = gBS->LocateProtocol (
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		(VOID**)&gSimpleFileSystem
		);

  if (EFI_ERROR(Status))
    return Status;

  Status = gSimpleFileSystem->OpenVolume(gSimpleFileSystem, &gFileRoot);

  return Status;
}

EFI_STATUS
EFIAPI
ShellBoscFileToolDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_SUCCESS;
}
