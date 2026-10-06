#include "node_power.h"

#include <stddef.h>
#include "definitions.h"

#define NODE_POWER_CLOCK_HZ       1024U
#define NODE_POWER_WAIT_ATTEMPTS  1000000U
#define NODE_POWER_RADIO_OP_MODE  0x01U
#define NODE_POWER_RADIO_MODE_MASK 0xF8U

static volatile bool rtcWakePending;
static bool powerReady;
/* PLIB SPI is asynchronous. These buffers survive even a failed transfer. */
static uint8_t spiTx[2];
static uint8_t spiRx[2];

static bool NODE_POWER_WaitRtcSync(void)
{
    uint32_t attempts = NODE_POWER_WAIT_ATTEMPTS;
    while ((RTC_REGS->MODE0.RTC_STATUS & RTC_STATUS_SYNCBUSY_Msk) != 0U)
    {
        if (--attempts == 0U)
        {
            return false;
        }
    }
    return true;
}

static void NODE_POWER_RtcCallback(RTC_TIMER32_INT_MASK cause, uintptr_t context)
{
    (void)context;
    if ((cause & RTC_TIMER32_INT_MASK_COMPARE_MATCH) != 0U)
    {
        rtcWakePending = true;
    }
}

static bool NODE_POWER_StopRtc(void)
{
    /* A register write during synchronization can stall the APB bus itself;
     * do not attempt cleanup writes after a stopped-clock sync timeout.
     * SAM D21 datasheet 14.3.1.2 and 14.3.1.6.
     */
    if (!NODE_POWER_WaitRtcSync())
    {
        return false;
    }
    RTC_REGS->MODE0.RTC_INTENCLR = RTC_MODE0_INTENSET_Msk;
    RTC_REGS->MODE0.RTC_CTRL &= (uint16_t)~RTC_MODE0_CTRL_ENABLE_Msk;
    if (!NODE_POWER_WaitRtcSync())
    {
        return false;
    }
    RTC_REGS->MODE0.RTC_INTFLAG = RTC_MODE0_INTFLAG_Msk;
    NVIC_ClearPendingIRQ(RTC_IRQn);
    return true;
}

static bool NODE_POWER_WaitSpi(void)
{
    uint32_t attempts = NODE_POWER_WAIT_ATTEMPTS;
    while (SERCOM1_SPI_IsBusy())
    {
        if (--attempts == 0U)
        {
            return false;
        }
    }
    return true;
}

static bool NODE_POWER_DrainPeripherals(void)
{
    uint32_t attempts = NODE_POWER_WAIT_ATTEMPTS;
    while (SERCOM5_USART_WriteIsBusy() || !SERCOM5_USART_TransmitComplete())
    {
        if (--attempts == 0U)
        {
            return false;
        }
    }
    return NODE_POWER_WaitSpi();
}

static bool NODE_POWER_RadioRegister(SX1278_t *radio, uint8_t command, uint8_t value)
{
    bool transferred;

    if (!NODE_POWER_WaitSpi())
    {
        return false;
    }
    spiTx[0] = command;
    spiTx[1] = value;
    SX1278_hw_SetNSS(radio->hw, 0);
    transferred = SERCOM1_SPI_WriteRead(spiTx, sizeof(spiTx), spiRx, sizeof(spiRx));
    if (transferred)
    {
        transferred = NODE_POWER_WaitSpi();
    }
    SX1278_hw_SetNSS(radio->hw, 1);
    return transferred;
}

static bool NODE_POWER_SleepRadio(SX1278_t *radio)
{
    uint8_t sleepMode;

    if (!NODE_POWER_RadioRegister(radio, NODE_POWER_RADIO_OP_MODE, 0U))
    {
        return false;
    }
    /* Keep LoRa and the LF/HF selection; change only the three mode bits.
     * https://github.com/Lora-net/LoRaMac-node/blob/master/src/radio/sx1276/sx1276.c
     */
    if ((spiRx[1] & 0x80U) == 0U)
    {
        return false;
    }
    sleepMode = spiRx[1] & NODE_POWER_RADIO_MODE_MASK;
    if (!NODE_POWER_RadioRegister(radio, NODE_POWER_RADIO_OP_MODE | 0x80U, sleepMode) ||
        !NODE_POWER_RadioRegister(radio, NODE_POWER_RADIO_OP_MODE, 0U) ||
        (spiRx[1] != sleepMode))
    {
        return false;
    }
    radio->status = SLEEP;
    radio->readBytes = 0U;
    return true;
}

bool NODE_POWER_Initialize(void)
{
    bool interruptState;

    powerReady = false;
    if ((RTC_Timer32FrequencyGet() != NODE_POWER_CLOCK_HZ) ||
        (NVIC_GetEnableIRQ(RTC_IRQn) == 0U) ||
        !NODE_POWER_WaitRtcSync() ||
        ((RTC_REGS->MODE0.RTC_CTRL & (RTC_MODE0_CTRL_MODE_Msk |
          RTC_MODE0_CTRL_PRESCALER_Msk)) != 0U))
    {
        return false;
    }
    interruptState = NVIC_INT_Disable();
    powerReady = NODE_POWER_StopRtc();
    rtcWakePending = false;
    RTC_Timer32CallbackRegister(NODE_POWER_RtcCallback, 0U);
    NVIC_INT_Restore(interruptState);
    return powerReady;
}

bool NODE_POWER_Sleep(SX1278_t *radio, uint32_t interval_ms)
{
    uint64_t ticks = (((uint64_t)interval_ms * NODE_POWER_CLOCK_HZ) + 999U) / 1000U;
    bool interruptState;
    uint32_t savedSleepDeep;

    if (!powerReady || (radio == NULL) || (radio->hw == NULL) ||
        (ticks == 0U) || (ticks > UINT32_MAX) || (__get_IPSR() != 0U) ||
        (NVIC_GetEnableIRQ(RTC_IRQn) == 0U))
    {
        return false;
    }
    interruptState = NVIC_INT_Disable();
    NVIC_INT_Restore(interruptState);
    if (!interruptState)
    {
        return false;
    }
    /* SPI needs its interrupt while draining and preparing radio Sleep. */
    if (!NODE_POWER_DrainPeripherals() || !NODE_POWER_SleepRadio(radio))
    {
        powerReady = false;
        return false;
    }

    interruptState = NVIC_INT_Disable();
    powerReady = NODE_POWER_StopRtc();
    rtcWakePending = false;
    if (powerReady)
    {
        RTC_REGS->MODE0.RTC_COUNT = 0U;
        powerReady = NODE_POWER_WaitRtcSync();
    }
    if (powerReady)
    {
        RTC_REGS->MODE0.RTC_COMP = (uint32_t)ticks;
        powerReady = NODE_POWER_WaitRtcSync();
    }
    if (powerReady)
    {
        RTC_REGS->MODE0.RTC_INTENSET = RTC_TIMER32_INT_MASK_COMPARE_MATCH;
        RTC_REGS->MODE0.RTC_CTRL |= RTC_MODE0_CTRL_ENABLE_Msk;
        powerReady = NODE_POWER_WaitRtcSync();
    }
    NVIC_INT_Restore(interruptState);
    if (!powerReady)
    {
        (void)NODE_POWER_StopRtc();
        return false;
    }

    savedSleepDeep = SCB->SCR & SCB_SCR_SLEEPDEEP_Msk;
    while (!rtcWakePending)
    {
        /* PRIMASK protects the final flag check. A pending masked interrupt
         * still releases WFI, so a compare between this check and WFI is safe.
         * https://arm-software.github.io/CMSIS_5/Core/html/group__intrinsic__CPU__gr.html
         * SAM D21 datasheet 16.6.2.8.2, 19.6.7: RTC runs through Standby;
         * SRAM and peripheral settings survive, no SYS_Initialize on wake.
         */
        interruptState = NVIC_INT_Disable();
        if (!rtcWakePending)
        {
            __DSB();
            PM_StandbyModeEnter();
        }
        SCB->SCR = (SCB->SCR & ~SCB_SCR_SLEEPDEEP_Msk) | savedSleepDeep;
        NVIC_INT_Restore(interruptState);
        __ISB();
    }
    interruptState = NVIC_INT_Disable();
    powerReady = NODE_POWER_StopRtc();
    NVIC_INT_Restore(interruptState);
    return powerReady;
}
