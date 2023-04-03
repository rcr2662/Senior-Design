#include <stdint.h>
#include <stdbool.h>
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
#include "driverlib/comp.h"
#include "driverlib/timer.h"

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

#define PACKET_LENGTH 19
#define USER_INPUT_BUFFER_LENGTH 32

enum states
{
    PC_initial,
    PC_report,
    Tx_mode,
    Rx_mode
};

// global state variables
// needed to be global to be used by readout timer handler function
enum states state = PC_initial;
enum states next_state;

/////////////////////////////////////////////
//          Timer handler functions
/////////////////////////////////////////////

// function samples the output of the comparator and relays the signal to a digital pin (PF1)
void comparator_timer_handler(void){
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    if(ComparatorValueGet(COMP_BASE,0) == true){
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
    }else{
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0);
    }
}

// function is called when readout timer expires without receiving a message
void readout_timer_handler() {
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

    UARTSendString(UART0_BASE, (uint8_t *) "No message received.\n", 21);

    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);
    next_state = PC_initial;
}

/////////////////////////////////////////////
//          initialization functions
/////////////////////////////////////////////

// contains initialization code from the starter project
void init_clock(void){
    ROM_FPUEnable();
    ROM_FPULazyStackingEnable();

    ROM_SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
}

// initializes on-board LED GPIO outputs
// PF1 = comparator digital output
// PF3 = power enable output
void init_leds(void){
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_3);
}

// initializes UART0 and UART1
// UART0 Rx = PA0, Tx = PA1
// UART1 Rx = PB0, Tx = PB1
void init_uart(void){
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);
    ROM_GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure both UARTs for 115,200, 8-N-1 operation.
    ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    ROM_UARTConfigSetExpClk(UART1_BASE, ROM_SysCtlClockGet(), 9600,
                                (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                                 UART_CONFIG_PAR_NONE));
}

void init_comparator(){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_COMP0);

    GPIOPinTypeComparator(GPIO_PORTC_BASE, GPIO_PIN_7);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_COMP0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_COMP0)){}

    ComparatorRefSet(COMP_BASE, COMP_REF_1_340625V);
    ComparatorConfigure(COMP_BASE, 0,(COMP_TRIG_NONE | COMP_ASRCP_REF | COMP_OUTPUT_INVERT));

    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);

    SysCtlDelay(100000);
}

// initialize TIMER0 and TIMER1
// TIMER0 is for sampling the comparator and TIMER1 is the readout timer
void init_timer(){
    // Enable the Timer0 peripheral
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0)){}

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, 416);
    TimerIntRegister(TIMER0_BASE, TIMER_A, comparator_timer_handler);

    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);

    // Enable the Timer1 peripheral
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1)){}

    TimerConfigure(TIMER1_BASE, TIMER_CFG_ONE_SHOT);
    TimerLoadSet(TIMER1_BASE, TIMER_A, 16000000);
    TimerIntRegister(TIMER1_BASE, TIMER_A, readout_timer_handler);

    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    //TimerEnable(TIMER1_BASE, TIMER_A); // timer is enabled when MCU enters Readout Mode
}



/////////////////////////////////////////////
//          UART functions
/////////////////////////////////////////////

void UARTIntHandler(void){ // code from starter project mostly, don't think this function gets used

    uint32_t ui32Status;
    ui32Status = ROM_UARTIntStatus(UART0_BASE, true);
    ROM_UARTIntClear(UART0_BASE, ui32Status);

    while(ROM_UARTCharsAvail(UART0_BASE))
    {
        ROM_UARTCharPutNonBlocking(UART0_BASE, ROM_UARTCharGetNonBlocking(UART0_BASE));
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
        SysCtlDelay(SysCtlClockGet() / (1000 * 3));
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);
    }
}

// Sends string to the desired UART
void UARTSendString(uint32_t ui32Base, const uint8_t *pui8Buffer, uint32_t ui32Count){
    int i;
    for(i=0; i<ui32Count; i++){
        UARTCharPut(ui32Base, pui8Buffer[i]);
    }
}

// Read string from the desired UART
void UARTReadString(uint32_t ui32Base, uint8_t * rxBuffer){
    // load user response into rx_buffer
    int i=0;
    do{
        rxBuffer[i] = (uint8_t) UARTCharGet(ui32Base);
        i++;
    }while((char) rxBuffer[i-1] != '\n'); // might need to consider '\r' as a message terminator as well
}

/////////////////////////////////////////////
//          Miscellaneous functions
/////////////////////////////////////////////

// this function checks if the user input is valid
// the function checks if the user input is the correct length,
// then checks if all characters are uppercase hex characters
bool checkValidInput(uint8_t* buffer, uint8_t length){
    int i;
    for(i=0; i<USER_INPUT_BUFFER_LENGTH; i++){
        UARTSendString(UART0_BASE, (uint8_t *)buffer[i], 1);
        // check if user entered non-hex characters
        // 0x30 - 0x39 == '0' - '9' && 0x41 - 0x46 == 'A' - 'F'
        if(i<=length){ // the +1 is for the new line character
            if((buffer[i] >= 0x30 && buffer[i] <= 0x39) || (buffer[i] >= 0x41 && buffer[i] <= 0x46) || (buffer[i] == 0x0A)){
                // valid
                UARTSendString(UART0_BASE, (uint8_t *)"valid", 5);
            }else{
                UARTSendString(UART0_BASE, (uint8_t *)"invalid1", 8);
                return false;
            }
        // check if user entered too many characters
        }else{
            if(buffer[i] != 0){
                UARTSendString(UART0_BASE, (uint8_t *)"invalid2", 8);
                return false;
            }
        }
        UARTSendString(UART0_BASE, (uint8_t *)"\n", 1);
    }
    return true;
}

uint8_t rx_buffer[256] = {0};
uint8_t tx_buffer[40] = {0};
uint8_t user_input_buffer[USER_INPUT_BUFFER_LENGTH] = {0};
int rx_index = 0;

// this variable is used to adjust for the UART incorrectly reading zeros at the start of a message
// the data becomes valid after a \n char is sent, indicating the start of a message
bool valid_data = false;

int
main(void)
{
    init_clock(); // 16 MHz
    init_leds(); // PF1 & PF3
    init_uart(); // UART0 & UART1
    init_comparator(); // COMP0
    init_timer(); // TIMER0 & TIMER1

    while(1){

        switch(state){
          case PC_initial:

              // turn off power enable
              GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);

              // clear buffers
              memset(rx_buffer, 0, sizeof(*rx_buffer));
              memset(tx_buffer, 0, sizeof(*tx_buffer));

              // prompt user input
              UARTSendString(UART0_BASE, (uint8_t *)"Program or Readout? (P/R): ", 27);

              // load user response into rx_buffer
              UARTReadString(UART0_BASE, &user_input_buffer);

              // interpret user response and enter desired mode of operation
              if(user_input_buffer[0] == 'P'){
                   UARTSendString(UART0_BASE, (uint8_t *)"Entering Programming Mode: \n", 28);
                   memset(tx_buffer, 0, sizeof(*tx_buffer));

                   // prompt user for G1

//                    read G1
//                   do{
                       memset(user_input_buffer, 0, sizeof(*user_input_buffer));
                       UARTSendString(UART0_BASE, (uint8_t *)"\nEnter Group Delay G1: ", 23);
                       UARTReadString(UART0_BASE, &user_input_buffer);
//                   while(checkValidInput(user_input_buffer, 4) == false);
//                   if(checkValidInput(user_input_buffer, 4)){
//                       UARTSendString(UART0_BASE, (uint8_t *)"valid\n", 6);
//                   }else{
//                       UARTSendString(UART0_BASE, (uint8_t *)"invalid\n", 8);
//                   }
                   memcpy(&tx_buffer[0], &user_input_buffer[0], 4*sizeof(uint8_t));
                   memset(user_input_buffer, 0, sizeof(*user_input_buffer));

                   // prompt user for G2
                   UARTSendString(UART0_BASE, (uint8_t *)"\nEnter Group Delay G2: ", 23);
                   // read G2
                   UARTReadString(UART0_BASE, &user_input_buffer);
                   memcpy(&tx_buffer[4], &user_input_buffer[0], 4*sizeof(uint8_t));
                   memset(user_input_buffer, 0, sizeof(*user_input_buffer));

                   // prompt user for E5
                   UARTSendString(UART0_BASE, (uint8_t *)"\nEnter Group Delay E5: ", 23);
                   // read E5
                   UARTReadString(UART0_BASE, &user_input_buffer);
                   memcpy(&tx_buffer[8], &user_input_buffer[0], 4*sizeof(uint8_t));
                   memset(user_input_buffer, 0, sizeof(*user_input_buffer));

                   // prompt user for serial number
                   UARTSendString(UART0_BASE, (uint8_t *)"\nEnter Serial Number: ", 22);
                   // read serial number
                   UARTReadString(UART0_BASE, &user_input_buffer);
                   memcpy(&tx_buffer[12], &user_input_buffer[0], 6*sizeof(uint8_t));
                   memset(user_input_buffer, 0, sizeof(*user_input_buffer));

                   // prompt user for manufacturer code
                   UARTSendString(UART0_BASE, (uint8_t *)"\nEnter Manufacturer Code: ", 26);
                   // read manufacturer code
                   UARTReadString(UART0_BASE, &user_input_buffer);
                   memcpy(&tx_buffer[18], &user_input_buffer[0], 4*sizeof(uint8_t));
                   memset(user_input_buffer, 0, sizeof(*user_input_buffer));

                   // prompt user for misc data
                   UARTSendString(UART0_BASE, (uint8_t *)"\nEnter Miscellaneous Data: ", 26);
                   // read misc data
                   UARTReadString(UART0_BASE, &user_input_buffer);
                   memcpy(&tx_buffer[22], &user_input_buffer[0], 16*sizeof(uint8_t));
                   memset(user_input_buffer, 0, sizeof(*user_input_buffer));


                   // convert ascii to hex
                   int i;
                   for(i=0; i<PACKET_LENGTH*2; i++){
                       if(tx_buffer[i] >= 0x30 && tx_buffer[i] <= 0x39){
                           tx_buffer[i] -= 0x30;
                       }else if(tx_buffer[i] >= 0x41 && tx_buffer[i] <= 0x46){
                           tx_buffer[i] -= 0x37;
                       }else{
                           tx_buffer[i] = 0;
                       }
                   }

                   // compress the hex data into smaller packet size
                   // 2 ASCII characters are stored in one byte
                   for(i=0; i<PACKET_LENGTH; i++){
                       tx_buffer[i] = (tx_buffer[2*i] << 4) | tx_buffer[2*i+1];
                       if(i>1){
                           tx_buffer[2*i] = 0;
                           tx_buffer[2*i+1] = 0;
                       }
                   }

                   next_state = Tx_mode;

              }else if (user_input_buffer[0] == 'R'){

                   UARTSendString(UART0_BASE, (uint8_t *)"Entering Readout Mode: \n", 24);



                   next_state = Rx_mode;

              }else{
                   UARTSendString(UART0_BASE, (uint8_t *)"Invalid Input\n", 14);
                   next_state = PC_initial;
              }

              // turn on power enable and start timer

              memset(user_input_buffer, 0, sizeof(*user_input_buffer));

              // used for testing user inputs and data output
              //memcpy(rx_buffer, tx_buffer, 19*sizeof(uint8_t));
              //next_state = PC_report;

          break;
          case Tx_mode:
          {
              // turn on power supply and allow Antenna MCU to boot
              GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
              SysCtlDelay(1000000); // should be roughly 0.1 sec delay


              // output return character that indicates start of message
              UARTCharPut(UART1_BASE, '\r');

              // output the user data to the antenna
              int i;
              for(i=0; i<19; i++){
                  //UARTSendString(UART0_BASE, (uint8_t *)tx_buffer, sizeof(*tx_buffer));
                  UARTCharPut(UART1_BASE, tx_buffer[i]);

              }

              while(UARTCharsAvail(UART1_BASE)){
                  UARTCharGet(UART1_BASE);
              }

              UARTSendString(UART0_BASE, (uint8_t *)"Programming Message Sent\n", 25);

              // wait 0.5 seconds and turn off power enable
              SysCtlDelay(5000000); // should be roughly 0.5 sec
              GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);

              next_state = PC_initial;

          break;
          }
          case Rx_mode:

              // clear buffers
              memset(rx_buffer, 0, sizeof(*rx_buffer));

              // enable power supply for Antenna MCU and start timer
              GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
              //TimerEnable(TIMER1_BASE, TIMER_A);

              // receive code written by roberto
              //uint8_t rx_buffer[38];
              //uint8_t rx_index = 0;
              rx_index = 0;

              while(UARTCharGet(UART1_BASE) != '\r'){}

              while(rx_index < 19){

                  // Get char from UART1
                  uint8_t c = UARTCharGet(UART1_BASE);

                  // If char is not terminating character add it to buffer

                  if(c != '\n' && c != '\r'){
                      rx_buffer[rx_index] = c;
                      rx_index++;
                  }
              }
              next_state = PC_report;


              // receive code written by travis
//              memset(rx_buffer, 0, sizeof(*rx_buffer));
//
//              // wait for data to be sent by antenna MCU
//              // this should be a 0.5 second delay
//              //while(UARTCharsAvail(UART1_BASE) == false) {}
//              if(UARTCharsAvail(UART1_BASE)){
//
//              uint8_t c = UARTCharGet(UART1_BASE);
//              if(c == '\r'){
//                  valid_data = true;
//              }else if(c != '\n' && rx_index < PACKET_LENGTH){
//                  if(valid_data){
//                      rx_buffer[rx_index] = c;
//                      rx_index++;
//                      next_state = Rx_mode;
//                  }
//              }else{
//                  //message_received_flag = true;
//                  rx_buffer[rx_index] = '\0';
//                  rx_index = 0;
//                  GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
//
//                      // used for testing to output data message to PC
////                      int i = 0;
////                      for(i=0; i<30; i++){
////                          UARTCharPut(UART0_BASE, rx_buffer[i]);
////                      }
//                  valid_data = false;
//                  next_state = PC_report;
//              }
//
//              }
          break;
          case PC_report:
          {
              // turn off power enable and readout timer
              GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);
              TimerDisable(TIMER1_BASE, TIMER_A);

              // decompress rx_buffer into 1 byte per hex character
              int i;
              for(i=2*PACKET_LENGTH-1; i>1; i-=2){
                  rx_buffer[i-1] = rx_buffer[i/2] >> 4;
                  rx_buffer[i] = rx_buffer[i/2] & 0x0F;
              }
              rx_buffer[1] = rx_buffer[0] & 0x0F;
              rx_buffer[0] = rx_buffer[0] >> 4;

              for(i=0; i<PACKET_LENGTH*2; i++){
                  if(rx_buffer[i] >= 0x00 && rx_buffer[i] <= 0x09){
                      rx_buffer[i] += 0x30;
                  }else if(rx_buffer[i] >= 0x0A && rx_buffer[i] <= 0x0F){
                      rx_buffer[i] += 0x37;
                  }else
                      rx_buffer[i] = '\0';
              }

              UARTSendString(UART0_BASE, (uint8_t *)"Data Readout:\n", 14);
              UARTSendString(UART0_BASE, (uint8_t *)"G1: ", 4);
              UARTSendString(UART0_BASE, (uint8_t *)&rx_buffer[0], 4);
              UARTSendString(UART0_BASE, (uint8_t *)"\nG2: ", 5);
              UARTSendString(UART0_BASE, (uint8_t *)&rx_buffer[4], 4);
              UARTSendString(UART0_BASE, (uint8_t *)"\nE5: ", 5);
              UARTSendString(UART0_BASE, (uint8_t *)&rx_buffer[8], 4);
              UARTSendString(UART0_BASE, (uint8_t *)"\nSerial Number: ", 16);
              UARTSendString(UART0_BASE, (uint8_t *)&rx_buffer[12], 6);
              UARTSendString(UART0_BASE, (uint8_t *)"\nManufacturer Code: ", 20);
              UARTSendString(UART0_BASE, (uint8_t *)&rx_buffer[18], 4);
              UARTSendString(UART0_BASE, (uint8_t *)"\nMiscellaneous Data: ", 21);
              UARTSendString(UART0_BASE, (uint8_t *)&rx_buffer[22], 16);
              UARTSendString(UART0_BASE, (uint8_t *)"\nData Readout Complete\n", 23);


              next_state = PC_initial;

          break;
          }
          default:
            next_state = PC_initial;
          break;
        }
        state = next_state;
    }

}
