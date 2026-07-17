#include <Uefi.h>

#include <Guid/FileInfo.h>
#include <IndustryStandard/PeImage.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#define EFI_IMAGE_PE_SIGNATURE                  0x00004550
#define EFI_IMAGE_FILE_MACHINE_RISCV32          0x5032
#define EFI_IMAGE_FILE_MACHINE_RISCV64          0x5064
#define EFI_IMAGE_PE_OPTIONAL_HDR32_MAGIC       0x10b
#define EFI_IMAGE_PE_OPTIONAL_HDR32_PLUS_MAGIC  0x20b

#define KMH_KERNEL_COPY_EXTRA_SIZE  SIZE_8MB

STATIC CONST EFI_GUID mKmhFdtBootargsGuid = {
  0x2f4c6f91, 0x8d2a, 0x4a44, { 0x9b, 0x5d, 0x48, 0x7d, 0x2d, 0x35, 0xa4, 0x17 }
};

STATIC CONST CHAR16 *mKmhExternalImagePaths[] = {
  L"\\EFI\\KMH\\VMLINUX.EFI",
  L"\\VMLINUX.EFI",
  NULL
};

typedef struct {
  UINTN                        Signature;
  EFI_LOADED_IMAGE_PROTOCOL    Info;
  EFI_HANDLE                   Handle;
  EFI_DEVICE_PATH_PROTOCOL     *LoadedImageDevicePath;
  EFI_DEVICE_PATH_PROTOCOL     *FilePath;
  EFI_PHYSICAL_ADDRESS         ImageBasePage;
  UINTN                        NumberOfPages;
} KMH_LOADED_IMAGE_PRIVATE_DATA;

STATIC
EFI_STATUS
KmhSetupCmdLineFromFdtBootargs (
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
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
    Print (L"BdsLoaderTool: KmhFdtBootargs unavailable: %r\n", Status);
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
  Print (L"BdsLoaderTool: Cmdline from DT bootargs: %s\n", CmdLine);

  LoadedImage->LoadOptions = CmdLine;
  LoadedImage->LoadOptionsSize = (TextSize + 1) * sizeof (CHAR16);

  FreePool (Bootargs);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
KmhReadFileFromVolume (
  IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFs,
  IN  CONST CHAR16                     *Path,
  OUT VOID                             **Buffer,
  OUT UINTN                            *BufferSize
  )
{
  EFI_STATUS         Status;
  EFI_FILE_PROTOCOL  *Root;
  EFI_FILE_PROTOCOL  *File;
  EFI_FILE_INFO      *FileInfo;
  UINTN              FileInfoSize;
  UINTN              ReadSize;
  VOID               *Data;

  Root = NULL;
  File = NULL;
  FileInfo = NULL;
  Data = NULL;

  Status = SimpleFs->OpenVolume (SimpleFs, &Root);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Root->Open (Root, &File, (CHAR16 *)Path, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Root->Close (Root);
    return Status;
  }

  FileInfoSize = 0;
  Status = File->GetInfo (File, &gEfiFileInfoGuid, &FileInfoSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    File->Close (File);
    Root->Close (Root);
    return EFI_LOAD_ERROR;
  }

  FileInfo = AllocatePool (FileInfoSize);
  if (FileInfo == NULL) {
    File->Close (File);
    Root->Close (Root);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->GetInfo (File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
  if (EFI_ERROR (Status) || (FileInfo->FileSize == 0)) {
    FreePool (FileInfo);
    File->Close (File);
    Root->Close (Root);
    return EFI_ERROR (Status) ? Status : EFI_NOT_FOUND;
  }

  Data = AllocatePool ((UINTN)FileInfo->FileSize);
  if (Data == NULL) {
    FreePool (FileInfo);
    File->Close (File);
    Root->Close (Root);
    return EFI_OUT_OF_RESOURCES;
  }

  ReadSize = (UINTN)FileInfo->FileSize;
  Status = File->Read (File, &ReadSize, Data);
  if (EFI_ERROR (Status) || (ReadSize != (UINTN)FileInfo->FileSize)) {
    FreePool (Data);
    FreePool (FileInfo);
    File->Close (File);
    Root->Close (Root);
    return EFI_ERROR (Status) ? Status : EFI_LOAD_ERROR;
  }

  *Buffer = Data;
  *BufferSize = ReadSize;

  FreePool (FileInfo);
  File->Close (File);
  Root->Close (Root);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
KmhReadExternalImage (
  OUT UINT64  *ImageBase,
  OUT UINTN   *ImageSize
  )
{
  EFI_STATUS                       Status;
  EFI_HANDLE                       *Handles;
  UINTN                            HandleCount;
  UINTN                            HandleIndex;
  UINTN                            PathIndex;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFs;
  VOID                             *Buffer;
  UINTN                            BufferSize;

  Handles = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Status = gBS->HandleProtocol (
                    Handles[HandleIndex],
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&SimpleFs
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (PathIndex = 0; mKmhExternalImagePaths[PathIndex] != NULL; PathIndex++) {
      Buffer = NULL;
      BufferSize = 0;
      Status = KmhReadFileFromVolume (
                 SimpleFs,
                 mKmhExternalImagePaths[PathIndex],
                 &Buffer,
                 &BufferSize
                 );
      if (!EFI_ERROR (Status)) {
        *ImageBase = (UINT64)(UINTN)Buffer;
        *ImageSize = BufferSize;
        Print (
          L"BdsLoaderTool: External Image loaded: %s base=0x%lx size=0x%lx\n",
          mKmhExternalImagePaths[PathIndex],
          *ImageBase,
          *ImageSize
          );
        FreePool (Handles);
        return EFI_SUCCESS;
      }
    }
  }

  FreePool (Handles);
  return EFI_NOT_FOUND;
}

STATIC
VOID *
KmhGetEfiEntryPoint (
  IN UINT64  ImageBase
  )
{
  EFI_IMAGE_DOS_HEADER         *DosHeader;
  EFI_IMAGE_FILE_HEADER        *CoffHeader;
  EFI_IMAGE_OPTIONAL_HEADER32  *OptionalHeader;
  UINT32                       PeMagic;

  DosHeader = (EFI_IMAGE_DOS_HEADER *)(UINTN)ImageBase;
  if (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
    Print (L"BdsLoaderTool: DOS magic=0x%x, expected 0x5A4D\n", DosHeader->e_magic);
    return NULL;
  }

  PeMagic = *(UINT32 *)(UINTN)(ImageBase + sizeof (EFI_IMAGE_DOS_HEADER));
  if (PeMagic != EFI_IMAGE_PE_SIGNATURE) {
    Print (L"BdsLoaderTool: PE magic=0x%x, expected 0x00004550\n", PeMagic);
    return NULL;
  }

  CoffHeader = (EFI_IMAGE_FILE_HEADER *)(UINTN)(ImageBase + sizeof (EFI_IMAGE_DOS_HEADER) + sizeof (UINT32));
  if ((CoffHeader->Machine != EFI_IMAGE_FILE_MACHINE_RISCV32) &&
      (CoffHeader->Machine != EFI_IMAGE_FILE_MACHINE_RISCV64))
  {
    Print (L"BdsLoaderTool: Machine=0x%x, expected RISC-V\n", CoffHeader->Machine);
    return NULL;
  }

  OptionalHeader = (EFI_IMAGE_OPTIONAL_HEADER32 *)(UINTN)(
                     ImageBase + sizeof (EFI_IMAGE_DOS_HEADER) + sizeof (UINT32) + sizeof (EFI_IMAGE_FILE_HEADER)
                     );
  if ((OptionalHeader->Magic != EFI_IMAGE_PE_OPTIONAL_HDR32_MAGIC) &&
      (OptionalHeader->Magic != EFI_IMAGE_PE_OPTIONAL_HDR32_PLUS_MAGIC))
  {
    Print (L"BdsLoaderTool: Optional magic=0x%x\n", OptionalHeader->Magic);
    return NULL;
  }

  return (VOID *)(UINTN)(ImageBase + OptionalHeader->AddressOfEntryPoint);
}

STATIC
UINT32
KmhGetEfiEntryRva (
  IN UINT64  ImageBase
  )
{
  VOID                         *EntryPoint;
  EFI_IMAGE_OPTIONAL_HEADER32  *OptionalHeader;

  EntryPoint = KmhGetEfiEntryPoint (ImageBase);
  if (EntryPoint == NULL) {
    return 0;
  }

  OptionalHeader = (EFI_IMAGE_OPTIONAL_HEADER32 *)(UINTN)(
                     ImageBase + sizeof (EFI_IMAGE_DOS_HEADER) + sizeof (UINT32) + sizeof (EFI_IMAGE_FILE_HEADER)
                     );
  return OptionalHeader->AddressOfEntryPoint;
}

STATIC
EFI_STATUS
KmhCopyKernelImage (
  IN  UINT64  SourceBase,
  IN  UINTN   PeImageSize,
  OUT UINT64  *CopiedBase,
  OUT UINTN   *CopiedSize,
  OUT VOID    **CopiedEntryPoint
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  AllocBase;
  UINTN                 AllocSize;
  UINTN                 AllocPages;
  UINT32                EntryRva;

  EntryRva = KmhGetEfiEntryRva (SourceBase);
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

  Status = gBS->AllocatePages (AllocateAnyPages, EfiLoaderCode, AllocPages, &AllocBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ZeroMem ((VOID *)(UINTN)AllocBase, AllocSize);
  CopyMem ((VOID *)(UINTN)AllocBase, (VOID *)(UINTN)SourceBase, PeImageSize);
  InvalidateInstructionCacheRange ((VOID *)(UINTN)AllocBase, PeImageSize);

  *CopiedBase = AllocBase;
  *CopiedSize = AllocSize;
  *CopiedEntryPoint = (VOID *)(UINTN)(AllocBase + EntryRva);

  Print (
    L"BdsLoaderTool: Kernel copy src=0x%lx pe-size=0x%lx dst=0x%lx alloc-size=0x%lx entry=0x%p\n",
    SourceBase,
    PeImageSize,
    *CopiedBase,
    *CopiedSize,
    *CopiedEntryPoint
    );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
KmhConstructLoadedImage (
  IN  EFI_SYSTEM_TABLE                 *SystemTable,
  OUT KMH_LOADED_IMAGE_PRIVATE_DATA    *Image,
  IN  UINT64                           ImageBase,
  IN  UINTN                            ImageSize
  )
{
  ZeroMem (Image, sizeof (*Image));
  Image->Info.SystemTable = SystemTable;
  Image->Info.DeviceHandle = NULL;
  Image->Info.Revision = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
  Image->Info.FilePath = NULL;
  Image->Info.ImageBase = (VOID *)(UINTN)ImageBase;
  Image->Info.ImageSize = ImageSize;
  Image->NumberOfPages = EFI_SIZE_TO_PAGES (ImageSize);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BdsLoaderToolMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                       Status;
  UINT64                           SourceImageBase;
  UINT64                           ImageBase;
  UINTN                            ImageSize;
  UINTN                            ImagePages;
  VOID                             *EntryPoint;
  EFI_IMAGE_ENTRY_POINT            Entry;
  KMH_LOADED_IMAGE_PRIVATE_DATA    *Image;
  VOID                             *ExternalImageBuffer;

  (VOID)ImageHandle;

  Print (L"BdsLoaderTool: standard BDS entry started\n");

  SourceImageBase = FixedPcdGet64 (PcdKernelBase);
  ExternalImageBuffer = NULL;

  Status = KmhReadExternalImage (&SourceImageBase, &ImageSize);
  if (!EFI_ERROR (Status)) {
    ExternalImageBuffer = (VOID *)(UINTN)SourceImageBase;
  } else {
    Print (L"BdsLoaderTool: external Image not found, abort: %r\n", Status);
    return Status;
  }

  Print (
    L"BdsLoaderTool: ImageBase=0x%lx ImageSize=0x%lx PcdKernelSize=0x%lx\n",
    SourceImageBase,
    ImageSize,
    FixedPcdGet64 (PcdKernelSize)
    );

  EntryPoint = KmhGetEfiEntryPoint (SourceImageBase);
  if (EntryPoint == NULL) {
    if (ExternalImageBuffer != NULL) {
      FreePool (ExternalImageBuffer);
    }
    return EFI_LOAD_ERROR;
  }

  Status = KmhCopyKernelImage (SourceImageBase, ImageSize, &ImageBase, &ImageSize, &EntryPoint);
  if (ExternalImageBuffer != NULL) {
    FreePool (ExternalImageBuffer);
    ExternalImageBuffer = NULL;
  }
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ImagePages = EFI_SIZE_TO_PAGES (ImageSize);
  Image = AllocateZeroPool (sizeof (*Image));
  if (Image == NULL) {
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = KmhConstructLoadedImage (SystemTable, Image, ImageBase, ImageSize);
  if (EFI_ERROR (Status)) {
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    FreePool (Image);
    return Status;
  }

  Status = KmhSetupCmdLineFromFdtBootargs (&Image->Info);
  if (EFI_ERROR (Status)) {
    Print (L"BdsLoaderTool: use empty cmdline because DT bootargs are unavailable\n");
  }

  Status = gBS->InstallProtocolInterface (
                  &Image->Handle,
                  &gEfiLoadedImageProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &Image->Info
                  );
  if (EFI_ERROR (Status)) {
    if (Image->Info.LoadOptionsSize > 0) {
      FreePool (Image->Info.LoadOptions);
    }
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
    FreePool (Image);
    return Status;
  }

  Entry = (EFI_IMAGE_ENTRY_POINT)EntryPoint;
  Print (
    L"BdsLoaderTool: jump to Linux EFI stub EntryPoint=0x%p Handle=0x%llx SystemTable=0x%llx\n\n",
    Entry,
    Image->Handle,
    SystemTable
    );

  Status = Entry (Image->Handle, SystemTable);

  if (Image->Info.LoadOptionsSize > 0) {
    FreePool (Image->Info.LoadOptions);
  }
  gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ImageBase, ImagePages);
  FreePool (Image);

  return Status;
}
