/*
 * main.cpp
 *
 *  Created on: Jan 27, 2023
 *      Author: travp
 */

#include <iostream>
#include <stdio.h>
#include "UART.h"
#include <string>

using namespace std;

#define SYSCTL_RCGCGPIO_R (*((volatile unsigned long *) 0x400FE608))
#define GPIO_PORTF_DEN_R (*((volatile unsigned long *) 0x4002551C))
#define GPIO_PORTF_DIR_R (*((volatile unsigned long *) 0x40025400))
#define GPIO_PORTF_DATA_R (*((volatile unsigned long *) 0x40025038))
#define GPIO_PORTF_CLK_EN 0x20
#define GPIO_PORTF_PIN1_EN 0x02
#define GPIO_PORTF_PIN2_EN 0x04
#define GPIO_PORTF_PIN3_EN 0x08
#define LED_ON1 0x02
#define LED_ON2 0x04
#define LED_ON3 0x08
#define DELAY_VALUE 4000000

const uint32_t UART0_BASE_ADDRESS = 0x16000000;
const uint32_t UART1_BASE_ADDRESS = 0x17000000;

void Delay(void);

enum state_machine {PCmode, enable_PC_mode, disable_PC_mode, receive_mode, transmit_mode};

int main(void) {

    SYSCTL_RCGCGPIO_R |= GPIO_PORTF_CLK_EN; //enable clock for PORTF
    GPIO_PORTF_DEN_R |= GPIO_PORTF_PIN1_EN; //enable pins 1 on PORTF
    GPIO_PORTF_DIR_R |= GPIO_PORTF_PIN1_EN; //make pins 1 as output pins
    GPIO_PORTF_DEN_R |= GPIO_PORTF_PIN2_EN; //enable pins 2 on PORTF
    GPIO_PORTF_DIR_R |= GPIO_PORTF_PIN2_EN; //make pins 2 as output pins
    GPIO_PORTF_DEN_R |= GPIO_PORTF_PIN3_EN; //enable pins 3 on PORTF
    GPIO_PORTF_DIR_R |= GPIO_PORTF_PIN3_EN; //make pins 3 as output pins

    //UART0_Init();

    state_machine state = PCmode;
    state_machine next_state;

    char temp;
    string str = "user input: ";

    UARTEnable(UART0_BASE_ADDRESS);
    temp = UARTCharGet(UART0_BASE_ADDRESS);
    UARTCharPut(UART0_BASE_ADDRESS, '+');
    UARTCharPut(UART0_BASE_ADDRESS, temp);

    while(1) {
        /*
        switch (state) {

            case PCmode:
                next_state = disable_PC_mode
            case enable_PC_mode:
                // disable UART1
                // enable UART0
                next_state = PCmode;
            case disable_PC_mode:
                // disable UART0
                // enable UART1
                next_state = receive_mode
            case receive_mode:
                // enable Rx pin
                // disable Tx pin
            case transmit_mode:
                // disable Rx pin
                // enable Tx pin
            default: // should never occur
                state = PCmode;
        }

        state = next_state

        */




        /*GPIO_PORTF_DATA_R = 0x02; //Turn on RED LED
        Delay(); //Delay almost 1 sec
        GPIO_PORTF_DATA_R = 0x00; //Turn off LED
        Delay(); //Delay almost 1 sec*/

        //temp = UART0_InChar();
        //str = "User inputted: ";
        //UART0_OutString(str);
        for(int i=0; i<str.length(); i++){
            //UART0_OutChar(str[i]);
        }
        //printf("test\n");
        //UART0_OutChar(temp);
        //UART0_OutChar('\n');

    }
}

void Delay(void) {

volatile unsigned long i;

for(i=0;i<DELAY_VALUE;i++);

}

