/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>                     // Defines strlen
#include "definitions.h"                // SYS function prototypes


// *****************************************************************************
// Section: UART Helper Functions
// *****************************************************************************

static void UART_Print(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    /* Wait until the previous transmission has finished. */
    while (SERCOM5_USART_WriteIsBusy())
    {
        /* Do nothing */
    }

    (void)SERCOM5_USART_Write((void *)text, strlen(text));

    /* Keep text buffers valid until the complete string is copied to UART. */
    while (SERCOM5_USART_WriteIsBusy())
    {
        /* Do nothing */
    }
}


// *****************************************************************************
// Section: Timing Helper Functions
// *****************************************************************************

static void DelayMs(uint32_t milliseconds)
{
    /* Configure SysTick to count one millisecond at a 48 MHz CPU clock. */
    SysTick->LOAD = (CPU_CLOCK_FREQUENCY / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (milliseconds > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
            /* Wait for one millisecond. */
        }

        milliseconds--;
    }

    SysTick->CTRL = 0U;
}


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    UART_Print("\r\nLoRa TX started\r\n");

    while ( true )
    {
        UART_Print("Hello\r\n");
        DelayMs(1000U);

        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

