/** @file
 UART Serial Port library functions

 Copyright (c) 2022, BOSC. All rights reserved.

 SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/SerialPortLib.h>
#include <Include/BoscXSUart.h>

//---------------------------------------------
// UART Register Offsets
//---------------------------------------------
#define UART_RBR_OFFSET		0	/* In:  Recieve Buffer Register */
#define UART_THR_OFFSET		0	/* Out: Transmitter Holding Register */
#define UART_DLL_OFFSET		0	/* Out: Divisor Latch Low */
#define UART_IER_OFFSET		1	/* I/O: Interrupt Enable Register */
#define UART_DLH_OFFSET		1	/* Out: Divisor Latch High */
#define UART_FCR_OFFSET		2	/* Out: FIFO Control Register */
#define UART_IIR_OFFSET		2	/* I/O: Interrupt Identification Register */
#define UART_LCR_OFFSET		3	/* Out: Line Control Register */
#define UART_MCR_OFFSET		4	/* Out: Modem Control Register */
#define UART_LSR_OFFSET		5	/* In:  Line Status Register */
#define UART_MSR_OFFSET		6	/* In:  Modem Status Register */
#define UART_SCR_OFFSET		7	/* I/O: Scratch Register */
#define UART_MDR1_OFFSET	8	/* I/O: Mode Register */
#define UART_USR_OFFSET   31/* I/O: UART Status Register */

#define UART_LSR_FIFOE		0x80	/* Fifo error */
#define UART_LSR_TEMT		  0x40	/* Transmitter empty */
#define UART_LSR_THRE		  0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI		    0x10	/* Break interrupt indicator */
#define UART_LSR_FE		    0x08	/* Frame error indicator */
#define UART_LSR_PE		    0x04	/* Parity error indicator */
#define UART_LSR_OE		    0x02	/* Overrun error indicator */
#define UART_LSR_DR		    0x01	/* Receiver data ready */
#define UART_LSR_BRK_ERROR_BITS	0x1E	/* BI, FE, PE, OE bits */

#define UART_DATA_TERMINAL_READY        (1 << 0)
#define UART_REQUEST_TO_SEND            (1 << 1)
#define UART_LOOPBACK_BIT               (1 << 4)
#define UART_AUTO_FLOW_CONTROL          (1 << 5)

#define UART_CLEAR_TO_SEND              (1 << 4)
#define UART_DATA_SET_READY             (1 << 5)
#define UART_RING_INDICATOR             (1 << 6)
#define UART_CARRIER_DETECT             (1 << 7)

//---------------------------------------------
// UART Settings
//---------------------------------------------
#define UART_BAUDRATE  9600
#define SYS_CLK        FixedPcdGet32(PcdU5PlatformSystemClock)


/**
  Get value from serial port register.

  @param  RegIndex   Register index

  @retval Vale returned from from serial port.

**/
UINT32 GetReg (
  IN UINT32 RegIndex
  )
{
  STATIC volatile UINT32 * const uart = (UINT32 *)FixedPcdGet32(PcdU5UartBase);
  return readl ((volatile void *)(uart + RegIndex));
}

/**
  Set serial port register.

  @param RegIndex   Register index
  @param Value      Value write to Register

**/
VOID SetReg (
  IN UINT32 RegIndex,
  IN UINT32 Value
  )
{
  STATIC volatile UINT32 * const uart = (UINT32 *)FixedPcdGet32(PcdU5UartBase);
  writel (Value, (volatile void *)(uart + RegIndex));
}

/**
  Character output to serial port.

  @param Ch        The character to serial port.

**/
VOID UartPutChar (
  IN UINT8 Ch
  )
{
  while((GetReg(UART_LSR_OFFSET) & UART_LSR_THRE) == 0)
    ;
  SetReg(UART_THR_OFFSET, Ch);
}

/**
  Get character from serial port.

  @retval character        The character from serial port.

**/
UINT32 UartGetChar (VOID)
{
  if (GetReg(UART_LSR_OFFSET) & UART_LSR_DR)
    return GetReg(UART_RBR_OFFSET);
  return -1;
}

/**
  baudrate = serial clock freq / ( 16 * divisor )
  divisor  =  serial clock freq / ( 16 * baudrate )
  @param Freq         The given clock to UART.
  @param MaxTargetHZ  Target baudrate.

**/
UINT32
UartMinClkDivisor (
  IN UINT64 Freq,
  IN UINT64 MaxTargetHZ
  )
{
  UINT64 Quotient;
  Quotient = Freq / ( MaxTargetHZ * 16 );
  return Quotient;
}


/**
  Wait for the serial port to be idle
**/
VOID
SerialWaitForIdle ()
{
  while(GetReg(UART_USR_OFFSET) != 0);
}

/**
  Get current CLK Divisor
**/
UINT32
UartCurrentClkDivisor ()
{
  UINT32 Quotient;
  UINT32 LCR;
  while(GetReg(UART_USR_OFFSET) != 0);
  LCR = GetReg(UART_LCR_OFFSET);
  SetReg(UART_LCR_OFFSET, LCR | 0x80);
  Quotient = GetReg(UART_DLL_OFFSET);
  SetReg(UART_LCR_OFFSET, LCR);
  return Quotient;
}

/**
  Initialize the serial device hardware.

  If no initialization is required, then return RETURN_SUCCESS.
  If the serial device was successfuly initialized, then return RETURN_SUCCESS.
  If the serial device could not be initialized, then return RETURN_DEVICE_ERROR.

  @retval RETURN_SUCCESS        The serial device was initialized.
  @retval RETURN_DEVICE_ERROR   The serail device could not be initialized.

**/
EFI_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  UINT32 Divisor;
  UINT32 CurrentDivisor;

  Divisor = UartMinClkDivisor (SYS_CLK, UART_BAUDRATE);
  if (Divisor == 0) {
    return EFI_INVALID_PARAMETER;
  }
  CurrentDivisor = UartCurrentClkDivisor();
  if (Divisor != CurrentDivisor) {
    uart8250_init(FixedPcdGet32(PcdU5UartBase), SYS_CLK, UART_BAUDRATE, 2, 4);
  }
  return EFI_SUCCESS;
}

/**
  Write data from buffer to serial device.

  Writes NumberOfBytes data bytes from Buffer to the serial device.
  The number of bytes actually written to the serial device is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.

  If Buffer is NULL, then ASSERT().

  If NumberOfBytes is zero, then return 0.

  @param  Buffer           Pointer to the data buffer to be written.
  @param  NumberOfBytes    Number of bytes to written to the serial device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the write operation failed.

**/
UINTN
EFIAPI
SerialPortWrite (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  UINTN Index;

  if (Buffer == NULL) {
    return 0;
  }

  for(Index = 0; Index < NumberOfBytes; Index ++) {
    UartPutChar (Buffer [Index]);
  }

  return Index;
}

/**
  Reads data from a serial device into a buffer.

  @param  Buffer           Pointer to the data buffer to store the data read from the serial device.
  @param  NumberOfBytes    Number of bytes to read from the serial device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes read from the serial device.
                           If this value is less than NumberOfBytes, then the read operation failed.

**/
UINTN
EFIAPI
SerialPortRead (
  OUT UINT8     *Buffer,
  IN  UINTN     NumberOfBytes
  )
{
  UINTN Index;

  if (NULL == Buffer) {
    return 0;
  }

  for (Index = 0; Index < NumberOfBytes; Index ++) {
    Buffer [Index] = (UINT8)UartGetChar ();
  }

  return Index;
}

/**
  Polls a serial device to see if there is any data waiting to be read.

  Polls aserial device to see if there is any data waiting to be read.
  If there is data waiting to be read from the serial device, then TRUE is returned.
  If there is no data waiting to be read from the serial device, then FALSE is returned.

  @retval TRUE             Data is waiting to be read from the serial device.
  @retval FALSE            There is no data waiting to be read from the serial device.

**/
BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  STATIC volatile UINT32 * const uart = (UINT32 *)FixedPcdGet32(PcdU5UartBase);
  UINT32 IP;

  IP = MmioRead32 ((UINTN)(uart + UART_LSR_OFFSET));
  if (IP & UART_LSR_DR) {
    return TRUE;
  }
  return FALSE;
}

/**
  Sets the control bits on a serial device.

  @param Control                Sets the bits of Control that are settable.

  @retval RETURN_SUCCESS        The new control bits were set on the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32 Control
  )
{
  UINT32 Mcr;
  if ((Control & (~(EFI_SERIAL_REQUEST_TO_SEND | EFI_SERIAL_DATA_TERMINAL_READY |
                    EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE))) != 0)
  {
    return RETURN_UNSUPPORTED;
  }
  Mcr = GetReg(UART_MCR_OFFSET);
  Mcr &= (~(UART_DATA_TERMINAL_READY | UART_REQUEST_TO_SEND));

  if ((Control & EFI_SERIAL_DATA_TERMINAL_READY) == EFI_SERIAL_DATA_TERMINAL_READY) {
    Mcr |= UART_DATA_TERMINAL_READY;
  }

  if ((Control & EFI_SERIAL_REQUEST_TO_SEND) == EFI_SERIAL_REQUEST_TO_SEND) {
    Mcr |= UART_REQUEST_TO_SEND;
  }

  SetReg(UART_MCR_OFFSET, Mcr);

  return RETURN_SUCCESS;
}

/**
  Retrieve the status of the control bits on a serial device.

  @param Control                A pointer to return the current control signals from the serial device.

  @retval RETURN_SUCCESS        The control bits were read from the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32 *Control
  )
{
  UINT32 Mcr;
  UINT32 Msr;
  UINT32 Lsr;
  Mcr = GetReg(UART_MCR_OFFSET);
  if ((Mcr & UART_LOOPBACK_BIT) == UART_LOOPBACK_BIT){
    *Control |= EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE;
  }
  if ((Mcr & UART_AUTO_FLOW_CONTROL) == UART_AUTO_FLOW_CONTROL) {
    *Control |= EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE;
  }
  if ((Mcr & UART_REQUEST_TO_SEND) == UART_REQUEST_TO_SEND) {
    *Control |= EFI_SERIAL_REQUEST_TO_SEND;
  }
  if ((Mcr & UART_DATA_TERMINAL_READY) == UART_DATA_TERMINAL_READY) {
    *Control |= EFI_SERIAL_DATA_TERMINAL_READY;
  }

  Msr = GetReg(UART_MSR_OFFSET);
  if ((Msr & UART_CLEAR_TO_SEND) == UART_CLEAR_TO_SEND) {
    *Control |= EFI_SERIAL_CLEAR_TO_SEND;
  }
  if ((Msr & UART_DATA_SET_READY) == UART_DATA_SET_READY) {
    *Control |= EFI_SERIAL_DATA_SET_READY;
  }
  if ((Msr & UART_RING_INDICATOR) == UART_RING_INDICATOR) {
    *Control |= EFI_SERIAL_RING_INDICATE;
  }
  if ((Msr & UART_CARRIER_DETECT) == UART_CARRIER_DETECT) {
    *Control |= EFI_SERIAL_CARRIER_DETECT;
  }

  Lsr = GetReg(UART_LSR_OFFSET);
  if ((Lsr & (UART_LSR_TEMT | UART_LSR_THRE)) == (UART_LSR_TEMT | UART_LSR_THRE)) {
    *Control |= EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
  }
  if ((Lsr & UART_LSR_DR) == 0) {
    *Control |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
  }
  return RETURN_SUCCESS;
}

/**
  Sets the baud rate, receive FIFO depth, transmit/receice time out, parity,
  data bits, and stop bits on a serial device.

  @param BaudRate           The requested baud rate. A BaudRate value of 0 will use the
                            device's default interface speed.
                            On output, the value actually set.
  @param ReveiveFifoDepth   The requested depth of the FIFO on the receive side of the
                            serial interface. A ReceiveFifoDepth value of 0 will use
                            the device's default FIFO depth.
                            On output, the value actually set.
  @param Timeout            The requested time out for a single character in microseconds.
                            This timeout applies to both the transmit and receive side of the
                            interface. A Timeout value of 0 will use the device's default time
                            out value.
                            On output, the value actually set.
  @param Parity             The type of parity to use on this serial device. A Parity value of
                            DefaultParity will use the device's default parity value.
                            On output, the value actually set.
  @param DataBits           The number of data bits to use on the serial device. A DataBits
                            vaule of 0 will use the device's default data bit setting.
                            On output, the value actually set.
  @param StopBits           The number of stop bits to use on this serial device. A StopBits
                            value of DefaultStopBits will use the device's default number of
                            stop bits.
                            On output, the value actually set.

  @retval RETURN_SUCCESS            The new attributes were set on the serial device.
  @retval RETURN_UNSUPPORTED        The serial device does not support this operation.
  @retval RETURN_INVALID_PARAMETER  One or more of the attributes has an unsupported value.
  @retval RETURN_DEVICE_ERROR       The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64             *BaudRate,
  IN OUT UINT32             *ReceiveFifoDepth,
  IN OUT UINT32             *Timeout,
  IN OUT EFI_PARITY_TYPE    *Parity,
  IN OUT UINT8              *DataBits,
  IN OUT EFI_STOP_BITS_TYPE *StopBits
  )
{
  UINT32  SerialBaudRate;
  UINTN   Divisor;
  UINT8   Lcr;
  UINT8   LcrData;
  UINT8   LcrParity;
  UINT8   LcrStop;

  if (*BaudRate == 0) {
    *BaudRate = UART_BAUDRATE;
  }

  SerialBaudRate = *BaudRate;

  if (*DataBits == 0) {
    LcrData = 0x3;
    *DataBits = LcrData + 5;
  } else {
    if ((*DataBits < 5) || (*DataBits > 8)) {
      return RETURN_INVALID_PARAMETER;
    }
    LcrData = (UINT8)(*DataBits - (UINT8)5);
  }

  if (*Parity == DefaultParity) {
    LcrParity = 0;
    *Parity = NoParity;
  } else {
    switch (*Parity) {
      case NoParity:
        LcrParity = 0;
        break;
      case EvenParity:
        LcrParity = 3;
        break;
      case OddParity:
        LcrParity = 1;
        break;
      case SpaceParity:
        LcrParity = 7;
        break;
      case MarkParity:
        LcrParity = 5;
        break;
      default:
        return RETURN_INVALID_PARAMETER;
    }
  }

  if (*StopBits == DefaultStopBits) {
    LcrStop = 0;
    *StopBits = OneStopBit;
  } else {
    switch (*StopBits) {
      case OneStopBit:
        LcrStop = 0;
        break;
      case OneFiveStopBits:
      case TwoStopBits:
        LcrStop = 1;
        break;
      default:
        return RETURN_INVALID_PARAMETER;
    }
  }

  Divisor = SYS_CLK / (SerialBaudRate * 16);
  // Set Baudrate
  SetReg(UART_LCR_OFFSET,0x80);
  SetReg(UART_DLH_OFFSET, (UINT8)(Divisor >> 8));
  SetReg(UART_DLL_OFFSET, (UINT8)(Divisor & 0xFF));

  // Set 
  Lcr = (UINT8)((LcrParity << 3) | (LcrStop << 2) | LcrData);
  SetReg(UART_LCR_OFFSET, Lcr & 0x3F);

  return RETURN_SUCCESS;
}