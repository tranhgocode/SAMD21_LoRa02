/**
 * @file SX1278_hw.c
 * @brief SAM D21 Curiosity Nano hardware layer for the LoRa-02/SX1278.
 */

#include "SX1278_hw.h"

#include <stdbool.h>
#include <stddef.h>

#include "definitions.h"

static void SX1278_hw_SPITransfer(uint8_t txByte, uint8_t *rxByte)
{
    uint8_t discard;
    uint8_t *destination = (rxByte != NULL) ? rxByte : &discard;

    /* SERCOM1 is generated in interrupt mode, so the stack buffers must remain
       valid until IsBusy() reports completion. */
    while (SERCOM1_SPI_IsBusy())
    {
        /* Wait for the previous transaction. */
    }

    while (!SERCOM1_SPI_WriteRead(&txByte, 1U, destination, 1U))
    {
        /* Retry only if another client acquired the SPI peripheral. */
        while (SERCOM1_SPI_IsBusy())
        {
            /* Wait. */
        }
    }

    while (SERCOM1_SPI_IsBusy())
    {
        /* The SERCOM1 interrupt handler completes the transaction. */
    }
}

void SX1278_hw_init(SX1278_hw_t *hw)
{
    (void)hw;

    LORA_NSS_OutputEnable();
    LORA_RESET_OutputEnable();
    LORA_DIO0_InputEnable();

    LORA_NSS_Set();
    LORA_RESET_Set();
    SX1278_hw_Reset(hw);
}

void SX1278_hw_SetNSS(SX1278_hw_t *hw, int value)
{
    (void)hw;

    if (value == 1)
    {
        LORA_NSS_Set();
    }
    else
    {
        LORA_NSS_Clear();
    }
}

void SX1278_hw_Reset(SX1278_hw_t *hw)
{
    SX1278_hw_SetNSS(hw, 1);
    LORA_RESET_Clear();
    SX1278_hw_DelayMs(1U);
    LORA_RESET_Set();

    /* The SX1278 needs several milliseconds before its SPI registers are
       available after RESET is released. */
    SX1278_hw_DelayMs(10U);
}

void SX1278_hw_SPICommand(SX1278_hw_t *hw, uint8_t cmd)
{
    SX1278_hw_SetNSS(hw, 0);
    SX1278_hw_SPITransfer(cmd, NULL);
}

uint8_t SX1278_hw_SPIReadByte(SX1278_hw_t *hw)
{
    uint8_t value = 0U;

    SX1278_hw_SetNSS(hw, 0);
    SX1278_hw_SPITransfer(0U, &value);
    return value;
}

void SX1278_hw_DelayMs(uint32_t msec)
{
    if (msec == 0U)
    {
        return;
    }

    /* This project is bare-metal and does not otherwise use SysTick. */
    SysTick->LOAD = (CPU_CLOCK_FREQUENCY / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (msec > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
            /* Wait for one millisecond. */
        }
        msec--;
    }

    SysTick->CTRL = 0U;
}

int SX1278_hw_GetDIO0(SX1278_hw_t *hw)
{
    (void)hw;
    return (LORA_DIO0_Get() != 0U) ? 1 : 0;
}
