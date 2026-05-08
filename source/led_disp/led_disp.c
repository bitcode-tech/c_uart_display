/******************************************************************************
*       UART Single-Wire LED Display Interface Driver Library
*
* Program description: Driver function library for the display interface of BC759x chips
* Copyright: Beijing Lingzhi BitCode Technology Co., Ltd. https://bitcode.com.cn
* Version: V4.1
* Initial version: March 30, 2021
* Version history:
*   March 2021   V1.0
*   April 2021   V2.0  Fixed a FIFO operation error
*   April 2021   V2.1  Fixed the DIRECT_WT spelling error and reformatted the code
*                       for compatibility with older C standards such as C89
*   April 2021   V3.0  Removed the interrupt-disable operation after buffered
*                       transmission completes, to support devices that do not
*                       distinguish between TX and RX interrupts
*                       Renamed API functions to unify the API with the BC727X
*                       driver library:
*                         uart_tx_ready()      ---> tx_ready()
*                         set_tx_eni_func()    ---> set_eni_func()
*                         set_tx_disi_func()   ---> set_disi_func()
*                         set_uart_wt_func()   ---> set_write_func()
*   April 2021   V3.1  Fixed the issue where zeros in the middle of display_dec()
*                       were not displayed
*   May 2021     V3.2  Added leading-zero display support to display_dec()
*   May 2022     V4.0  Added the display_float() function
*   April 2023   V4.1  Fixed the DIG0 display bug in display_dec() and added
*                       C89 standard support
*
* Usage:
*   Add this header file to all C source files that need to use functions from
*   this driver library, and add led_disp.c to the project source file list.
*   Then the functions in this library can be called directly from the user
*   program. The key_scan directory may also need to be added to the project's
*   include search path.
*   For detailed usage, refer to the "UART Single-Wire LED Display Interface
*   Driver Library Technical Specification".
******************************************************************************/
#include "led_disp.h"

const uint8_t BlinkBit[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
const uint32_t Mul[8] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

/* If the UART works in interrupt mode, provide a FIFO */
#if UART_MODE_INTERRUPT == 1

typedef struct
{
    volatile uint8_t FifoFreeSpace;
    volatile uint8_t Busy;
    uint8_t          NextWritePos;
    volatile uint8_t NextReadPos;
    uint8_t          BlinkCtlL;
    uint8_t          BlinkCtlH;
    uint8_t          UartFifo[UART_FIFO_SIZE];
    void (*pUartWrite)(uint8_t);
    void (*pEnableTxInt)(void);
    void (*pDisableTxInt)(void);
} display_t;

#if __STDC_VERSION__ >= 199901l
display_t Display = {
    .FifoFreeSpace = UART_FIFO_SIZE
};
#else
display_t Display = {UART_FIFO_SIZE};
#endif

/******************************************************************************
* Send command
* This function can be used to send any command to a BC759x chip. The library
* only provides several high-level commonly used display functions. If the user
* needs full control of the BC759x, this function should be called directly.
* All high-level functions operate the BC759x by calling this function.
* Input parameters:
*     uint8_t Cmd   - Command to be sent
*     uint8_t Data  - Data to be sent
* Return value: None
******************************************************************************/
void send_cmd(uint8_t Cmd, uint8_t Data)
{
    while (Display.FifoFreeSpace == 0)
        ; /* Check whether there is still space in the FIFO */
    if (Display.pDisableTxInt != NULL)
    {
        Display.pDisableTxInt(); /* Disable the UART interrupt to prevent conflicts */
    }
    Display.UartFifo[Display.NextWritePos] = Cmd;                                               /* Write new data */
    Display.NextWritePos                   = (Display.NextWritePos + 1) & (UART_FIFO_SIZE - 1); /* Move the write pointer to the next element */
    --Display.FifoFreeSpace;                                                                    /* Decrease the FIFO free-space count by 1 */
    if (!Display.Busy)                                                                          /* If the FIFO was already empty before this function call (automatic transmission had stopped) */
    {
        Display.Busy = 1;
        if (Display.pUartWrite != NULL)
        {
            Display.pUartWrite(Display.UartFifo[Display.NextReadPos]); /* Start UART transmission */
        }
        Display.NextReadPos = (Display.NextReadPos + 1) & (UART_FIFO_SIZE - 1); /* Move the read pointer to the next element */
        ++Display.FifoFreeSpace;
    }
    if (Display.pEnableTxInt != NULL)
    {
        Display.pEnableTxInt(); /* Re-enable the UART interrupt */
    }
    while (Display.FifoFreeSpace == 0) /* Continue writing the data byte to the FIFO; first check whether there is still space in the FIFO */
        ;
    if (Display.pDisableTxInt != NULL)
    {
        Display.pDisableTxInt(); /* Disable the UART interrupt to prevent conflicts */
    }
    --Display.FifoFreeSpace;                                                                    /* Decrease the FIFO free-space count by 1 */
    Display.UartFifo[Display.NextWritePos] = Data;                                              /* Write new data */
    Display.NextWritePos                   = (Display.NextWritePos + 1) & (UART_FIFO_SIZE - 1); /* Move the write pointer to the next element */
    if (Display.pEnableTxInt != NULL)
    {
        Display.pEnableTxInt(); /* Re-enable the UART interrupt */
    }
}

/******************************************************************************
* UART transmission ready
* Usually called from the UART interrupt. After data is written to the UART,
* an interrupt is generally generated when transmission is complete and the next
* byte can be sent. The user calls this function from the UART interrupt service
* routine to notify the driver library that it may send the next byte from the
* FIFO.
* Input parameters: None
* Return value: None
******************************************************************************/
void tx_ready(void)
{
    if (Display.FifoFreeSpace < UART_FIFO_SIZE) /* If the FIFO has not been fully transmitted */
    {
        if (Display.pUartWrite != NULL)
        {
            Display.pUartWrite(Display.UartFifo[Display.NextReadPos]); /* Send the next character */
        }
        Display.NextReadPos = (Display.NextReadPos + 1) & (UART_FIFO_SIZE - 1);
        ++Display.FifoFreeSpace; /* Increase available space by 1 */
    }
    else /* If the FIFO is already empty */
    {
        Display.Busy = 0;
    }
}

/******************************************************************************
* Set the interrupt-enable function
* When the driver library uses UART interrupt mode, it must be able to enable and
* disable the interrupt. The user should wrap this operation in a function and
* use this library function to tell the driver which function to call to enable
* the UART interrupt.
* Input parameter: Pointer to the UART interrupt-enable function. This function
*                  must take no input parameters and return void.
* Return value: None
******************************************************************************/
void set_eni_func(void (*pTxIntEnFunc)(void))
{
    Display.pEnableTxInt = pTxIntEnFunc;
}

/******************************************************************************
* Set the UART interrupt-disable function
* When the driver library uses UART interrupt mode, it must be able to enable and
* disable the interrupt. The user should wrap this operation in a function and
* use this library function to tell the driver which function to call to disable
* the UART interrupt.
* Input parameter: Pointer to the UART interrupt-enable function. This function
*                  must take no input parameters and return void.
* Return value: None
******************************************************************************/
void set_disi_func(void (*pTxIntDisFunc)(void))
{
    Display.pDisableTxInt = pTxIntDisFunc;
}

#else /* UART uses non-interrupt mode */

struct display_t
{
    uint8_t BlinkCtlL;
    uint8_t BlinkCtlH;
    void (*pUartWrite)(uint8_t);
};

struct display_t Display;

/******************************************************************************
* Send command
* This function can be used to send any command to a BC759x chip. The library
* only provides several high-level commonly used display functions. If the user
* needs full control of the BC759x, this function should be called directly.
* All high-level functions operate the BC759x by calling this function.
* Input parameters:
*     uint8_t Cmd   - Command to be sent
*     uint8_t Data  - Data to be sent
* Return value: None
******************************************************************************/
void send_cmd(uint8_t Cmd, uint8_t Data)
{
    Display.pUartWrite(Cmd);
    Display.pUartWrite(Data);
}

#endif

/******************************************************************************
* Set the UART write function
* The user must wrap the UART write operation in a function. This function must
* take one uint8_t parameter as input and return void. Before using the driver
* library, the user must call this function to tell the driver which function is
* used as the UART write function.
* Input parameter: Pointer to the UART write function. The UART write function
*                  must take a uint8_t input parameter and return void.
* Return value: void
******************************************************************************/
void set_write_func(void (*pUartWriteFunc)(uint8_t))
{
    Display.pUartWrite = pUartWriteFunc;
}

/******************************************************************************
* Clear display
* This function clears all display content and resets all blinking display
* attributes to the non-blinking state.
* Input parameters: None
* Return value: None
******************************************************************************/
void clear(void)
{
    uint8_t i;
    send_cmd(WRITE_ALL, 0x00);
    for (i = 0; i < 0x10; i++)
    {
        send_cmd(BLINK_WT_CLR | i, 0xff);
    }
    send_cmd(BLINK_DIG_CTL, 0x00);
    send_cmd(BLINK_DIG_CTL + 1, 0x00);
    Display.BlinkCtlH = 0;
    Display.BlinkCtlL = 0;
}

/******************************************************************************
* Display a decimal number
* This function displays the input value in decimal format. Only unsigned values
* are accepted. To display negative values, the user must display the '-' sign
* separately. If the higher-order digits of the input value exceed the display
* range of the chip, the excess part will not be displayed and no error message
* will be returned. If the configured display width is smaller than the actual
* number width, the excess part will be ignored. If it is larger than the actual
* number width, the extra positions will be displayed as blanks.
* Input parameters:
*     uint32_t Val  - Value to be displayed, range 0 to 4,294,967,295
*     uint8_t Pos   - Position where the ones digit is displayed. The display
*                     direction is determined by LOW_DIG_NUM_ON_RIGHT in
*                     disp_config.h. When this value is 1, the ones digit is
*                     displayed at display position Pos, and higher-order digits
*                     use display positions with higher indexes in sequence.
*     uint8_t Width - The lower 7 bits indicate the numeric display width,
*                     counted from the lowest digit. When the highest bit is 1,
*                     leading zeros are not suppressed.
******************************************************************************/
void display_dec(uint32_t Val, uint8_t Pos, uint8_t Width)
{
    if ((Pos > 31) || ((Width & 0x7f) == 0))
    {
        return;
    }
    send_cmd(DECODE_WT | Pos, Val % 10);
    Val = Val / 10;
#if (LOW_DIG_NUM_ON_RIGHT == 1)
    if (++Pos > 31)
    {
        return;
    }
    while ((--Width & 0x7f) > 0)
    {
        if ((Val >= 10) || (Width & 0x80))
        {
            send_cmd(DECODE_WT | Pos, Val % 10);
        }
        else
        {
            send_cmd(DECODE_WT | Pos, (Val % 10) | 0x80);
        }
        Val = Val / 10;
        if (++Pos > 31)
        {
            return;
        }
    }
#else
    if (Pos-- == 0)
    {
        return;
    }
    while ((--Width & 0x7f) > 0)
    {
        if ((Val >= 10) || (Width & 0x80))
        {
            send_cmd(DECODE_WT | Pos, Val % 10);
        }
        else
        {
            send_cmd(DECODE_WT | Pos, (Val % 10) | 0x80);
        }
        Val = Val / 10;
        if (Pos-- == 0)
        {
            return;
        }
    }
#endif
}


/******************************************************************************
* Display a floating-point number display_float()
* Displays a floating-point number as a decimal fraction, but does not display
* the decimal point or the sign.
* Parameters:
*      Val    -- Floating-point number to be displayed
*      Pos    -- Lowest digit display position, that is, the position of the
*                lowest digit after the decimal point. It is used in the same
*                way as the corresponding parameter in display_dec().
*      Width  -- Display width of the value counted from the lowest digit,
*                including both the fractional and integer parts. Other usage is
*                the same as display_dec().
*      Frac   -- Number of digits after the decimal point
******************************************************************************/
void display_float(float Val, uint8_t Pos, uint8_t Width, uint8_t Frac)
{
	uint32_t IntVal;
	if (((Width&0x7f) >= Frac) && (Frac <8))
	{
		if (Val < 0)
		{
			Val = -Val;
		}
		IntVal = (uint32_t)(Val*Mul[Frac]+0.5);
		display_dec(IntVal, Pos, Frac|0x80);
#if LOW_DIG_NUM_ON_RIGHT == 1
		display_dec(IntVal/Mul[Frac], Pos+Frac, Width-Frac);
#else
		display_dec(IntVal/Mul[Frac], Pos-Frac, Width-Frac);
#endif
	}
}


/******************************************************************************
* Display a hexadecimal number
* This function displays the input value in hexadecimal format. The maximum input
* value of this function is 0xffff, but larger numbers can be displayed by
* splitting them and calling this function multiple times.
* If the higher-order digits of the input value exceed the display range of the
* chip, the excess part will not be displayed and no error message will be
* returned.
* Input parameters:
*     uint32_t Val  - Value to be displayed, range 0 to 0xffff
*     uint8_t Pos   - Position where the lowest digit is displayed. The display
*                     direction is determined by LOW_DIG_NUM_ON_RIGHT in
*                     disp_config.h. When this value is 1, the lowest digit is
*                     displayed at display position Pos, and higher-order digits
*                     use display positions with higher indexes in sequence.
*     uint8_t Width - Display width. If the display width is smaller than the
*                     actual value width, only the digits within the width are
*                     displayed. If the display width is larger than the actual
*                     number width, the extra positions are filled with '0'.
******************************************************************************/
void display_hex(uint16_t Val, uint8_t Pos, uint8_t Width)
{
    uint8_t i;
    if (Pos > 31)
    {
        return;
    }
#if (LOW_DIG_NUM_ON_RIGHT == 1)
    if (Width + Pos > 32)
    {
        Width = 32 - Pos;
    }
    for (i = Pos; i < Width + Pos; i++)
    {
        send_cmd(DECODE_WT | i, Val & 0x0f);
        Val >>= 4;
    }
#else
    if (Width > Pos + 1)
    {
        Width = Pos + 1;
    }
    for (i = 0; i < Width; i++)
    {
        send_cmd(DECODE_WT | (Pos - i), Val & 0x0f);
        Val >>= 4;
    }
#endif
}

/******************************************************************************
* Digit blinking control
* Controls blinking by digit. This function controls the blinking attribute of
* one display position each time. If the blinking control state of digits 16 to
* 31 has been directly changed by the user through send_cmd(), settings made by
* direct commands may be lost when this function is later used to control
* blinking for digits 16 to 31.
* Input parameters:
*     uint8_t Digit  - Digit index, range 0 to 31
*     uint8_t OnOff  - Blinking state to set. 0 = not blinking, 1 = blinking
* Return value: None
******************************************************************************/
void digit_blink(uint8_t Digit, uint8_t OnOff)
{
    if (Digit > 31)
    {
        return;
    }
    if (Digit < 16)
    {
        if (OnOff)
        {
            send_cmd(BLINK_WT_SET | Digit, 0xff);
        }
        else
        {
            send_cmd(BLINK_WT_CLR | Digit, 0xff);
        }
    }
    else
    {
        if (Digit & 0x08)
        {
            Display.BlinkCtlH = OnOff ? Display.BlinkCtlH | BlinkBit[Digit & 0x07] : Display.BlinkCtlH & (~BlinkBit[Digit & 0x07]);
            send_cmd(BLINK_DIG_CTL + 1, Display.BlinkCtlH);
        }
        else
        {
            Display.BlinkCtlL = OnOff ? Display.BlinkCtlL | BlinkBit[Digit & 0x07] : Display.BlinkCtlL & (~BlinkBit[Digit & 0x07]);
            send_cmd(BLINK_DIG_CTL, Display.BlinkCtlL);
        }
    }
}
