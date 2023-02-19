#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "driverlib/eeprom.h"

#define BAUD_RATE 115200
#define MAX_MSG_LEN 64

//
//Interrupt handler for UARTs
//
void UARTIntHandler(void)
{
    uint32_t ui32Status;

    // Get the interrupt status.
    ui32Status = UARTIntStatus(UART0_BASE, true);

    // Clear the asserted interrupts.
    UARTIntClear(UART0_BASE, ui32Status);

    while(UARTCharsAvail(UART0_BASE))
    {
        // Read the next character from the UART and write it to the other UART.
        UARTCharPut(UART1_BASE, UARTCharGet(UART0_BASE));
    }
}

//
// Uses char pointer pui8Buffer to put char string to ui32Base.
//
void UARTSendString(uint32_t ui32Base, const char *pui8Buffer)
{
    while (*pui8Buffer != '\0')
    {
        UARTCharPut(ui32Base, *pui8Buffer);
        pui8Buffer++;
    }
}

//
// Gets chars from ui32Base and stores them in rxBuffer.
//
void UARTReadString(uint32_t ui32Base, char * rxBuffer){
    // load user response into rx_buffer
    int i=0;
    do{
        rxBuffer[i] = (char) UARTCharGet(ui32Base);
        i++;
    }while(rxBuffer[i-1] != '\n'); // might need to consider '\r' as a message terminator as well
}

//
// Initializes UART0 and UART1.
//
void setUpUARTs(){
    // Set the system clock to run at 80MHz
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    // Enable UART0 and GPIO port A
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // Configure the UART0 pins
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART0 settings
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), BAUD_RATE,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Enable UART0
    UARTEnable(UART0_BASE);

    // Enable UART1 and GPIO port B
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    // Configure the UART1 pins
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART1 settings
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), BAUD_RATE,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Enable UART1
    UARTEnable(UART1_BASE);

    // Enable UART0 interrupt on receive.
    UARTIntEnable(UART0_BASE, UART_INT_RX);
    IntEnable(INT_UART0);

    // Enable UART1 interrupt on receive.
    UARTIntEnable(UART1_BASE, UART_INT_RX);
    IntEnable(INT_UART1);
}

//
// Initializes EEPROM
//
void setUpEEPROM(){
    //
    // Enable the EEPROM module.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);
    //
    // Wait for the EEPROM module to be ready.
    //
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_EEPROM0)){}
    //
    // Wait for the EEPROM Initialization to complete
    //
    uint32_t ui32EEPROMInit = EEPROMInit();
    //
    // Check if the EEPROM Initialization returned an error
    // and inform the application
    //
    if(ui32EEPROMInit != EEPROM_INIT_OK){
        const char* prompt = "Error Initializing EEPROM.\n";
        UARTSendString(UART0_BASE, prompt);
        exit(0);
    }
}

//
// Any input received through UART1 is stored on the EEPROM and read back. It is then sent to UART0 to be
// displayed.
//
int main(void) {
    setUpUARTs();
    setUpEEPROM();

    while (1)
    {
        char msg[MAX_MSG_LEN + 1];  //message buffer

        // Wait for a message on UART1. Store it in the EEPROM
        // then read it back. Send stored data to UART0.
        while(!UARTCharsAvail(UART1_BASE)){}
        UARTReadString(UART1_BASE, msg);
        
        EEPROMProgram(msg, 0x500, sizeof(msg));
        EEPROMRead(msg, 0x500, sizeof(msg));
        
        UARTSendString(UART0_BASE, "Received message: ");
        UARTSendString(UART0_BASE, msg);
        UARTSendString(UART0_BASE, "\n");
    }
}
