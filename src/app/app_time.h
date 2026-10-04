/** Powered timebase for MVP2; reserves the otherwise unused TC4/TC5 pair. */
#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdbool.h>
#include <stdint.h>
#include "definitions.h"

static uint32_t loraTimeLastCount;
static uint32_t loraTimeMs;
static uint32_t loraTimeRemainder;

static inline bool LORA_APP_TIME_WaitSync(void)
{
    uint32_t attempts = 1000000U;
    while ((TC4_REGS->COUNT32.TC_STATUS & TC_STATUS_SYNCBUSY_Msk) != 0U)
    {
        if (--attempts == 0U)
        {
            return false;
        }
    }
    return true;
}

static inline bool LORA_APP_TIME_Initialize(void)
{
    uint32_t attempts = 1000000U;

    PM_REGS->PM_APBCMASK |= PM_APBCMASK_TC4_Msk | PM_APBCMASK_TC5_Msk;
    GCLK_REGS->GCLK_CLKCTRL = GCLK_CLKCTRL_ID_TC4_TC5 |
        GCLK_CLKCTRL_GEN(0U) | GCLK_CLKCTRL_CLKEN_Msk;
    while ((GCLK_REGS->GCLK_STATUS & GCLK_STATUS_SYNCBUSY_Msk) != 0U)
    {
        if (--attempts == 0U)
        {
            return false;
        }
    }
    TC4_REGS->COUNT32.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    attempts = 1000000U;
    while ((TC4_REGS->COUNT32.TC_CTRLA & TC_CTRLA_SWRST_Msk) != 0U)
    {
        if (--attempts == 0U)
        {
            return false;
        }
    }
    if (!LORA_APP_TIME_WaitSync())
    {
        return false;
    }
    TC4_REGS->COUNT32.TC_CTRLA = TC_CTRLA_MODE_COUNT32 |
        TC_CTRLA_PRESCALER_DIV1024 | TC_CTRLA_ENABLE_Msk;
    if (!LORA_APP_TIME_WaitSync())
    {
        return false;
    }

    /* SAM D21 datasheet: continuous COUNT synchronization requires RCONT/RREQ. */
    TC4_REGS->COUNT32.TC_READREQ = TC_READREQ_ADDR(TC_COUNT32_COUNT_REG_OFST) |
        TC_READREQ_RCONT_Msk | TC_READREQ_RREQ_Msk;
    if (!LORA_APP_TIME_WaitSync())
    {
        return false;
    }
    loraTimeLastCount = TC4_REGS->COUNT32.TC_COUNT;
    loraTimeMs = 0U;
    loraTimeRemainder = 0U;
    return true;
}

static inline uint32_t LORA_APP_TIME_NowMs(void)
{
    const uint32_t countsPerSecond = CPU_CLOCK_FREQUENCY / 1024U;
    uint32_t count = TC4_REGS->COUNT32.TC_COUNT;
    uint32_t elapsed = count - loraTimeLastCount;
    uint64_t scaled = ((uint64_t)elapsed * 1000U) + loraTimeRemainder;

    /* Retain fractional milliseconds; poll at least once per hardware wrap
     * (about 25 hours at 48 MHz / 1024). No interrupt affects DHT11 timing. */
    loraTimeLastCount = count;
    loraTimeMs += (uint32_t)(scaled / countsPerSecond);
    loraTimeRemainder = (uint32_t)(scaled % countsPerSecond);
    return loraTimeMs;
}

#endif /* APP_TIME_H */
