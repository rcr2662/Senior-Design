//*****************************************************************************
//
// uart_echo.c - Example for reading data from and writing data to the UART in
//               an interrupt driven fashion.
//
// Copyright (c) 2012-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.4.178 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>UART Echo (uart_echo)</h1>
//!
//! This example application utilizes the UART to echo text.  The first UART
//! (connected to the USB debug virtual serial port on the evaluation board)
//! will be configured in 115,200 baud, 8-n-1 mode.  All characters received on
//! the UART are transmitted back to the UART.
//
//*****************************************************************************

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

// user-made function declarations
void init_LED();
void init_UART0();
void init_UART1();
void init_IO_pins();
void init_5V_output();
void test_IO();

//*****************************************************************************
//
// The UART interrupt handler.
//
//*****************************************************************************
void
UARTIntHandler(void)
{
    uint32_t ui32Status;

    //
    // Get the interrrupt status.
    //
    ui32Status = ROM_UARTIntStatus(UART0_BASE, true);

    //
    // Clear the asserted interrupts.
    //
    ROM_UARTIntClear(UART0_BASE, ui32Status);

    //
    // Loop while there are characters in the receive FIFO.
    //
    while(ROM_UARTCharsAvail(UART0_BASE))
    {
        //
        // Read the next character from the UART and write it back to the UART.
        //
        ROM_UARTCharPutNonBlocking(UART0_BASE,
                                   ROM_UARTCharGetNonBlocking(UART0_BASE));

        //
        // Blink the LED to show a character transfer is occuring.
        //
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);

        //
        // Delay for 1 millisecond.  Each SysCtlDelay is about 3 clocks.
        //
        SysCtlDelay(SysCtlClockGet() / (1000 * 3));

        //
        // Turn off the LED
        //
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);

    }
}

//*****************************************************************************
//
// Send a string to the UART.
//
//*****************************************************************************
void UARTSendString(uint32_t ui32Base, const uint8_t *pui8Buffer, uint32_t ui32Count){
    int i;
    for(i=0; i<ui32Count; i++){
        UARTCharPut(ui32Base, pui8Buffer[i]);
    }
}

// Read string from the UART
void UARTReadString(uint32_t ui32Base, uint8_t * rxBuffer){
    // load user response into rx_buffer
    int i=0;
    do{
        rxBuffer[i] = (uint8_t) UARTCharGet(ui32Base);
        i++;
    }while((char) rxBuffer[i-1] != '\n'); // might need to consider '\r' as a message terminator as well
}


// UART rx_buffer
uint8_t rx_buffer[10];

// State Enumerations
enum MODES {
  PROGRAMMING_MODE,
  READOUT_MODE,
  UNKNOWN
};

enum states
{
    PC_initial,
    PC_report,
    Tx_mode,
    Rx_mode
};

// Programming Variables
uint8_t serial_number[4] = {0};
uint8_t calibration_data[4] = {0};

//*****************************************************************************
//
// This example demonstrates how to send a string of data to the UART.
//
//*****************************************************************************
int
main(void)
{
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    ROM_FPUEnable();
    ROM_FPULazyStackingEnable();

    //
    // Set the clocking to run directly from the crystal.
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);

    //
    // Enable processor interrupts.
    //
    ROM_IntMasterEnable();

    init_LED();
    init_UART0();
    //init_UART1();
    init_IO_pins();
    init_5V_output();

    // UART Variables
    memset(rx_buffer, 0, sizeof(*rx_buffer));

    // State machine variables
    enum states state = PC_initial;
    enum states next_state;

    // Communication variables and flags
    enum MODES OPERATING_MODE;
    OPERATING_MODE = UNKNOWN;

    // function is used to test digital output to modulator
    // contains an infinite loop
    test_IO();
    /*
    while(1)
    {
        //tesing UARTReadString function
//        UARTSendString(UART0_BASE, (uint8_t *)"Program or Readout? (P/R): ", 27);
//        UARTReadString(UART0_BASE, &rx_buffer);
//        UARTSendString(UART0_BASE, (uint8_t *) rx_buffer, 10);

        switch(state){

            case PC_initial:
                while(1){
                    if(OPERATING_MODE == UNKNOWN){
                        // prompt user input
                        UARTSendString(UART0_BASE, (uint8_t *)"Program or Readout? (P/R): ", 27);

                        // load user response into rx_buffer
                        UARTReadString(UART0_BASE, &rx_buffer);

                        // interpret user response and enter desired mode of operation
                        if(rx_buffer[0] == 'P'){
                            //UARTSend(UART0_BASE, (uint8_t *)"Entering Programming Mode: \n", 28);
                            //GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_2 | GPIO_PIN_3);
                            OPERATING_MODE = PROGRAMMING_MODE;
                        }else if (rx_buffer[0] == 'R'){
                            //UARTSend(UART0_BASE, (uint8_t *)"Entering Readout Mode: \n", 24);
                            //GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3, 0);
                            OPERATING_MODE = READOUT_MODE;
                        }else{
                            UARTSendString(UART0_BASE, (uint8_t *)"Invalid Input\n", 14);
                        }

                    }else if(OPERATING_MODE == PROGRAMMING_MODE){
                        // enter programming mode
                        UARTSendString(UART0_BASE, (uint8_t *)"Programming Mode: \n", 19);
                        // prompt user for serial number
                        UARTSendString(UART0_BASE, (uint8_t *)"Enter Serial Number (4 characters)\n", 35);
                        // read serial number
                        UARTReadString(UART0_BASE, &rx_buffer);
                        memcpy(serial_number, rx_buffer, 4);

                        // clear rx_buffer between data reads
                        memset(rx_buffer, 0, sizeof(*rx_buffer));

                        // prompt user for calibration data
                        UARTSendString(UART0_BASE, (uint8_t *)"Enter Calibration Data (4 characters)\n", 38);
                        // read calibration data
                        UARTReadString(UART0_BASE, &rx_buffer);
                        memcpy(calibration_data, rx_buffer, 4);

                        // display data to user
                        UARTSendString(UART0_BASE, (uint8_t *)"Sending data to Antenna MCU\n", 28);
                        UARTSendString(UART0_BASE, (uint8_t *)"Serial Number: ", 15);
                        UARTSendString(UART0_BASE, (uint8_t *) serial_number, 4);
                        UARTSendString(UART0_BASE, (uint8_t *)" Cal data: ", 11);
                        UARTSendString(UART0_BASE, (uint8_t *) calibration_data, 4);
                        UARTSendString(UART0_BASE, (uint8_t *)"\n", 1);

                        // end of programming mode testing
                        next_state = PC_initial;
                        // not for testing
                        // next_state = Tx_mode;
                        OPERATING_MODE = UNKNOWN;

                    }else if(OPERATING_MODE == READOUT_MODE){
                        UARTSendString(UART0_BASE, (uint8_t *)"Readout Mode: \n", 15);
                    }else{
                        UARTSendString(UART0_BASE, (uint8_t *)"EXCEPTION: INVALID MODE\n", 24);
                        next_state = PC_initial;
                    }
                }
                break;

            case Tx_mode:

                break;
            case Rx_mode:

                break;

            case PC_report:

                break;

            default:
                UARTSendString(UART0_BASE, (uint8_t *)"Invalid State\n", 14);
                next_state = PC_initial;
                break;
        }

        // clear buffer for next input
        memset(rx_buffer, 0, sizeof(*rx_buffer));
        state = next_state;
    }*/
}

//enables red on-board LED
void init_LED(){
    //
    // Enable the GPIO port that is used for the on-board LED.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    // set to high for testing initialization
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1);
    //GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
}



// enables UART0 using Rx - PA0 and Tx - PA1
void init_UART0(){
    // Enable peripherals
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // Set GPIO A0 and A1 as UART pins.
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART for 115,200, 8-N-1 operation.
    ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    // Enable the UART interrupt.
    ROM_IntEnable(INT_UART0);
    ROM_UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);

}

// enables UART1 using Rx - PB0 and Tx - PB1
void init_UART1(){
    // Enable peripherals
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    // Set GPIO B0 and B1 as UART pins.
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);
    ROM_GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART for 115,200, 8-N-1 operation.
    ROM_UARTConfigSetExpClk(UART1_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    // Enable the UART interrupt.
    ROM_IntEnable(INT_UART1);
    ROM_UARTIntEnable(UART1_BASE, UART_INT_RX | UART_INT_RT);
}

// enables PA2 and PA3 as digital outputs to modulator
void init_IO_pins(){
    // Enable pin for digital output to modulator (PA2, PA3?)
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3);

    // initially set to high for testing
    GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_2 | GPIO_PIN_3);
}

// enables the 5V output required for the modulator
// allows PA7 to be connected to external resistor and power supply for 5V output
void init_5V_output(){
    GPIOPinTypeGPIOOutputOD(GPIO_PORTA_BASE, GPIO_PIN_7);
    GPIOPinTypeGPIOOutputOD(GPIO_PORTA_BASE, GPIO_PIN_6);
}

// Function designed to test digital output to modulator
// PA2 and PA3 are for 3.3V output
// PA7 is used for 5 V outputs connected to external power supply
//uint8_t test_bitstream[16] = {0,1,0,1,0,0,1,1,0,1,0,0,0,1,1,1};
uint8_t test_bitstream[16] = {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1};
int count = 0;
void test_IO(){
    int i = 0;
    GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_PIN_7);
    GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, 0);
    while(1){
        if(test_bitstream[i] == 0){
            //GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3, 0);
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0);
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, GPIO_PIN_6);
            UARTSendString(UART0_BASE, (uint8_t *)"0", 1);
//            UARTSendString(UART1_BASE, (uint8_t *)"0", 1);
        }else{
            //GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_2 | GPIO_PIN_3);
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_PIN_7);
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, 0);
            UARTSendString(UART0_BASE, (uint8_t *)"1", 1);
//            UARTSendString(UART1_BASE, (uint8_t *)"1", 1);
        }
        i++;
        if(i == 16) i=0;

        SysCtlDelay(160000);
    }
}
