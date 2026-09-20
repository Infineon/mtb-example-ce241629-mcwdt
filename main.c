/******************************************************************************
* File Name:   main.c

* Description: This is the source code for the PSOC™ Control C3M/P8 Multi-Counter
*              Watchdog Timer (MCWDT) Example.
*
* Related Document: See README.md
*
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "mtb_hal.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
********************************************************************************/

/* Switch press/release check interval in milliseconds for debouncing */
#define SWITCH_DEBOUNCE_CHECK_UNIT          (1u)

/* Number of debounce check units to count before considering that switch is pressed
 * or released */
#define SWITCH_DEBOUNCE_MAX_PERIOD_UNITS    (80u)

/* The function Cy_MCWDT_Enable() waits for some delay in microseconds before
 * returning */
#define MCWDT_0_ENABLE_DELAY                (93u)

#define LED_ON                              (0u)      /* Value to switch LED ON  */
#define LED_OFF                             (!LED_ON) /* Value to switch LED OFF */

/*******************************************************************************
* Global variables
********************************************************************************/
/* Debug UART context */
cy_stc_scb_uart_context_t  DEBUG_UART_context;
/* Debug UART HAL object */
mtb_hal_uart_t DEBUG_UART_hal_obj;
/*******************************************************************************
* Function Prototypes
********************************************************************************/
void handle_error(void);
static uint32_t read_switch_status(void);


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. The application uses cascade of Counter
* 0 and Counter 1 of MCWDT block. The main loop waits till the button switch is
* pressed. Once pressed, it reads the timer value and gets the difference in time
* between the last two switch press events. It then prints the time over UART.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    cy_en_mcwdt_status_t mcwdt_init_status = CY_MCWDT_SUCCESS;

    /* Switch press event count value */
    uint32_t event1_cnt, event2_cnt;
    uint32_t counter1_value, counter0_value;

    /* The time between two presses of switch */
    uint32_t timegap;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;

    /* BSP initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

/* Initialize retarget-io to use the debug UART port */
    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(DEBUG_UART_HW);
    /* Initialize HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);
    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the MCWDT_0 */
    mcwdt_init_status = Cy_MCWDT_Init(MCWDT_0_HW, &MCWDT_0_config);

#ifdef CY_CASCADE_IMPROVEMENT_FEATURE_EN
    Cy_MCWDT_SetCascadeCarryOutRollOver(MCWDT0_HW, CY_MCWDT_CASCADE_C0C1, CY_MCWDT_CASCADE_CARRYOUT_CONFIG_ROLLOVER);
    Cy_MCWDT_SetCascadeMatchCombined(MCWDT0_HW, CY_MCWDT_CASCADE_C0C1,  CY_MCWDT_CASCADE_MATCH_CONFIG_CASCADE);
#endif

    if(mcwdt_init_status!=CY_MCWDT_SUCCESS)
    {
        handle_error();
    }

    /* Enable the MCWDT_0 counters */
    Cy_MCWDT_Enable(MCWDT_0_HW, CY_MCWDT_CTR0|CY_MCWDT_CTR1,
                    MCWDT_0_ENABLE_DELAY); // MCWDT0_THREE_LF_CLK_CYCLES_DELAY

    /* Initialize event count value */
    event1_cnt = 0;
    event2_cnt = 0;

    /* Print a message on UART */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: Free-running MCWDT\r\n");
    printf("************************************************************\r\n\n");

    printf("\r\nMCWDT initialization is complete. Press the user button to "
              "display the time between two presses of the user button. \r\n");


    for(;;)
    {
        /* Check if the switch is pressed.
         * Note that if the switch is pressed, the CPU will not return from
         * read_switch_status() function until the switch is released.
         */
        if (0UL != read_switch_status())
        {
            /* Consider previous key press as 1st key press event */
            event1_cnt = event2_cnt;

            /* Consider current key press as 2nd key press event and get live
             * counter value from MCWDT_0.
             * Note that MCWDT_0 Counter1 is cascaded from MCWDT_0 Counter0
             */
            counter0_value = Cy_MCWDT_GetCount(MCWDT_0_HW, CY_MCWDT_COUNTER0);
            counter1_value = Cy_MCWDT_GetCount(MCWDT_0_HW, CY_MCWDT_COUNTER1);
            event2_cnt = ((counter1_value<<16) | (counter0_value<<0));

            //event2_cnt = Cy_MCWDT_GetCountCascaded(MCWDT0_HW);

            /* Calculate the time between two presses of switch and print on the
             * terminal. MCWDT Counter0 and Counter1 are clocked by LFClk sourced
             * from WCO of frequency 32768 Hz
             */
            if(event2_cnt > event1_cnt)
            {
                timegap = (event2_cnt - event1_cnt)/CY_SYSCLK_WCO_FREQ;
                /* Print the timegap value */
                printf("\r\nThe time between two presses of user button %ds\r\n", (unsigned int)timegap);
            }
            else /* counter overflow */
            {
                /* Print a message on overflow of counter */
                 printf("\r\n\r\nCounter overflow detected\r\n");
            }

        }
    }
}


/*******************************************************************************
* Function Name: read_switch_status
********************************************************************************
* Summary:
*  Reads and returns the current status of the switch.
*
* Parameters:
*  None
*
* Return:
*  Returns non-zero value if switch is pressed and zero otherwise.
*
*******************************************************************************/
uint32_t read_switch_status(void)
{
    uint32_t delayCounter = 0;
    uint32_t sw_status = 0;

    /* Check if the switch is pressed */
    while(0UL == Cy_GPIO_Read(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM))
    {
        /* Switch is pressed. Proceed for debouncing. */
        Cy_SysLib_Delay(SWITCH_DEBOUNCE_CHECK_UNIT);
        ++delayCounter;

        /* Keep checking the switch status till the switch is pressed for a minimum
         * period of SWITCH_DEBOUNCE_CHECK_UNIT x SWITCH_DEBOUNCE_MAX_PERIOD_UNITS
         */
        if (delayCounter > SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
        {
            /* Wait till the switch is released */
            while(0UL == Cy_GPIO_Read(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM))
            {
            }

            /* Debounce when the switch is being released */
            do
            {
                delayCounter = 0;

                while(delayCounter < SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
                {
                    Cy_SysLib_Delay(SWITCH_DEBOUNCE_CHECK_UNIT);
                    ++delayCounter;
                }

            }while(0UL == Cy_GPIO_Read(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM));

            /* Switch is pressed and released*/
            sw_status = 1u;
        }
    }

    return (sw_status);
}


/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* This function processes unrecoverable errors such as UART component
* initialization error. In case of such error the system will Turn on ERROR_LED
* and stay in an infinite loop of this function.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void handle_error(void)
{
     /* Disable all interrupts */
    __disable_irq();

    /* Turn on error LED */
    Cy_GPIO_Write(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN, LED_ON);

    /* Halt the CPU */
    CY_ASSERT(0);

}


/* [] END OF FILE */
