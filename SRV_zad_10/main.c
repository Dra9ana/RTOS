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


static void prvSetupHardware( void );
static void prvTask1Function( void *pvParameters );
static void prvTask2Function( void *pvParameters );
static void prvTask3Function( void *pvParameters );


/** Task 1 Priority */
#define mainTAKS_1_PRIO        ( 1 )
/** Task 2 Priority */
#define mainTAKS_2_PRIO        ( 2 )
#define mainTAKS_3_PRIO        ( 3 )

xSemaphoreHandle xEvent_Button;
xSemaphoreHandle xEvent_Counter;
xSemaphoreHandle xEvent_Button_Pressed;

static void prvTask1Function( void *pvParameters )
{
            uint16_t i;

            uint8_t     currentButtonState  = 1;
            for ( ;; )
            {
                /*Read button state*/
                xSemaphoreTake(xEvent_Button_Pressed, portMAX_DELAY);
                currentButtonState = ((P1IN & 0x10) >> 4);//BIT4

                    for(i = 0; i < 10000; i++);

                    if(currentButtonState == 0){
                        /* If button is still pressed toggle diode LD3 */
                        xSemaphoreGive( xEvent_Button);
                    }


            }

}

static void prvTask2Function( void *pvParameters )
{
    uint8_t cnt = 0;
    /* low priority task that toggles LED D4 */
    for ( ;; )
    {

        xSemaphoreTake( xEvent_Button, portMAX_DELAY );
        if(cnt == 9){
            cnt = 0;
            xSemaphoreGive(xEvent_Counter);
        }
        else{
            cnt++;
        }

        vHAL7SEGWriteDigit(cnt);
    }
}

static void prvTask3Function(void *pvParameters){
    for(;;){
    xSemaphoreTake( xEvent_Counter, portMAX_DELAY );
    halTOGGLE_LED( LED3 );
    }
}
void main( void )
{
    /* Configure peripherals */
    prvSetupHardware();

    /* Create tasks */
    if(xTaskCreate( prvTask1Function,
                 "Task 1",
                 configMINIMAL_STACK_SIZE,
                 NULL,//no arguments
                 mainTAKS_1_PRIO,
                 NULL
               ) != pdPASS) while(1);
    if(xTaskCreate( prvTask2Function,
                 "Task 2",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainTAKS_2_PRIO,
                 NULL
               ) != pdPASS) while(1);
    if(xTaskCreate( prvTask3Function,
                 "Task 3",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 mainTAKS_3_PRIO,
                 NULL
               ) != pdPASS) while(1);

    xEvent_Button = xSemaphoreCreateBinary();
    xEvent_Button_Pressed= xSemaphoreCreateBinary();
    xEvent_Counter =  xSemaphoreCreateBinary();
    /* Start the scheduler. */
    vTaskStartScheduler();

    /* If all is well then this line will never be reached.  If it is reached
    then it is likely that there was insufficient (FreeRTOS) heap memory space
    to create the idle task.  This may have been trapped by the malloc() failed
    hook function, if one is configured. */
    for( ;; );
}

static void prvSetupHardware( void )
{
    taskDISABLE_INTERRUPTS();

    /* Disable the watchdog. */
    WDTCTL = WDTPW + WDTHOLD;

    hal430SetSystemClock( configCPU_CLOCK_HZ, configLFXT_CLOCK_HZ );

    /* Init buttons */
    P1DIR &= ~0x30;
    P1REN |= 0x30;
    P1OUT |= 0x30;
    /*Enable interrupt for pin connected to S3*/
    P1IE  |= 0x10;
    P1IFG &=~0x10;
    /*Interrupt is generated during high to low transition*/
    P1IES |= 0x10;
    /* initialize LEDs */
    vHALInitLED();

    /* Init 7 seg display */
    vHAL7SEGInit();
    HAL_7SEG_DISPLAY_1_OFF;
    HAL_7SEG_DISPLAY_2_ON;
    taskENABLE_INTERRUPTS();
}
void __attribute__ ( ( interrupt( PORT1_VECTOR  ) ) ) vPORT1ISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Give semaphore if button SW3 is pressed*/
    /* Note: This check is not truly necessary but it is good to
     * have it*/
    if((P1IFG & 0x10) == 0x10){
        xSemaphoreGiveFromISR(xEvent_Button_Pressed, &xHigherPriorityTaskWoken);
    }
    /*Clear IFG register on exit. Read more about it in official MSP430F5529 documentation*/
    P1IFG &=~0x10;
    /* trigger scheduler if higher priority task is woken */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
