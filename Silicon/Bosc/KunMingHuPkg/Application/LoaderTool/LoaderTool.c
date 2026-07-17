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
#include <Library/CacheMaintenanceLib.h>
#include <Core/Dxe/DxeMain.h>
#include <IndustryStandard/PeImage.h>

#define EFI_IMAGE_PE_SIGNATURE                 0x00004550 // PE
#define EFI_IMAGE_FILE_MACHINE_RISCV32         0x5032 // RISC-V 32-bit
#define EFI_IMAGE_FILE_MACHINE_RISCV64         0x5064 // RISC-V 64-bit
#define EFI_IMAGE_PE_OPTIONAL_HDR32_MAGIC      0x10b
#define EFI_IMAGE_PE_OPTIONAL_HDR32_PLUS_MAGIC 0x20b

EFI_HANDLE gShellLoaderToolHiiHandle = NULL;
CONST CHAR16 gShellLoaderToolFileName[] = L"LoaderToolShellCommand";

#define KMH_KERNEL_COPY_EXTRA_SIZE  SIZE_8MB
#define KMH_EXTERNAL_IMAGE_DEFAULT  L"fs0:\\VMLINUX.EFI"

STATIC CONST EFI_GUID mKmhFdtBootargsGuid = {
  0x2f4c6f91, 0x8d2a, 0x4a44, { 0x9b, 0x5d, 0x48, 0x7d, 0x2d, 0x35, 0xa4, 0x17 }
};

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"efi_stub", TypeFlag},
  {L"jump", TypeFlag},
  {L"help", TypeFlag},
  {L"-image", TypeValue},
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
         "2. bl efi_stub [ImagePath] [param1 param2 ...]\n"
	 "   find efi entry point in payload or external Image and start from it.\n"
	 "   If no param is provided, command line comes from DT bootargs.\n"
	 "   eg. bl efi_stub\n"
	 "   eg. bl efi_stub fs0:\\VMLINUX.EFI\n"
	 "   eg. bl efi_stub -image fs0:\\VMLINUX.EFI\n"
	 "   eg. bl efi_stub fs0:\\VMLINUX.EFI console=ttyS0,115200 earlycon=sbi acpi=force\n"
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
  UINTN                     StartIndex,
  EFI_LOADED_IMAGE_PROTOCOL *LoadImage
  )
{
  UINTN       Index;
  CHAR16      *ValueStr;
  CHAR16       *CmdLine;
  CHAR16       *Tmp;

  CmdLine = AllocateZeroPool (1024);
  Tmp = CmdLine;

  for (Index = StartIndex; ; Index++) {
    ValueStr = (CHAR16 *)ShellCommandLineGetRawValue (CheckPackage, Index);
    if (ValueStr == NULL) {
      break;
    }

    while (*ValueStr != 0)
      *Tmp++ = *ValueStr++;

    *Tmp++ = ' ';
  }
  *Tmp = 0;

  Print (L"Cmdline: %s\n", CmdLine);

  LoadImage->LoadOptions = CmdLine;
  LoadImage->LoadOptionsSize = (UINTN)(Tmp - CmdLine + 1) * sizeof (CHAR16);
}

STATIC
EFI_STATUS
ShellCommandSetupCmdLineFromFdtBootargs (
  EFI_LOADED_IMAGE_PROTOCOL *LoadImage
  )
{
  EFI_STATUS  Status;
  UINTN       BootargsSize;
  CHAR8       *Bootargs;
  CHAR16      *CmdLine;
  UINTN       Index;
  UINTN       TextSize;

  BootargsSize = 0;
  Status = gRT->GetVariable (
                  L"KmhFdtBootargs",
                  (EFI_GUID *)&mKmhFdtBootargsGuid,
                  NULL,
                  &BootargsSize,
                  NULL
                  );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print (L"KmhFdtBootargs variable is not available: %r\n", Status);
    return Status;
  }

  Bootargs = AllocateZeroPool (BootargsSize + 1);
  if (Bootargs == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetVariable (
                  L"KmhFdtBootargs",
                  (EFI_GUID *)&mKmhFdtBootargsGuid,
                  NULL,
                  &BootargsSize,
                  Bootargs
                  );
  if (EFI_ERROR (Status)) {
    Print (L"Read KmhFdtBootargs variable failed: %r\n", Status);
    FreePool (Bootargs);
    return Status;
  }

  Bootargs[BootargsSize] = '\0';
  TextSize = AsciiStrnLenS (Bootargs, BootargsSize);
  CmdLine = AllocateZeroPool ((TextSize + 1) * sizeof (CHAR16));
  if (CmdLine == NULL) {
    FreePool (Bootargs);
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < TextSize; Index++) {
    CmdLine[Index] = (CHAR16)Bootargs[Index];
  }

  CmdLine[TextSize] = L'\0';
  Print (L"Cmdline from DT bootargs: %s\n", CmdLine);

  LoadImage->LoadOptions = CmdLine;
  LoadImage->LoadOptionsSize = (TextSize + 1) * sizeof (CHAR16);

  FreePool (Bootargs);
  return EFI_SUCCESS;
}
STATIC
BOOLEAN
ShellCommandIsImagePath (
  IN CONST CHAR16  *Path
  )
{
  if (Path == NULL) {
    return FALSE;
  }

  if ((StrStr (Path, L":\\") != NULL) ||
      (StrStr (Path, L":/") != NULL) ||
      (Path[0] == L'\\') ||
      (Path[0] == L'/'))
  {
    return TRUE;
  }

  return FALSE;
}
STATIC
EFI_STATUS
ShellCommandReadExternalImage (
  IN  CONST CHAR16  *ImagePath,
  OUT UINT64        *ImageBase,
  OUT UINTN         *ImageSize
  )
{
  EFI_STATUS         Status;
  SHELL_FILE_HANDLE  FileHandle;
  UINT64             FileSize;
  UINTN              ReadSize;
  VOID               *Buffer;

  FileHandle = NULL;
  FileSize   = 0;
  Buffer     = NULL;

  Status = ShellOpenFileByName (ImagePath, &FileHandle, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Open external Image %s failed: %r\n", ImagePath, Status);
    return Status;
  }

  Status = ShellGetFileSize (FileHandle, &FileSize);
  if (EFI_ERROR (Status) || (FileSize == 0)) {
    Print (L"Get external Image size failed: %r size=0x%lx\n", Status, FileSize);
    ShellCloseFile (&FileHandle);
    return EFI_ERROR (Status) ? Status : EFI_NOT_FOUND;
  }

  Buffer = AllocatePool ((UINTN)FileSize);
  if (Buffer == NULL) {
    ShellCloseFile (&FileHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  ReadSize = (UINTN)FileSize;
  Status = ShellReadFile (FileHandle, &ReadSize, Buffer);
  ShellCloseFile (&FileHandle);
  if (EFI_ERROR (Status) || (ReadSize != (UINTN)FileSize)) {
    Print (L"Read external Image failed: %r read=0x%lx size=0x%lx\n", Status, ReadSize, FileSize);
    FreePool (Buffer);
    return EFI_ERROR (Status) ? Status : EFI_LOAD_ERROR;
  }

  *ImageBase = (UINT64)(UINTN)Buffer;
  *ImageSize = (UINTN)FileSize;

  Print (L"External Image loaded: %s base=0x%lx size=0x%lx\n", ImagePath, *ImageBase, *ImageSize);
  return EFI_SUCCESS;
}

STATIC
SHELL_STATUS
ShellCommandConstructImage (
  EFI_IMAGE_ENTRY_POINT     *EntryPoint,
  EFI_SYSTEM_TABLE          *SystemTable,
  LOADED_IMAGE_PRIVATE_DATA *Image,
  UINT64                    ImageBase,
  UINTN                     ImageSize
  )
{
  Image->Signature         = LOADED_IMAGE_PRIVATE_DATA_SIGNATURE;
  Image->Info.SystemTable  = SystemTable;
  Image->Info.DeviceHandle = NULL;
  Image->Info.Revision     = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
  Image->Info.FilePath     = NULL;
  Image->Info.ImageBase    = (VOID *)(UINTN)ImageBase;
  Image->Info.ImageSize    = ImageSize;
  Image->NumberOfPages     = EFI_SIZE_TO_PAGES (ImageSize);

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
UINT32
ShellCommandGetEfiEntryRva (
  UINT64 ImageBase
  )
{
  EFI_IMAGE_DOS_HEADER        *DosHeader;
  EFI_IMAGE_FILE_HEADER       *CoffHeader;
  EFI_IMAGE_OPTIONAL_HEADER32 *OptionalHeader;
  UINT32                      PeMagic;

  DosHeader = (EFI_IMAGE_DOS_HEADER *)(UINTN)ImageBase;
  if (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
    Print (L"DosHeader->e_magic is 0x%x, but expected 0x5A4D\n", DosHeader->e_magic);
    return 0;
  }

  PeMagic = *(UINT32 *)(UINTN)(ImageBase + sizeof (EFI_IMAGE_DOS_HEADER));
  if (PeMagic != EFI_IMAGE_PE_SIGNATURE) {
    Print (L"PeMagic is 0x%x, but expected 0x00004550\n", PeMagic);
    return 0;
  }

  CoffHeader = (EFI_IMAGE_FILE_HEADER *)(UINTN)(ImageBase + sizeof (EFI_IMAGE_DOS_HEADER) + 4);
  if (CoffHeader->Machine != EFI_IMAGE_FILE_MACHINE_RISCV32 &&
      CoffHeader->Machine != EFI_IMAGE_FILE_MACHINE_RISCV64) {
    Print (L"CoffHeader->Machine is 0x%x, but expected is 0x5032 or 0x5064\n", CoffHeader->Machine);
    return 0;
  }

  OptionalHeader = (EFI_IMAGE_OPTIONAL_HEADER32 *)(UINTN)(ImageBase + sizeof (EFI_IMAGE_DOS_HEADER) + 4 + sizeof (EFI_IMAGE_FILE_HEADER));
  if (OptionalHeader->Magic != EFI_IMAGE_PE_OPTIONAL_HDR32_MAGIC &&
      OptionalHeader->Magic != EFI_IMAGE_PE_OPTIONAL_HDR32_PLUS_MAGIC) {
    Print (L"OptionalHeader->Magic is 0x%x, but expected is 0x010b or 0x020b\n", OptionalHeader->Magic);
    return 0;
  }

  return OptionalHeader->AddressOfEntryPoint;
}
STATIC
EFI_STATUS
ShellCommandCopyKernelImage (
  IN  UINT64  SourceBase,
  IN  UINTN   PeImageSize,
  OUT UINT64  *CopiedBase,
  OUT UINTN   *CopiedSize,
  OUT VOID    **CopiedEntryPoint
  )
{
  EFI_STATUS  Status;
  EFI_PHYSICAL_ADDRESS  AllocBase;
  UINTN       AllocSize;
  UINTN       AllocPages;
  UINT32      EntryRva;

  EntryRva = ShellCommandGetEfiEntryRva (SourceBase);
  if (EntryRva == 0) {
    return EFI_LOAD_ERROR;
  }

  AllocSize = FixedPcdGet64 (PcdKernelSize) + KMH_KERNEL_COPY_EXTRA_SIZE;
  if (AllocSize < PeImageSize) {
    AllocSize = PeImageSize + KMH_KERNEL_COPY_EXTRA_SIZE;
  }
  AllocSize = ALIGN_VALUE (AllocSize, EFI_PAGE_SIZE);
  AllocPages = EFI_SIZE_TO_PAGES (AllocSize);

  AllocBase = 0;
  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiLoaderCode,
                  AllocPages,
                  &AllocBase
                  );
  if (EFI_ERROR (Status)) {
    Print (L"Allocate kernel copy pages failed: %r pages=0x%lx size=0x%lx\n", Status, AllocPages, AllocSize);
    return Status;
  }

  ZeroMem ((VOID *)(UINTN)AllocBase, AllocSize);
  CopyMem ((VOID *)(UINTN)AllocBase, (VOID *)(UINTN)SourceBase, PeImageSize);
  InvalidateInstructionCacheRange ((VOID *)(UINTN)AllocBase, PeImageSize);

  *CopiedBase       = AllocBase;
  *CopiedSize       = AllocSize;
  *CopiedEntryPoint = (VOID *)(UINTN)(AllocBase + EntryRva);

  Print (
    L"Kernel copy: src=0x%lx pe-size=0x%lx dst=0x%lx alloc-size=0x%lx entry=0x%p rva=0x%x\n",
    SourceBase,
    PeImageSize,
    *CopiedBase,
    *CopiedSize,
    *CopiedEntryPoint,
    EntryRva
    );

  return EFI_SUCCESS;
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
  UINT64                     SourceImageBase;
  UINT64                     ImageBase;
  UINTN                      ImageSize;
  UINTN                      ImagePages;
  VOID                       *EntryPoint;
  EFI_IMAGE_ENTRY_POINT      Entry;
  LOADED_IMAGE_PRIVATE_DATA  *Image = NULL;
  UINTN                       CmdLineStartIndex;
  CHAR16                      *ExternalImagePath;
  VOID                        *ExternalImageBuffer;

  SourceImageBase     = FixedPcdGet64(PcdKernelBase);
  ExternalImagePath   = (CHAR16 *)ShellCommandLineGetValue (CheckPackage, L"-image");
  ExternalImageBuffer = NULL;
  CmdLineStartIndex   = 1;

  if (ExternalImagePath == NULL) {
    ExternalImagePath = (CHAR16 *)ShellCommandLineGetRawValue (CheckPackage, 1);
    if (ShellCommandIsImagePath (ExternalImagePath)) {
      CmdLineStartIndex = 2;
    } else {
      ExternalImagePath = NULL;
    }
  }
  if (ExternalImagePath == NULL) {
    ExternalImagePath = KMH_EXTERNAL_IMAGE_DEFAULT;
    Print (L"No external Image path specified, try %s\n", ExternalImagePath);
  }

  Status = ShellCommandReadExternalImage (ExternalImagePath, &SourceImageBase, &ImageSize);
  if (EFI_ERROR (Status)) {
    Print (L"External Image is unavailable, abort: %r\n", Status);
    return SHELL_ABORTED;
  }
  ExternalImageBuffer = (VOID *)(UINTN)SourceImageBase;

  Print (L"ImageBase:0x%p ImageSize:0x%lx PcdKernelSize:0x%lx\n", SourceImageBase, ImageSize, FixedPcdGet64 (PcdKernelSize));

  EntryPoint = ShellCommandGetEfiEntryPoint (SourceImageBase);
  if (!EntryPoint) {
    Print (L"Can not get EntryPoint\n");
    if (ExternalImageBuffer != NULL) {
      FreePool (ExternalImageBuffer);
    }
    return SHELL_ABORTED;
  }

  Print (L"EntryPoint:0x%p\n", EntryPoint);

  Status = ShellCommandCopyKernelImage (SourceImageBase, ImageSize, &ImageBase, &ImageSize, &EntryPoint);
  if (EFI_ERROR (Status)) {
    Print (L"Kernel image copy failed: %r\n", Status);
    if (ExternalImageBuffer != NULL) {
      FreePool (ExternalImageBuffer);
    }
    return SHELL_ABORTED;
  }
  if (ExternalImageBuffer != NULL) {
    FreePool (ExternalImageBuffer);
    ExternalImageBuffer = NULL;
  }
  ImagePages = EFI_SIZE_TO_PAGES (ImageSize);

  Image = AllocateZeroPool (sizeof (LOADED_IMAGE_PRIVATE_DATA));
  if (Image == NULL) {
    Print (L"%s Out of Memory\n", __FUNCTION__);
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    return SHELL_ABORTED;
  }

  Status = ShellCommandConstructImage (EntryPoint, SystemTable, Image, ImageBase, ImageSize);
  if (EFI_ERROR (Status)) {
    Print (L"ConstructImage failed\n");
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    FreePool (Image);
    return SHELL_ABORTED;
  }

  if (ShellCommandLineGetRawValue (CheckPackage, CmdLineStartIndex) != NULL) {
    ShellCommandSetupCmdLine (CheckPackage, CmdLineStartIndex, &Image->Info);
  } else {
    Status = ShellCommandSetupCmdLineFromFdtBootargs (&Image->Info);
    if (EFI_ERROR (Status)) {
      Print (L"Use empty cmdline because DT bootargs are unavailable\n");
    }
  }

  Status = gBS->InstallProtocolInterface (
			&Image->Handle,
			&gEfiLoadedImageProtocolGuid,
			EFI_NATIVE_INTERFACE,
			&Image->Info);
  if (EFI_ERROR (Status)) {
    Print (L"InstallProtocolInterface Image failed\n");
    if (Image->Info.LoadOptionsSize > 0)
	FreePool (Image->Info.LoadOptions);
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    FreePool (Image);
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
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    FreePool (Image);

    return SHELL_ABORTED;
  }

  if (Image->Info.LoadOptionsSize > 0)
    FreePool (Image->Info.LoadOptions);
  gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
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
