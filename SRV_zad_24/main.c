/**
 * @file    main.c
 * @author  Haris Turkmanovic(haris@etf.rs)
 * @date    2021
 * @brief   SRV ZaxValuek 8
 *
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Hardware includes. */
#include "msp430.h"

/* User's includes */
#include "../common/ETF5529_HAL/hal_ETF_5529.h"

/* User configuration*/
#define mainMAX_COUNTING_VALUE    40

/* Task priorities */
/** "Button task" priority */
#define mainBUTTON_TASK_PRIO        ( 3 )
/** "Counting task" Priority */
#define mainCOUNT_TASK_PRIO      ( 4 )
/** "LE Diode task" Priority */
#define mainLED_TASK_PRIO           ( 5)

#define mainDISP_TASK_PRIO           (2)
#define mainDIODE_TASK_PRIO  (1)


static void prvSetupHardware( void );

/*This semaphore whill be used to signal "Button press" event*/
xSemaphoreHandle    xEvent_ButtonPressed;
/*Used to signal "Button press" event*/
xSemaphoreHandle    xEvent_Button;
xSemaphoreHandle    xLightDiode;

xSemaphoreHandle    xGuard_xValue;
xSemaphoreHandle xGuard_Diode;
/**
 * @brief "Button Task" Function
 *
 * This task detect button press event and signal it to the other task
 * Thought binary semaphore
 */
volatile uint8_t     xValue = 10;
volatile uint8_t diodePeriod = 10;
static void prvButtonTaskFunc( void *pvParameters )
{
    uint16_t i;
        /*Initial button states are 1 because of pull-up configuration*/
        uint8_t     currentButtonState  = 1;
        for ( ;; )
        {
            xSemaphoreTake(xEvent_Button,portMAX_DELAY);
            /*wait for a little to check that button is still pressed*/
            for(i = 0; i < 1000; i++);
            /*Check is SW3 still pressed*/
            currentButtonState = ((P1IN & 0x10) >> 4);
            if(currentButtonState == 0){
                /* If button is still pressed write that info to global variable*/
                /* Global variable is shared resource. To access it, first try to
                 * take mutex */

                /* Write which button press event is detect */
                xSemaphoreGive(xEvent_ButtonPressed);
                /* Give mutex */

                /* Signal to "Diode task" to change state */

                continue;
            }
            /*Check is SW4 still pressed*/
            currentButtonState = ((P1IN & 0x20) >> 5);
            if(currentButtonState == 0){
                /* If button is still pressed write that info to global variable*/

                /* Write which button press event is detect */
                xSemaphoreGive(xLightDiode);
                /* Give mutex */


                continue;
            }
        }
}
/**
 * @brief "Button Task" Function
 *
 * This task detect button press event and signal it to the other task
 * Thought binary semaphore
 */

static void prvCountingTaskFunction( void *pvParameters )
{

    /*Init counter value to 0*/
    uint8_t     counter = 10;
    for ( ;; )
    {
        /*Wait on "Counting" event*/
        xSemaphoreTake(xEvent_ButtonPressed, portMAX_DELAY);
        if(counter == mainMAX_COUNTING_VALUE){
            /* if counting value reached, */

            counter = 2;

        }
        else{

            counter +=2;
        }
        xSemaphoreTake(xGuard_xValue, portMAX_DELAY);
        xValue = counter;
        xSemaphoreGive(xGuard_xValue);

    }
}
/**
 * @brief "LE Diode Task" function
 *
 * This task wait on "Counting" event and change LD3 diode state
 */
static void prvDiodeTaskFunc( void *pvParameters )
{
    for(;;){
    xSemaphoreTake(xLightDiode, portMAX_DELAY);

    xSemaphoreTake(xGuard_Diode, portMAX_DELAY);
    xSemaphoreTake(xGuard_xValue, portMAX_DELAY);

    diodePeriod = xValue;

   xSemaphoreGive(xGuard_xValue);
   xSemaphoreGive(xGuard_Diode);
    }

}
static void prvLightDiode( void *pvParameters )
{

    for ( ;; )
    {
        /*Wait on "Counting" event*/

        halTOGGLE_LED( LED3 );
        xSemaphoreTake(xGuard_Diode, portMAX_DELAY);
        vTaskDelay( pdMS_TO_TICKS(diodePeriod*100) );
        xSemaphoreGive(xGuard_Diode);


    }
}

static void prv7SegDispMux( void *pvParameters )
{

    volatile hal_7seg_display_t xCurrentActiveDisplay = HAL_DISPLAY_1;
    xSemaphoreTake(xGuard_xValue, portMAX_DELAY);
    uint8_t data = xValue;
    xSemaphoreGive(xGuard_xValue);
    uint8_t xSecondDigit     = data / 10;
    uint8_t xFirstDigit    = data - xSecondDigit*10;

    for ( ;; )
    {
        xSemaphoreTake(xGuard_xValue, portMAX_DELAY);
        data = xValue;
        xSemaphoreGive(xGuard_xValue);
        switch(xCurrentActiveDisplay){
            case HAL_DISPLAY_1:
                HAL_7SEG_DISPLAY_1_ON;
                HAL_7SEG_DISPLAY_2_OFF;
                /* Extract first digit from number*/
                xFirstDigit    = data - xSecondDigit*10;
                /* Show digit on previously enabled display*/
                vHAL7SEGWriteDigit(xFirstDigit);
                /* Change active display*/
                xCurrentActiveDisplay = HAL_DISPLAY_2;
                break;
            case HAL_DISPLAY_2:
                HAL_7SEG_DISPLAY_1_OFF;
                HAL_7SEG_DISPLAY_2_ON;
                /* Extract first digit from number*/
                xSecondDigit = data / 10;
                /* Show digit on previously enabled display*/
                vHAL7SEGWriteDigit(xSecondDigit);
                /* Change active display*/
                xCurrentActiveDisplay = HAL_DISPLAY_1;
                break;
        }
        vTaskDelay( pdMS_TO_TICKS(5) );
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
    xTaskCreate( prvButtonTaskFunc,
                 "Button Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainBUTTON_TASK_PRIO,
                 NULL
               );
    xTaskCreate( prvCountingTaskFunction,
                 "Counting Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainCOUNT_TASK_PRIO,
                 NULL
               );
    xTaskCreate( prvDiodeTaskFunc,
                 "LED Task",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainLED_TASK_PRIO,
                 NULL
               );
    xTaskCreate(  prv7SegDispMux,
                   "MUX display",
                   configMINIMAL_STACK_SIZE,
                   NULL,
                   mainDISP_TASK_PRIO,
                   NULL
                 );
    xTaskCreate(  prvLightDiode,
                      "LightDiode",
                      configMINIMAL_STACK_SIZE,
                      NULL,
                      mainDIODE_TASK_PRIO,
                      NULL
                    );


    xEvent_Button           =   xSemaphoreCreateBinary(); //isr
    /*Create FreeRTOS objects*/
    /*Create semaphores*/
    xEvent_ButtonPressed    =   xSemaphoreCreateBinary();//counting


    xLightDiode            =   xSemaphoreCreateBinary();// diode
    xGuard_xValue=   xSemaphoreCreateMutex();// mux disp
    xGuard_Diode = xSemaphoreCreateMutex();
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
        /*Enable interrupt for pins connected to SW3 and SW4*/
        P1IE  |= 0x30;
        P1IFG &=~0x30;
        /*Interrupt is generated during high to low transition*/
        P1IES |= 0x30;

        /* initialize LEDs */
        vHALInitLED();
        /*enable global interrupts*/
        taskENABLE_INTERRUPTS();
    /* initialize LEDs */
    vHALInitLED();
    /* init 7seg*/
    vHAL7SEGInit();
    /*left only one display*/
    HAL_7SEG_DISPLAY_2_OFF;
}

void __attribute__ ( ( interrupt( PORT1_VECTOR  ) ) ) vPORT1ISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* Give semaphore if button SW3 or button SW4 is pressed*/
    /* Note: This check is not truly necessary but it is good to
     * have it*/
    if(((P1IFG & 0x10) == 0x10) || ((P1IFG & 0x20) == 0x20)){
        xSemaphoreGiveFromISR(xEvent_Button, &xHigherPriorityTaskWoken);
    }
    /*Clear IFG register on exit. Read more about it in official MSP430F5529 documentation*/
    P1IFG &= ~ 0x30;
    /* trigger scheduler if higher priority task is woken */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
