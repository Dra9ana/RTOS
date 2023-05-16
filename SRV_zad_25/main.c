/**
 * @file    main.c
 * @author  Haris Turkmanovic(haris@etf.rs)
 * @date    2021
 * @brief   SRV Zadatak 17
 *
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

/* Hardware includes. */
#include "msp430.h"

/* User's includes */
#include "../common/ETF5529_HAL/hal_ETF_5529.h"

#define ULONG_MAX                       0xFFFFFFFF

typedef enum{
    DIODE_3_ON,
    DIODE_3_OFF,
    DIODE_4_ON,
    DIODE_4_OFF,
    DIODE_COMMAND_UNDEF
}diode_command_t;


/* Task priorities */
/** "Button processing" priority */
#define mainBUTTON_PROCESSING_TASK_PRIO     ( 3 )
/** "Char processing" priority */
#define mainCHAR_PROCESSING_TASK_PRIO       ( 2 )
/** "LE Diode task" Priority */
#define mainDIODE_CONTROL_TASK_PRIO         ( 4)
#define mainDISPLAY_TASK_PRIO  (1)

#define mainDISP_1            0x01    /* Start AD conversion bit mask */
#define mainDISP_2          0x02

/* Char queue parameters value*/
#define mainCHAR_QUEUE_LENGTH               5
/* Diode command queue parameters value*/
#define mainDIODE_COMMAND_QUEUE_LENGTH      5

TaskHandle_t        xButtonTaskHandle;
TaskHandle_t   xDispTaskHandle;
TaskHandle_t   xTimerTaskHandle;
/* Software Timer handler*/
TimerHandle_t       xDispTimer;
static void prvSetupHardware( void );

xSemaphoreHandle    xEvent_Timer;
/* This queue will be used to buffer char received over UART interface*/
xQueueHandle        xCharQueue;
/* This queue will be used to buffer diode control messages*/
xQueueHandle        xCommandQueue;
/**
 * @brief "Char Processing" Function
 *
 * This task waits for character over UART received from xCharQueue. After character
 * is received it is processed and based on character value appropriate Diode Control
 * message is written to global variable
 */
static void prvCharProcessingTaskFunction( void *pvParameters )
{
    char        recChar =   0;
    diode_command_t commandToSend = DIODE_COMMAND_UNDEF;
    uint8_t i = 0;
    uint8_t num = 0;
    for ( ;; )
    {
        /*Read char from the queue*/
        xQueueReceive(xCharQueue, &recChar, portMAX_DELAY);


        /*Determine which character is received and based on character value
         * determine which command will be sent to "Diode Control" task*/
        switch(recChar){
            case 'e':
                commandToSend   = DIODE_4_ON;
                i = 0;
                num = 0;
                break;
            case 'd':
                commandToSend   = DIODE_4_OFF;
                i = 0;
                num = 0;
                break;
            case 's':
                i = 1;
                num = 0;
                commandToSend   = DIODE_COMMAND_UNDEF;
                break;
            case '0':case'1': case'2': case'3': case'4':case'5':case'6': case'7': case'8': case'9':
                commandToSend   = DIODE_COMMAND_UNDEF;
                switch(i){
                case 1:
                    num += (recChar - '0')*10;
                    i++;
                    break;
                case 2:
                    num += (recChar - '0');
                    i++;
                    break;
                default:
                    i = 0;
                    break;
                }

                break;
            case 't':
                if( i == 3){
                    xTaskNotify(xDispTaskHandle, num, eSetValueWithOverwrite);
                }
                num = 0;
                i = 0;
                commandToSend   = DIODE_COMMAND_UNDEF;
                break;


           default:
                commandToSend   = DIODE_COMMAND_UNDEF;
                i = 0;
                num = 0;
                break;
        }
        if(commandToSend != DIODE_COMMAND_UNDEF)
            xQueueSendToBack(xCommandQueue,&commandToSend,portMAX_DELAY);
    }
}
/**
 * @brief "Diode Control" task function
 *
 * This task set diode state based on prvDIODE_COMMAND variable
 */
static void prvDiodeControlTaskFunction( void *pvParameters )
{
    diode_command_t commandToProcess = DIODE_COMMAND_UNDEF;
    for ( ;; )
    {
        /* Wait on command*/
        xQueueReceive(xCommandQueue, &commandToProcess, portMAX_DELAY);
        /* Process command*/
        switch(commandToProcess){
            case DIODE_3_OFF:
                halCLR_LED(LED3);
                break;
            case DIODE_3_ON:
                halSET_LED(LED3);
                break;
            case DIODE_4_OFF:
                halCLR_LED(LED4);
                break;
            case DIODE_4_ON:
                halSET_LED(LED4);
                break;

            case DIODE_COMMAND_UNDEF:
                break;
        }
    }
}
/**
 * @brief "Button Task" Function
 *
 * This task waits for xEvent_Button semaphore to be given from
 * port ISR. After that, simple debauncing is performed in order
 * to verify that button is still pressed
 */
static void prvButtonTaskFunction( void *pvParameters )
{
    uint16_t i;
    /*Initial button states are 1 because of pull-up configuration*/
    uint8_t         currentButtonState  = 1;
    diode_command_t commandToSend       = DIODE_COMMAND_UNDEF;
    for ( ;; )
    {
        /* Wait for notification from ISR*/
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /*wait for a little to check that button is still pressed*/
        for(i = 0; i < 1000; i++);
        /*take button state*/
        /* check if button SW3 is pressed*/
        currentButtonState = ((P1IN & 0x10) >> 4);
        if(currentButtonState == 0){
            /* If SW3 is pressed send command to disable diode */
            commandToSend   =   DIODE_3_ON;
            xQueueSendToBack(xCommandQueue, &commandToSend, portMAX_DELAY);
            continue;
        }
        /* check if button SW4 is pressed*/
        currentButtonState = ((P1IN & 0x20) >> 5);
        if(currentButtonState == 0){
            /* If SW4 is pressed send command to enable diode */
            commandToSend   =   DIODE_3_OFF;
            xQueueSendToBack(xCommandQueue, &commandToSend, portMAX_DELAY);
            continue;
        }
    }
}

void    prvDispTimerCallback(TimerHandle_t xTimer){

    xSemaphoreGive(xEvent_Timer);


}
static void prvDisplayTaskFunction( void *pvParameters )
{
    /* New received 8bit data */
    uint32_t         NewValueToShow      = 0;
    uint32_t notifyValue = 0;
    /* High and Low number digit*/
    uint8_t         digitLow, digitHigh;
    uint32_t current_disp = mainDISP_1;
    for ( ;; )
    {
        /* Check if new number is received*/
        /* Waits for notification value */
        if(xTaskNotifyWait(ULONG_MAX, ULONG_MAX, &NewValueToShow, 0)== pdTRUE){
        /* If there is new number to show on display, split it on High and Low digit */
              /* Extract high digit*/
              digitHigh = NewValueToShow/10;
              /* Extract low digit*/
              digitLow = NewValueToShow - digitHigh*10;
        }


        xSemaphoreTake(xEvent_Timer,portMAX_DELAY);

        if(current_disp == mainDISP_1){
        HAL_7SEG_DISPLAY_1_ON;
        HAL_7SEG_DISPLAY_2_OFF;
        vHAL7SEGWriteDigit(digitLow);
        }
        if(current_disp ==  mainDISP_2){
          HAL_7SEG_DISPLAY_2_ON;
          HAL_7SEG_DISPLAY_1_OFF;
          vHAL7SEGWriteDigit(digitHigh);
        }
        current_disp = (current_disp == mainDISP_1)? mainDISP_2:mainDISP_1;
    }
}

/**
 * @brief main function
 */
void main( void )
{
    /* Configure peripherals */
    prvSetupHardware();

    /* Create tasks */
    xTaskCreate( prvCharProcessingTaskFunction,
                 "Char Processing Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainCHAR_PROCESSING_TASK_PRIO,
                 NULL
               );
    xTaskCreate( prvDiodeControlTaskFunction,
                 "Diode Control Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainDIODE_CONTROL_TASK_PRIO,
                 NULL
               );
    xTaskCreate( prvButtonTaskFunction,
                 "Button Processing Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainBUTTON_PROCESSING_TASK_PRIO,
                 &xButtonTaskHandle
               );
    xTaskCreate( prvDisplayTaskFunction,
                 "Display Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainDISPLAY_TASK_PRIO,
                 &xDispTaskHandle
               );

    /* Create FreeRTOS objects  */
    /* Create semaphores        */
    /* Create Queue*/
    xCharQueue              =   xQueueCreate(mainCHAR_QUEUE_LENGTH,sizeof(char));
    xCommandQueue           =   xQueueCreate(mainDIODE_COMMAND_QUEUE_LENGTH,sizeof(diode_command_t));
    /* Create timer */
    xDispTimer         = xTimerCreate("Display timer",
                                           pdMS_TO_TICKS(5),
                                           pdTRUE,
                                           &xTimerTaskHandle,
                                           prvDispTimerCallback);
    xEvent_Timer          =   xSemaphoreCreateBinary();
    /* Start timer with initial period */
    xTimerStart(xDispTimer,portMAX_DELAY);
    /* Start the scheduler. */
    vTaskStartScheduler();

    /* If all is well then this line will never be reached.  If it is reached
    then it is likely that there was insufficient (FreeRTOS) heap memory space
    to create the idle task.  This may have been trapped by the malloc() failed
    hook function, if one is configured. */
    for( ;; );
}

/**
 * @brief Configure hardware upon boot
 */
static void prvSetupHardware( void )
{
    taskDISABLE_INTERRUPTS();

    /* Disable the watchdog. */
    WDTCTL = WDTPW + WDTHOLD;

    hal430SetSystemClock( configCPU_CLOCK_HZ, configLFXT_CLOCK_HZ );

    /* - Init buttons - */
    /*Set direction to input*/
    P1DIR &= ~0x30;
    /*Enable pull-up resistor*/
    P1REN |= 0x30;
    P1OUT |= 0x30;
    /*Enable interrupt for pin connected to SW3*/
    P1IE  |= 0x30;
    P1IFG &=~0x30;
    /*Interrupt is generated during high to low transition*/
    P1IES |= 0x30;

    /* Initialize UART */

     P4SEL       |= BIT4+BIT5;                    // P4.4,5 = USCI_AA TXD/RXD
     UCA1CTL1    |= UCSWRST;                      // **Put state machine in reset**
     UCA1CTL1    |= UCSSEL_2;                     // SMCLK
     UCA1BRW      = 1041;                         // 1MHz - Baudrate 9600
     UCA1MCTL    |= UCBRS_6 + UCBRF_0;            // Modulation UCBRSx=1, UCBRFx=0
     UCA1CTL1    &= ~UCSWRST;                     // **Initialize USCI state machine**
     UCA1IE      |= UCRXIE;                       // Enable USCI_A1 RX interrupt

    /* initialize LEDs */
    vHALInitLED();

    /* initialize display*/
        vHAL7SEGInit();
        /*enable global interrupts*/
    taskENABLE_INTERRUPTS();
}
// Echo back RXed character, confirm TX buffer is ready first
void __attribute__ ( ( interrupt( USCI_A1_VECTOR  ) ) ) vUARTISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch(UCA1IV)
    {
        case 0:break;                             // Vector 0 - no interrupt
        case 2:                                   // Vector 2 - RXIFG
            xQueueSendToBackFromISR(xCharQueue, &UCA1RXBUF, &xHigherPriorityTaskWoken);
            UCA1TXBUF = UCA1RXBUF;
        break;
        case 4:break;                             // Vector 4 - TXIFG
        default: break;
    }
    /* trigger scheduler if higher priority task is woken */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void __attribute__ ( ( interrupt( PORT1_VECTOR  ) ) ) vPORT1ISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* Give semaphore if button SW3 is pressed*/
    /* Note: This check is not truly necessary but it is good to
     * have it*/
    if(((P1IFG & 0x10) == 0x10) || ((P1IFG & 0x20) == 0x20)){
        vTaskNotifyGiveFromISR( xButtonTaskHandle, &xHigherPriorityTaskWoken);
    }
    /*Clear IFG register on exit. Read more about it in offical MSP430F5529 documentation*/
    P1IFG &=~0x30;
    /* trigger scheduler if higher priority task is woken */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
