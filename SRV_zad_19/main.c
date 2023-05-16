/**
 * @file    main.c
 * @author  Haris Turkmanovic(haris@etf.rs)
 * @date    2021
 * @brief   SRV Zadatak 18
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

/* Hardware includes. */
#include "msp430.h"

/* User's includes */
#include "../common/ETF5529_HAL/hal_ETF_5529.h"

/** "Display task" priority */
#define mainDISPLAY_TASK_PRIO           ( 1 )
/** "ADC task" priority */
#define mainADC_TASK_PRIO               ( 2 )
#define mainBUTTON_TASK_PRIO               ( 3 )

/* Display queue parameters value*/
/* Queue with length 1 is mailbox*/
#define mainDISPLAY_QUEUE_LENGTH            1

static void prvSetupHardware( void );

/* This queue will be used to send data to display task*/
xQueueHandle        xDisplayMailbox;
/* This semaphore will be used to signal "Button press" event*/
xSemaphoreHandle    xEvent_Button;
/**
 * @brief "Display Task" Function
 *
 * This task read data from xDisplayQueue but reading is not blocking.
 * xDisplayQueue is used to send data which will be printed on 7Seg
 * display. After data is received it is decomposed on high and low digit.
 */

static void prvTask2Function( void *pvParameters )
{
    uint16_t i;
    /*Initial button states are 1 because of pull-up configuration*/
    uint8_t     currentButtonState  = 1;
    /* New received 8bit data */
    uint8_t         NewValueToShow      = 0;
    /* High and Low number digit*/
    uint8_t         digitLow, digitHigh;
    for(;;){
        xSemaphoreTake(xEvent_Button,portMAX_DELAY);
        /*wait for a little to check that button is still pressed*/
        for(i = 0; i < 1000; i++);
        /*take button state*/
        currentButtonState = ((P1IN & 0x10) >> 4);
        if(currentButtonState == 0){
            /* If button is still pressed send signal to "Diode" task */
            /* Check if new number is received*/
            if(xQueuePeek(xDisplayMailbox, &NewValueToShow, 0) == pdTRUE){
                     /* If there is new number to show on display, split it on High and Low digit */
                     /* Extract high digit*/
                     digitHigh = NewValueToShow/10;
                     /* Extract low digit*/
                     digitLow = NewValueToShow - digitHigh*10;




                 UCA1TXBUF = digitHigh+ 48;
                 while(!(UCA1IFG&UCTXIFG));
                 UCA1TXBUF = digitLow+48;

            }
        }
    }

}

static void prvDisplayTaskFunction( void *pvParameters )
{
    /* New received 8bit data */
    uint8_t         NewValueToShow      = 0;
    /* High and Low number digit*/
    uint8_t         digitLow, digitHigh;
    for ( ;; )
    {
        /* Check if new number is received*/
        if(xQueuePeek(xDisplayMailbox, &NewValueToShow, 0) == pdTRUE){
            /* If there is new number to show on display, split it on High and Low digit */
            /* Extract high digit*/
            digitHigh = NewValueToShow/10;
            /* Extract low digit*/
            digitLow = NewValueToShow - digitHigh*10;
        }
        HAL_7SEG_DISPLAY_1_ON;
        HAL_7SEG_DISPLAY_2_OFF;
        vHAL7SEGWriteDigit(digitLow);
        vTaskDelay(5);
        HAL_7SEG_DISPLAY_2_ON;
        HAL_7SEG_DISPLAY_1_OFF;
        vHAL7SEGWriteDigit(digitHigh);
        vTaskDelay(5);
    }
}

/**
 * @brief "ADC Task" Function
 *
 * This task periodically, with 200 sys-tick period, trigger ADC
 */
static void prvADCTaskFunction( void *pvParameters )
{

    for ( ;; )
    {
       /*Trigger ADC Conversion*/
       ADC12CTL0 |= ADC12SC;
       vTaskDelay(200);
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
    xTaskCreate( prvDisplayTaskFunction,
                 "Display Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainDISPLAY_TASK_PRIO,
                 NULL
               );
    xTaskCreate( prvADCTaskFunction,
                 "ADC Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainADC_TASK_PRIO,
                 NULL
               );
    xTaskCreate(prvTask2Function,
                "Button task",
                configMINIMAL_STACK_SIZE,
                NULL,
                mainBUTTON_TASK_PRIO,
                NULL
                );
    /* Create FreeRTOS objects  */
    xDisplayMailbox       =   xQueueCreate(mainDISPLAY_QUEUE_LENGTH,sizeof(uint8_t));
    /* Create semaphores        */
    xEvent_Button           =   xSemaphoreCreateBinary();
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
    P1DIR &= ~0x10;
    /*Enable pull-up resistor*/
    P1REN |= 0x10;
    P1OUT |= 0x10;
    /*Enable interrupt for pin connected to S3*/
    P1IE  |= 0x10;
    P1IFG &=~0x10;
    /*Interrupt is generated during high to low transition*/
    P1IES |= 0x10;

    /*Initialize ADC */
    ADC12CTL0      = ADC12SHT02 + ADC12ON;       // Sampling time, ADC12 on
    ADC12CTL1      = ADC12SHP;                   // Use sampling timer
    ADC12IE        = 0x01;                       // Enable interrupt
    ADC12MCTL0     |= (ADC12INCH_0 );
    ADC12CTL0      |= ADC12ENC;
    P6SEL          |= 0x01;                      // P6.0 ADC option select

    /* Initialize UART */

         P4SEL       |= BIT4+BIT5;                    // P4.4,5 = USCI_AA TXD/RXD
         UCA1CTL1    |= UCSWRST;                      // **Put state machine in reset**
         UCA1CTL1    |= UCSSEL_2;                     // SMCLK
         UCA1BRW      = 1041;                         // 1MHz 115200
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
void __attribute__ ( ( interrupt( ADC12_VECTOR  ) ) ) vADC12ISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint16_t    temp;
    switch(__even_in_range(ADC12IV,34))
    {
        case  0: break;                           // Vector  0:  No interrupt
        case  2: break;                           // Vector  2:  ADC overflow
        case  4: break;                           // Vector  4:  ADC timing overflow
        case  6:                                  // Vector  6:  ADC12IFG0
            /* Scaling ADC value to fit on two digits representation*/
            temp    = ADC12MEM0>>6;
            xQueueOverwriteFromISR(xDisplayMailbox,&temp,&xHigherPriorityTaskWoken);
            break;
        case  8:                                  // Vector  8:  ADC12IFG1
            break;
        case 10: break;                           // Vector 10:  ADC12IFG2
        case 12: break;                           // Vector 12:  ADC12IFG3
        case 14: break;                           // Vector 14:  ADC12IFG4
        case 16: break;                           // Vector 16:  ADC12IFG5
        case 18: break;                           // Vector 18:  ADC12IFG6
        case 20: break;                           // Vector 20:  ADC12IFG7
        case 22: break;                           // Vector 22:  ADC12IFG8
        case 24: break;                           // Vector 24:  ADC12IFG9
        case 26: break;                           // Vector 26:  ADC12IFG10
        case 28: break;                           // Vector 28:  ADC12IFG11
        case 30: break;                           // Vector 30:  ADC12IFG12
        case 32: break;                           // Vector 32:  ADC12IFG13
        case 34: break;                           // Vector 34:  ADC12IFG14
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
    if((P1IFG & 0x10) == 0x10 ){
        xSemaphoreGiveFromISR(xEvent_Button, &xHigherPriorityTaskWoken);
    }
    /*Clear IFG register on exit. Read more about it in official MSP430F5529 documentation*/
    P1IFG &=~0x10;
    /* trigger scheduler if higher priority task is woken */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
