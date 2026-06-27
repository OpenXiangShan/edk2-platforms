/** @file
  Timer-backed non-secure EFI_RNG_PROTOCOL provider for Kunminghu.

  This driver exists to satisfy network stack users such as Udp4Dxe when the
  platform has no hardware RNG device. It advertises only EdkiiRngAlgorithmUnSafe.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Guid/RngAlgorithm.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Rng.h>

STATIC UINT64  mTimerRngState;

STATIC
UINT64
TimerRngNext64 (
  VOID
  )
{
  UINT64  Value;

  Value = mTimerRngState ^ GetPerformanceCounter ();
  if (Value == 0) {
    Value = 0x9E3779B97F4A7C15ULL;
  }

  Value ^= Value << 13;
  Value ^= Value >> 7;
  Value ^= Value << 17;

  mTimerRngState = Value + 0x9E3779B97F4A7C15ULL;
  return Value;
}

STATIC
EFI_STATUS
EFIAPI
TimerRngGetInfo (
  IN     EFI_RNG_PROTOCOL   *This,
  IN OUT UINTN              *RNGAlgorithmListSize,
  OUT    EFI_RNG_ALGORITHM  *RNGAlgorithmList
  )
{
  if (RNGAlgorithmListSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*RNGAlgorithmListSize < sizeof (EFI_RNG_ALGORITHM)) {
    *RNGAlgorithmListSize = sizeof (EFI_RNG_ALGORITHM);
    return EFI_BUFFER_TOO_SMALL;
  }

  if (RNGAlgorithmList == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyGuid (RNGAlgorithmList, &gEdkiiRngAlgorithmUnSafe);
  *RNGAlgorithmListSize = sizeof (EFI_RNG_ALGORITHM);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
TimerRngGetRng (
  IN  EFI_RNG_PROTOCOL   *This,
  IN  EFI_RNG_ALGORITHM  *RNGAlgorithm  OPTIONAL,
  IN  UINTN              RNGValueLength,
  OUT UINT8              *RNGValue
  )
{
  UINT64  Value;
  UINTN   Offset;
  UINTN   Chunk;

  if ((RNGValue == NULL) || (RNGValueLength == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((RNGAlgorithm != NULL) &&
      !CompareGuid (RNGAlgorithm, &gEdkiiRngAlgorithmUnSafe))
  {
    return EFI_UNSUPPORTED;
  }

  Offset = 0;
  while (Offset < RNGValueLength) {
    Value = TimerRngNext64 ();
    Chunk = MIN (sizeof (Value), RNGValueLength - Offset);
    CopyMem (RNGValue + Offset, &Value, Chunk);
    Offset += Chunk;
  }

  return EFI_SUCCESS;
}

STATIC EFI_RNG_PROTOCOL  mTimerRngProtocol = {
  TimerRngGetInfo,
  TimerRngGetRng
};

EFI_STATUS
EFIAPI
TimerRngDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HANDLE  Handle;

  mTimerRngState = GetPerformanceCounter () ^
                   (UINTN)&mTimerRngProtocol ^
                   (UINTN)ImageHandle;
  Handle = NULL;

  DEBUG ((DEBUG_INFO, "%a: installing non-secure EFI_RNG_PROTOCOL\n", __func__));
  return gBS->InstallMultipleProtocolInterfaces (
                &Handle,
                &gEfiRngProtocolGuid,
                &mTimerRngProtocol,
                NULL
                );
}
