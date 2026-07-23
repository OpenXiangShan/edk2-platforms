/** @file
  This file include all platform actions

Copyright (c) 2021-2022, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PlatformBootManager.h"

EFI_GUID  mUefiShellFileGuid = {
  0x7C04A583, 0x9E3E, 0x4f1c, { 0xAD, 0x65, 0xE0, 0x52, 0x68, 0xD0, 0xB4, 0xD1 }
};

EFI_GUID  mKmhBdsLoaderToolFileGuid = {
  0x4A4B2A66, 0x9E59, 0x4C62, { 0xBD, 0x69, 0x7C, 0x93, 0x2B, 0x9F, 0x6C, 0x41 }
};

typedef struct {
  EFI_GUID       FileGuid;
  CONST CHAR8    *Name;
} PLATFORM_FV_DRIVER;

STATIC CONST PLATFORM_FV_DRIVER  mConsoleDrivers[] = {
  {
    { 0xD6099B94, 0xCD97, 0x4CC5, { 0x87, 0x14, 0x7F, 0x63, 0x12, 0x70, 0x1A, 0x8A } },
    "VirtioGpuDxe"
  },
  {
    { 0xCCCB0C28, 0x4B24, 0x11D5, { 0x9A, 0x5A, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } },
    "GraphicsConsoleDxe"
  },
  {
    { 0x737F7F4B, 0xD375, 0x4B19, { 0xB7, 0x5F, 0xEA, 0x9E, 0x5D, 0x6B, 0x27, 0x2B } },
    "VirtioInputDxe"
  }
};

STATIC CONST PLATFORM_FV_DRIVER  mNetworkDrivers[] = {
  {
    { 0x025BBFC7, 0xE6A9, 0x4B8B, { 0x82, 0xAD, 0x68, 0x15, 0xA1, 0xAE, 0xAF, 0x4A } },
    "MnpDxe"
  },
  {
    { 0x529D3F93, 0xE8E9, 0x4E73, { 0xB1, 0xE1, 0xBD, 0xF6, 0xA9, 0xD5, 0x01, 0x13 } },
    "ArpDxe"
  },
  {
    { 0x9FB1A1F3, 0x3B71, 0x4324, { 0xB3, 0x9A, 0x74, 0x5C, 0xBB, 0x01, 0x5F, 0xFF } },
    "Ip4Dxe"
  },
  {
    { 0x6D6963AB, 0x906D, 0x4A65, { 0xA7, 0xCA, 0xBD, 0x40, 0xE5, 0xD6, 0xAF, 0x2B } },
    "Udp4Dxe"
  },
  {
    { 0x94734718, 0x0BBC, 0x47FB, { 0x96, 0xA5, 0xEE, 0x7A, 0x5A, 0xE6, 0xA2, 0xAD } },
    "Dhcp4Dxe"
  }
};

STATIC CONST ACPI_HID_DEVICE_PATH  mVirtioSerialPort0Node = {
  {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
      (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
    },
  },
  EISA_PNP_ID (0x0501),
  0
};

STATIC CONST UART_DEVICE_PATH  mVirtioSerialUartNode = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_UART_DP,
    {
      (UINT8)(sizeof (UART_DEVICE_PATH)),
      (UINT8)((sizeof (UART_DEVICE_PATH)) >> 8)
    },
  },
  0,
  115200,
  8,
  1,
  1
};

STATIC CONST VENDOR_DEVICE_PATH  mVirtioSerialTerminalNode = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_VENDOR_DP,
    {
      (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
      (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
    },
  },
  DEVICE_PATH_MESSAGING_PC_ANSI
};

STATIC
VOID
PlatformBootManagerAddVirtioConsole (
  IN EFI_HANDLE  Handle
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *OldDevicePath;

  DevicePath = DevicePathFromHandle (Handle);
  if (DevicePath == NULL) {
    return;
  }

  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *)&mVirtioSerialPort0Node
                 );
  OldDevicePath = DevicePath;
  DevicePath    = AppendDevicePathNode (
                    DevicePath,
                    (EFI_DEVICE_PATH_PROTOCOL *)&mVirtioSerialUartNode
                    );
  FreePool (OldDevicePath);

  OldDevicePath = DevicePath;
  DevicePath    = AppendDevicePathNode (
                    DevicePath,
                    (EFI_DEVICE_PATH_PROTOCOL *)&mVirtioSerialTerminalNode
                    );
  FreePool (OldDevicePath);

  Status = EfiBootManagerUpdateConsoleVariable (ConIn, DevicePath, NULL);
  DEBUG ((DEBUG_INFO, "Virtio CONSOLE_IN variable set %s : %r\n", ConvertDevicePathToText (DevicePath, FALSE, FALSE), Status));

  Status = EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  DEBUG ((DEBUG_INFO, "Virtio CONSOLE_OUT variable set %s : %r\n", ConvertDevicePathToText (DevicePath, FALSE, FALSE), Status));

  Status = EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);
  DEBUG ((DEBUG_INFO, "Virtio STD_ERROR variable set %s : %r\n", ConvertDevicePathToText (DevicePath, FALSE, FALSE), Status));

  FreePool (DevicePath);
}

STATIC
VOID
PlatformBootManagerDetectVirtioConsoles (
  VOID
  )
{
  EFI_STATUS              Status;
  UINTN                   HandleCount;
  EFI_HANDLE              *HandleBuffer;
  UINTN                   Index;
  VIRTIO_DEVICE_PROTOCOL  *VirtIo;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gVirtioDeviceProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gVirtioDeviceProtocolGuid,
                    (VOID **)&VirtIo
                    );
    if (!EFI_ERROR (Status) &&
        (VirtIo->SubSystemDeviceId == VIRTIO_SUBSYSTEM_CONSOLE))
    {
      PlatformBootManagerAddVirtioConsole (HandleBuffer[Index]);
    }
  }

  FreePool (HandleBuffer);
}

STATIC
BOOLEAN
PlatformBootManagerFvDriverIsLoaded (
  IN CONST EFI_GUID  *FileGuid
  )
{
  EFI_STATUS                       Status;
  UINTN                            HandleCount;
  EFI_HANDLE                       *HandleBuffer;
  UINTN                            Index;
  EFI_LOADED_IMAGE_PROTOCOL        *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL         *Node;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FvFileNode;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiLoadedImageProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiLoadedImageProtocolGuid,
                    (VOID **)&LoadedImage
                    );
    if (EFI_ERROR (Status) || (LoadedImage->FilePath == NULL)) {
      continue;
    }

    for (Node = LoadedImage->FilePath; !IsDevicePathEnd (Node); Node = NextDevicePathNode (Node)) {
      if ((DevicePathType (Node) == MEDIA_DEVICE_PATH) &&
          (DevicePathSubType (Node) == MEDIA_PIWG_FW_FILE_DP) &&
          (DevicePathNodeLength (Node) == sizeof (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH)))
      {
        FvFileNode = (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node;
        if (CompareGuid (&FvFileNode->FvFileName, FileGuid)) {
          FreePool (HandleBuffer);
          return TRUE;
        }
      }
    }
  }

  FreePool (HandleBuffer);
  return FALSE;
}

STATIC
EFI_STATUS
PlatformBootManagerStartFvDriver (
  IN CONST EFI_GUID  *FileGuid,
  IN CONST CHAR8     *Name
  )
{
  EFI_STATUS                         Status;
  EFI_HANDLE                         ImageHandle;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;

  if (PlatformBootManagerFvDriverIsLoaded (FileGuid)) {
    DEBUG ((DEBUG_INFO, "%a is already loaded, skip FV start\n", Name));
    return EFI_ALREADY_STARTED;
  }

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EfiInitializeFwVolDevicepathNode (&FileNode, (EFI_GUID *)FileGuid);
  DevicePath = AppendDevicePathNode (
                 DevicePathFromHandle (LoadedImage->DeviceHandle),
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->LoadImage (
                  FALSE,
                  gImageHandle,
                  DevicePath,
                  NULL,
                  0,
                  &ImageHandle
                  );
  FreePool (DevicePath);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Load %a from FV failed: %r\n", Name, Status));
    return Status;
  }

  Status = gBS->StartImage (ImageHandle, NULL, NULL);
  if (EFI_ERROR (Status)) {
    gBS->UnloadImage (ImageHandle);
    DEBUG ((DEBUG_ERROR, "Start %a failed: %r\n", Name, Status));
  } else {
    DEBUG ((DEBUG_INFO, "Start %a: %r\n", Name, Status));
  }

  return Status;
}

STATIC
VOID
PlatformBootManagerStartNetworkDrivers (
  VOID
  )
{
  UINTN  Index;

  for (Index = 0; Index < ARRAY_SIZE (mNetworkDrivers); Index++) {
    PlatformBootManagerStartFvDriver (
      &mNetworkDrivers[Index].FileGuid,
      mNetworkDrivers[Index].Name
      );
  }
}

STATIC
VOID
PlatformBootManagerStartConsoleDrivers (
  VOID
  )
{
  UINTN  Index;

  for (Index = 0; Index < ARRAY_SIZE (mConsoleDrivers); Index++) {
    PlatformBootManagerStartFvDriver (
      &mConsoleDrivers[Index].FileGuid,
      mConsoleDrivers[Index].Name
      );
  }
}

STATIC
VOID
PlatformBootManagerAddHandlesToConsole (
  IN EFI_GUID             *ProtocolGuid,
  IN CONSOLE_TYPE         ConsoleType,
  IN CONST CHAR8          *Name
  )
{
  EFI_STATUS                Status;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  ProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    DevicePath = DevicePathFromHandle (HandleBuffer[Index]);
    if (DevicePath == NULL) {
      continue;
    }

    Status = EfiBootManagerUpdateConsoleVariable (
               ConsoleType,
               DevicePath,
               NULL
               );
    DEBUG ((
      DEBUG_INFO,
      "%a console variable set %s : %r\n",
      Name,
      ConvertDevicePathToText (DevicePath, FALSE, FALSE),
      Status
      ));
  }

  FreePool (HandleBuffer);
}

/**
  Perform the platform diagnostic, such like test memory. OEM/IBV also
  can customize this function to support specific platform diagnostic.

  @param MemoryTestLevel  The memory test intensive level
  @param QuietBoot        Indicate if need to enable the quiet boot

**/
VOID
PlatformBootManagerDiagnostics (
  IN EXTENDMEM_COVERAGE_LEVEL  MemoryTestLevel,
  IN BOOLEAN                   QuietBoot
  )
{
  EFI_STATUS  Status;

  //
  // Here we can decide if we need to show
  // the diagnostics screen
  // Notes: this quiet boot code should be remove
  // from the graphic lib
  //
  if (QuietBoot) {
    //
    // Perform system diagnostic
    //
    Status = PlatformBootManagerMemoryTest (MemoryTestLevel);
    return;
  }

  //
  // Perform system diagnostic
  //
  Status = PlatformBootManagerMemoryTest (MemoryTestLevel);
}

/**
  Return the index of the load option in the load option array.

  The function consider two load options are equal when the
  OptionType, Attributes, Description, FilePath and OptionalData are equal.

  @param Key    Pointer to the load option to be found.
  @param Array  Pointer to the array of load options to be found.
  @param Count  Number of entries in the Array.

  @retval -1          Key wasn't found in the Array.
  @retval 0 ~ Count-1 The index of the Key in the Array.
**/
INTN
PlatformFindLoadOption (
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION  *Key,
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION  *Array,
  IN UINTN                               Count
  )
{
  UINTN  Index;

  for (Index = 0; Index < Count; Index++) {
    if ((Key->OptionType == Array[Index].OptionType) &&
        (Key->Attributes == Array[Index].Attributes) &&
        (StrCmp (Key->Description, Array[Index].Description) == 0) &&
        (CompareMem (Key->FilePath, Array[Index].FilePath, GetDevicePathSize (Key->FilePath)) == 0) &&
        (Key->OptionalDataSize == Array[Index].OptionalDataSize) &&
        (CompareMem (Key->OptionalData, Array[Index].OptionalData, Key->OptionalDataSize) == 0))
    {
      return (INTN)Index;
    }
  }

  return -1;
}

STATIC
INTN
EFIAPI
PlatformCompareKmhBdsLoaderFirst (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  CONST EFI_BOOT_MANAGER_LOAD_OPTION  *Option1;
  CONST EFI_BOOT_MANAGER_LOAD_OPTION  *Option2;
  BOOLEAN                             Option1IsKmhBdsLoader;
  BOOLEAN                             Option2IsKmhBdsLoader;

  Option1 = (CONST EFI_BOOT_MANAGER_LOAD_OPTION *)Buffer1;
  Option2 = (CONST EFI_BOOT_MANAGER_LOAD_OPTION *)Buffer2;

  Option1IsKmhBdsLoader = (BOOLEAN)(StrCmp (Option1->Description, L"KMH BdsLoaderTool") == 0);
  Option2IsKmhBdsLoader = (BOOLEAN)(StrCmp (Option2->Description, L"KMH BdsLoaderTool") == 0);

  if (Option1IsKmhBdsLoader && !Option2IsKmhBdsLoader) {
    return -1;
  }

  if (!Option1IsKmhBdsLoader && Option2IsKmhBdsLoader) {
    return 1;
  }

  if (Option1->OptionNumber < Option2->OptionNumber) {
    return -1;
  }

  if (Option1->OptionNumber > Option2->OptionNumber) {
    return 1;
  }

  return 0;
}

/**
  Register a boot option using a file GUID in the FV.

  @param FileGuid     The file GUID name in FV.
  @param Description  The boot option description.
  @param Attributes   The attributes used for the boot option loading.
**/
VOID
PlatformRegisterFvBootOption (
  EFI_GUID  *FileGuid,
  CHAR16    *Description,
  UINT32    Attributes
  )
{
  EFI_STATUS                         Status;
  UINTN                              OptionIndex;
  EFI_BOOT_MANAGER_LOAD_OPTION       NewOption;
  EFI_BOOT_MANAGER_LOAD_OPTION       *BootOptions;
  UINTN                              BootOptionCount;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;

  Status = gBS->HandleProtocol (gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
  ASSERT_EFI_ERROR (Status);

  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = AppendDevicePathNode (
                 DevicePathFromHandle (LoadedImage->DeviceHandle),
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             Attributes,
             Description,
             DevicePath,
             NULL,
             0
             );
  if (!EFI_ERROR (Status)) {
    BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);

    OptionIndex = PlatformFindLoadOption (&NewOption, BootOptions, BootOptionCount);

    if (OptionIndex == -1) {
      Status = EfiBootManagerAddLoadOptionVariable (&NewOption, (UINTN)-1);
      ASSERT_EFI_ERROR (Status);
    }

    EfiBootManagerFreeLoadOption (&NewOption);
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
  }
}

/**
  Do the platform specific action before the console is connected.

  Such as:
    Update console variable;
    Register new Driver#### or Boot####;
    Signal ReadyToLock event.
**/
VOID
EFIAPI
PlatformBootManagerBeforeConsole (
  VOID
  )
{
  UINTN                         Index;
  EFI_STATUS                    Status;
  EFI_INPUT_KEY                 Enter;
  EFI_INPUT_KEY                 F2;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption;

  //
  // Signal EndOfDxe PI Event
  //
  EfiEventGroupSignal (&gEfiEndOfDxeEventGroupGuid);

  //
  // Update the console variables.
  //
  for (Index = 0; gPlatformConsole[Index].DevicePath != NULL; Index++) {
    DEBUG ((DEBUG_INFO, "Check gPlatformConsole %d\n", Index));
    if ((gPlatformConsole[Index].ConnectType & CONSOLE_IN) == CONSOLE_IN) {
      Status = EfiBootManagerUpdateConsoleVariable (ConIn, gPlatformConsole[Index].DevicePath, NULL);
      DEBUG ((DEBUG_INFO, "CONSOLE_IN variable set %s : %r\n", ConvertDevicePathToText (gPlatformConsole[Index].DevicePath, FALSE, FALSE), Status));
    }

    if ((gPlatformConsole[Index].ConnectType & CONSOLE_OUT) == CONSOLE_OUT) {
      Status = EfiBootManagerUpdateConsoleVariable (ConOut, gPlatformConsole[Index].DevicePath, NULL);
      DEBUG ((DEBUG_INFO, "CONSOLE_OUT variable set %s : %r\n", ConvertDevicePathToText (gPlatformConsole[Index].DevicePath, FALSE, FALSE), Status));
    }

    if ((gPlatformConsole[Index].ConnectType & STD_ERROR) == STD_ERROR) {
      Status = EfiBootManagerUpdateConsoleVariable (ErrOut, gPlatformConsole[Index].DevicePath, NULL);
      DEBUG ((DEBUG_INFO, "STD_ERROR variable set %r", Status));
    }
  }

  PlatformBootManagerStartConsoleDrivers ();
  EfiBootManagerConnectAll ();
  PlatformBootManagerAddHandlesToConsole (
    &gEfiGraphicsOutputProtocolGuid,
    ConOut,
    "GOP CONSOLE_OUT"
    );
  PlatformBootManagerAddHandlesToConsole (
    &gEfiGraphicsOutputProtocolGuid,
    ErrOut,
    "GOP STD_ERROR"
    );
  PlatformBootManagerAddHandlesToConsole (
    &gEfiSimpleTextInProtocolGuid,
    ConIn,
    "SimpleTextIn CONSOLE_IN"
    );

  PlatformBootManagerDetectVirtioConsoles ();

  //
  // Register ENTER as CONTINUE key
  //
  Enter.ScanCode    = SCAN_NULL;
  Enter.UnicodeChar = CHAR_CARRIAGE_RETURN;
  EfiBootManagerRegisterContinueKeyOption (0, &Enter, NULL);
  //
  // Map F2 to Boot Manager Menu
  //
  F2.ScanCode    = SCAN_F2;
  F2.UnicodeChar = CHAR_NULL;
  EfiBootManagerGetBootManagerMenu (&BootOption);
  EfiBootManagerAddKeyOptionVariable (NULL, (UINT16)BootOption.OptionNumber, 0, &F2, NULL);
  //
  // Register UEFI Shell
  //
  PlatformRegisterFvBootOption (&mUefiShellFileGuid, L"UEFI Shell", LOAD_OPTION_ACTIVE);
  PlatformRegisterFvBootOption (
    &mKmhBdsLoaderToolFileGuid,
    L"KMH BdsLoaderTool",
    LOAD_OPTION_ACTIVE
    );
  EfiBootManagerSortLoadOptionVariable (
    LoadOptionTypeBoot,
    PlatformCompareKmhBdsLoaderFirst
    );
}

/**
  Do the platform specific action after the console is connected.

  Such as:
    Dynamically switch output mode;
    Signal console ready platform customized event;
    Run diagnostics like memory testing;
    Connect certain devices;
    Dispatch additional option roms.
**/
VOID
EFIAPI
PlatformBootManagerAfterConsole (
  VOID
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  White;

  Black.Blue = Black.Green = Black.Red = Black.Reserved = 0;
  White.Blue = White.Green = White.Red = White.Reserved = 0xFF;

  PlatformBootManagerStartNetworkDrivers ();

  EfiBootManagerConnectAll ();
  EfiBootManagerRefreshAllBootOption ();

  PlatformBootManagerDiagnostics (QUICK, TRUE);

  PrintXY (10, 10, &White, &Black, L"F2    to enter Boot Manager Menu.                                            ");
  PrintXY (10, 30, &White, &Black, L"Enter to boot directly.");
}

/**
  The function is called when no boot option could be launched,
  including platform recovery options and options pointing to applications
  built into firmware volumes.

  If this function returns, BDS attempts to enter an infinite loop.
**/
VOID
EFIAPI
PlatformBootManagerUnableToBoot (
  VOID
  )
{
  return;
}
