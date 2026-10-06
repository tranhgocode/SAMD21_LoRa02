/** SAM D21 RTC/Standby integration for the continuously supplied sensor node. */
#ifndef NODE_POWER_H
#define NODE_POWER_H

#include <stdbool.h>
#include <stdint.h>
#include "drivers/sx1278/SX1278.h"

/** Called after SYS_Initialize; reserves RTC Mode 0, DIV1, 1024 Hz and its callback. */
bool NODE_POWER_Initialize(void);

/**
 * Drain UART/SPI, put radio into Sleep, arm RTC and wait in MCU Standby.
 * Call only from thread mode with interrupts enabled. Unrelated interrupts do
 * not end the interval. No reset, peripheral reinitialization or ID/Seq change.
 * Preparation/synchronization failures return false; the caller must halt the
 * application rather than sample/transmit after an incomplete interval.
 */
bool NODE_POWER_Sleep(SX1278_t *radio, uint32_t interval_ms);

#endif /* NODE_POWER_H */
