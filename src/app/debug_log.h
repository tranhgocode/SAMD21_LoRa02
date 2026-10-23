/** Compile-time UART logging shared by application and driver code. */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#ifndef IS_DEBUG
#define IS_DEBUG 1
#endif

#if IS_DEBUG

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "definitions.h"

#define DEBUG_LOG_CAPACITY       224U
#define DEBUG_LOG_WAIT_ATTEMPTS  1000000U

/** Call from task context with interrupts enabled, as required by UART PLIB. */
static inline void DEBUG_LOG_Printf(const char *format, ...)
{
    /* The asynchronous UART source must survive even if its ISR stalls. */
    static char line[DEBUG_LOG_CAPACITY];
    uint32_t attempts = DEBUG_LOG_WAIT_ATTEMPTS;
    va_list arguments;
    int length;

    if (format == NULL)
    {
        return;
    }
    /* Finish the previous transfer before formatting into its source buffer. */
    while (SERCOM5_USART_WriteIsBusy())
    {
        if (--attempts == 0U)
        {
            return;
        }
    }
    va_start(arguments, format);
    length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if ((length <= 0) || ((size_t)length >= sizeof(line)))
    {
        return;
    }
    if (!SERCOM5_USART_Write(line, (size_t)length))
    {
        return;
    }
    while (SERCOM5_USART_WriteIsBusy())
    {
        if (--attempts == 0U)
        {
            return;
        }
    }
}

#define DEBUG_LOG(...) DEBUG_LOG_Printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...) do {} while (0)
#endif

#endif /* DEBUG_LOG_H */
