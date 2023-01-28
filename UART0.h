/*
 * UART.h
 *
 *  Created on: Jan 27, 2023
 *      Author: travp
 */

#include <string>

#ifndef UART_H_
#define UART_H_

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1

// standard ASCII symbols
#define CR   0x0D
#define LF   0x0A
#define BS   0x08
#define ESC  0x1B
#define SP   0x20
#define DEL  0x7F


// Abstraction of general output device
// Volume 2 section 3.4.5

//------------Output_Init------------
// Initialize the UART for 115,200 baud rate (assuming 16 MHz bus clock),
// 8 bit word length, no parity bits, one stop bit, FIFOs enabled
// Input: none
// Output: none
void UART0_Init(void);

char UART0_InChar(void);

//------------UART0_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void UART0_OutChar(char data);

void UART0_OutString(const std::string& data);

// Clear display
void Output_Clear(void);

// Turn off display (low power)
void Output_Off(void);

// Turn on display
void Output_On(void);

// set the color for future output
void Output_Color(uint32_t newColor);



#endif /* UART_H_ */
