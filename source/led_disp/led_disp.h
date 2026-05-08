#ifndef LED_DISP_H
#define LED_DISP_H

/******************************************************************************
*       UART Single-Wire LED Display Interface Driver Library
*
* Description: Driver function library for the display interface of BC759x chips
* Copyright: Beijing Lingzhi Bitcode Technology Co., Ltd. https://bitcode.com.cn
* Version: V4.1
* Initial version: March 30, 2021
* Version history:
*   March 2021   V1.0
*   April 2021   V2.0  Fixed FIFO operation errors
*   April 2021   V2.1  Fixed the spelling error of DIRECT_WT, and reformatted the code to
*                       be compatible with C89 and other earlier C language standards
*   April 2021   V3.0  Removed the interrupt-disable operation when buffer transmission ends,
*                       to support devices that do not distinguish between transmit and
*                       receive interrupts
*                       Renamed API functions to unify the API interface with the BC727X
*                       driver library:
*                     uart_tx_ready() ---> tx_ready()
*                     set_tx_eni_func() ---> set_eni_func()
*                     set_tx_disi_func() ---> set_disi_func()
*                     set_uart_wt_func() ---> set_write_func()
*   April 2021   V3.1  Fixed the issue where middle zeros were not displayed in display_dec()
*   May 2021     V3.2  Added leading-zero display support to display_dec()
*   May 2022     V4.0  Added the display_float() function
*   April 2023   V4.1  Fixed the DIG0 display bug in display_dec() and added C89 support
*
* Usage:
*   Add this header file to all C source files that need to use functions from this driver
*   library, and add led_disp.c to the project source file list. Then the functions in this
*   library can be called directly from the user program. It may be necessary to add the
*   key_scan directory to the project's include search path.
*   For detailed usage, refer to the "UART Single-Wire LED Display Interface Driver Library
*   Technical Specification".
******************************************************************************/
#include <stdint.h>
/*
stdint.h contains the definitions of data types such as uint8_t. If your environment uses
an older standard than C99 or does not provide stdint.h, please define them yourself as follows:
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
*/

/* Library-related settings, including display direction, buffer size, etc. */
#include "disp_config.h"

#ifndef NULL
#define NULL (0)
#endif

/* Check whether the configuration definition is missing. If missing, use the default value. */
#ifndef LOW_DIG_NUM_ON_RIGHT
#define LOW_DIG_NUM_ON_RIGHT 1
#elif (LOW_DIG_NUM_ON_RIGHT != 1) && (LOW_DIG_NUM_ON_RIGHT != 0)
#error LOW_DIG_NUM_ON_RIGHT must be 1(default) or 0
#endif

/* Check whether the configuration definition is missing. If missing, use the default value. */
#ifndef UART_MODE_INTERRUPT
#define UART_MODE_INTERRUPT 1
#elif (UART_MODE_INTERRUPT != 1) && (UART_MODE_INTERRUPT != 0)
#error UART_MODE_INTERRUPT must be 0 or 1(default)
#endif

#ifndef UART_FIFO_SIZE
#define UART_FIFO_SIZE (8)
#endif

/***** BC759x commands *****/
#define DIRECT_WT     (0x00)
#define COL_WRITE     (0x00)
#define BLINK_WT_CLR  (0x20)
#define BLINK_WT_SET  (0x30)
#define SHIFT_H_WT    (0x40)
#define ROTATE_R      (0x5f)
#define ROTATE_L      (0x60)
#define SHIFT_l_WT    (0x61)
#define DECODE_WT     (0x80)
#define QTR_WT_BOT    (0xa0)
#define QTR_INS_BOT   (0xa4)
#define QTR_WT_TOP    (0xa8)
#define WRITE_EXT     (0xa8)
#define QTR_INS_TOP   (0xac)
#define DECODE_EXT    (0xb0)
#define SEG_OFF       (0xc0)
#define COORD_OFF     (0xc0)
#define SEG_ON        (0xc1)
#define COORD_ON      (0xc1)
#define BLINK_DIG_CTL (0xd0)
#define GLOBAL_CTL    (0xf0)
#define WRITE_ALL     (0xf1)
#define BLINK_SPEED   (0xf2)
#define DIM_CTL       (0xf3)
#define RESET_H       (0xff)
#define RESET_L       (0x5a)
#define UART_SEND_0_H (0xff)
#define UART_SEND_0_L (0xff)

#ifdef __cplusplus
extern "C" {
#endif

/* If the UART works in interrupt mode, some additional functions are required. */
#if UART_MODE_INTERRUPT == 1

/******************************************************************************
* UART transmit ready
* Usually called from the UART interrupt. After data is written to the UART, an interrupt is
* usually generated when the data transmission is complete and the next byte can be sent.
* This function is called by the user from the UART interrupt to notify the driver library
* that it may send the next byte in the FIFO.
* Input parameters: None
* Return value: None
******************************************************************************/
void tx_ready(void);

/******************************************************************************
* Set the interrupt enable function
* When the driver library uses UART interrupt mode, it must be able to enable and disable
* interrupts. The user should encapsulate this operation as a function and use this library
* function to tell the driver which function to call to enable the UART interrupt.
* Input parameter: Pointer to the UART interrupt enable function. This function must have
*                  void input and void return type.
* Return value: None
******************************************************************************/
void set_eni_func(void (*pTxIntEnFunc)(void));

/******************************************************************************
* Set the UART interrupt disable function
* When the driver library uses UART interrupt mode, it must be able to enable and disable
* interrupts. The user should encapsulate this operation as a function and use this library
* function to tell the driver which function to call to disable the UART interrupt.
* Input parameter: Pointer to the UART interrupt enable function. This function must have
*                  void input and void return type.
* Return value: None
******************************************************************************/
void set_disi_func(void (*pTxIntDisFunc)(void));

#endif

/******************************************************************************
* Set the UART write function
* The user must encapsulate the UART write operation as a function. This function must take
* one uint8_t parameter as input and return void. Before using the driver library, the user
* must call this function to tell the driver which function is the UART write function.
* Input parameter: Pointer to the UART write function. The UART write function must take
*                  uint8_t as the input parameter and return void.
* Return value: void
******************************************************************************/
void set_write_func(void (*pUartWriteFunc)(uint8_t));

/******************************************************************************
* Send command
* This function can be used to send any command to the BC759x. The library only provides
* several commonly used high-level display functions. If the user needs full control of the
* BC759x, this function should be called. All high-level functions operate the BC759x through
* this function.
* Input parameters:
*     uint8_t Cmd   - Command to be sent
*     uint8_t Data  - Data to be sent
* Return value: None
******************************************************************************/
void send_cmd(uint8_t Cmd, uint8_t Data);

/******************************************************************************
* Clear display
* This function clears all display contents and resets all blinking display attributes to
* non-blinking.
* Input parameters: None
* Return value: None
******************************************************************************/
void clear(void);

/******************************************************************************
* Display a decimal number
* This function displays the input value in decimal format. It only accepts unsigned values;
* if a negative value needs to be displayed, the user must display the '-' sign separately.
* If the higher-order digits of the input value exceed the chip's display range, the excess
* part will not be displayed and no error message will be given. If the configured display
* width is smaller than the actual number width, the excess part will be ignored. If it is
* larger than the actual number width, the excess part will be displayed as blank.
* Input parameters:
*     uint32_t Val  - Value to be displayed, range 0 to 4,294,967,295
*     uint8_t Pos   - Position where the ones digit is displayed. The number display direction
*                     is determined by LOW_DIG_NUM_ON_RIGHT in disp_config.h. When this value
*                     is 1, the ones digit is displayed at display position Pos, and then
*                     higher-numbered display positions are used in sequence.
*     uint8_t Width - The lower 7 bits indicate the display width of the value starting from
*                     the lowest digit. When the highest bit is 1, leading '0' characters are
*                     not suppressed.
******************************************************************************/
void display_dec(uint32_t Val, uint8_t Pos, uint8_t Width);


/******************************************************************************
* Display a floating-point number display_float()
* Displays a floating-point number as a decimal value, but does not display the decimal point
* or the sign.
* Parameters: Val    -- Floating-point number to be displayed
*             Pos    -- Lowest digit display position, that is, the position of the lowest
*                       digit after the decimal point. Usage is the same as the corresponding
*                       parameter in display_dec().
*             Width  -- Display width of the value starting from the lowest digit, including
*                       both the fractional and integer parts. Other behavior is the same as
*                       display_dec().
*             Frac   -- Number of digits after the decimal point
******************************************************************************/
void display_float(float Val, uint8_t Pos, uint8_t Width, uint8_t Frac);


/******************************************************************************
* Display a hexadecimal number
* This function displays the input value in hexadecimal format. The maximum input value of
* this function is 0xffff, but larger numbers can be displayed by splitting them and calling
* this function multiple times.
* If the higher-order digits of the input value exceed the chip's display range, the excess
* part will not be displayed and no error message will be given.
* Input parameters:
*     uint32_t Val  - Value to be displayed, range 0 to 0xffff
*     uint8_t Pos   - Position where the lowest digit is displayed. The number display direction
*                     is determined by LOW_DIG_NUM_ON_RIGHT in disp_config.h. When this value
*                     is 1, the lowest digit is displayed at display position Pos, and then
*                     higher-numbered display positions are used in sequence.
*     uint8_t Width - Display width. If the display width is smaller than the actual value
*                     width, only the digit positions within the width are displayed. If the
*                     display width is larger than the actual number width, the extra positions
*                     are padded with '0'.
******************************************************************************/
void display_hex(uint16_t Val, uint8_t Pos, uint8_t Width);

/******************************************************************************
* Digit blinking control
* Controls blinking by digit position. This function controls the blinking attribute of one
* display position each time. For the blinking control of digits 16 to 31, if the user directly
* changes the state through send_cmd(), then uses this function to control blinking for digits
* 16 to 31, the settings made through the direct command may be lost.
* Input parameters:
*     uint8_t Digit  - Digit number, range 0-31
*     uint8_t OnOff  - Blinking state to set. 0 = not blinking, 1 = blinking
* Return value: None
******************************************************************************/
void digit_blink(uint8_t Digit, uint8_t OnOff);


#ifdef __cplusplus
}
#endif

#endif
