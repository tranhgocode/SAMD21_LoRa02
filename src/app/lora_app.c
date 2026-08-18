/**
 * @file lora_app.c
 * @brief Minimal LoRa-02 link test at 433 MHz.
 */

#include "lora_app.h"

#include <stdint.h>
#include <string.h>

#include "definitions.h"
#include "drivers/sx1278/SX1278.h"

#define LORA_APP_FREQUENCY_HZ       433000000ULL
#define LORA_APP_PACKET_LENGTH      64U
#define LORA_APP_TX_PERIOD_MS       2000U
#define LORA_APP_RADIO_TIMEOUT_MS   3000U
#define SX1278_VERSION_REGISTER     0x42U
#define SX1278_EXPECTED_VERSION     0x12U

#if ((LORA_APP_ROLE != LORA_APP_ROLE_TRANSMITTER) && \
     (LORA_APP_ROLE != LORA_APP_ROLE_RECEIVER))
#error "LORA_APP_ROLE must be LORA_APP_ROLE_TRANSMITTER or LORA_APP_ROLE_RECEIVER"
#endif

static SX1278_hw_t loraHardware;
static SX1278_t loraModule =
{
    .hw = &loraHardware
};
static bool loraReady;
static bool receiverStarted;
static uint32_t packetCounter;

static void LORA_APP_Print(const char *text)
{
    size_t length;

    if (text == NULL)
    {
        return;
    }

    length = strlen(text);
    while (SERCOM5_USART_WriteIsBusy())
    {
        /* Wait for the preceding UART message. */
    }
    (void)SERCOM5_USART_Write((void *)text, length);
    while (SERCOM5_USART_WriteIsBusy())
    {
        /* Keep the source buffer valid until transmission completes. */
    }
}

static char *LORA_APP_AppendUnsigned(char *destination, uint32_t value)
{
    char reversed[10];
    uint32_t count = 0U;

    do
    {
        reversed[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    } while (value != 0U);

    while (count > 0U)
    {
        count--;
        *destination = reversed[count];
        destination++;
    }

    return destination;
}

bool LORA_APP_Initialize(void)
{
    uint8_t version;

    LORA_APP_Print("\r\nLoRa-02 test: 433 MHz, SF7, BW125, CR4/5, CRC on\r\n");

    SX1278_init(&loraModule,
                LORA_APP_FREQUENCY_HZ,
                SX1278_POWER_17DBM,
                SX1278_LORA_SF_7,
                SX1278_LORA_BW_125KHZ,
                SX1278_LORA_CR_4_5,
                SX1278_LORA_CRC_EN,
                LORA_APP_PACKET_LENGTH);

    version = SX1278_SPIRead(&loraModule, SX1278_VERSION_REGISTER);
    loraReady = (version == SX1278_EXPECTED_VERSION);

    if (!loraReady)
    {
        LORA_APP_Print("ERROR: SX1278 not found (check 3.3 V, GND, SPI and NSS).\r\n");
        return false;
    }

#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
    LORA_APP_Print("Role: TRANSMITTER - sending one packet every 2 seconds.\r\n");
#else
    LORA_APP_Print("Role: RECEIVER - waiting for packets.\r\n");
#endif

    return true;
}

void LORA_APP_Tasks(void)
{
    if (!loraReady)
    {
        SX1278_hw_DelayMs(1000U);
        return;
    }

#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
    {
        uint8_t message[32];
        char *end = (char *)message;
        uint8_t length;

        memcpy(end, "Hello LoRa #", 12U);
        end += 12;
        end = LORA_APP_AppendUnsigned(end, packetCounter);
        length = (uint8_t)(end - (char *)message);

        if (SX1278_transmit(&loraModule,
                           message,
                           length,
                           LORA_APP_RADIO_TIMEOUT_MS) != 0)
        {
            LORA_APP_Print("TX OK: ");
            *end = '\0';
            LORA_APP_Print((char *)message);
            LORA_APP_Print("\r\n");
            packetCounter++;
        }
        else
        {
            LORA_APP_Print("TX TIMEOUT\r\n");
        }

        SX1278_hw_DelayMs(LORA_APP_TX_PERIOD_MS);
    }
#else
    if (!receiverStarted)
    {
        receiverStarted = (SX1278_receive(&loraModule,
                                          LORA_APP_PACKET_LENGTH,
                                          LORA_APP_RADIO_TIMEOUT_MS) != 0);
        if (!receiverStarted)
        {
            LORA_APP_Print("RX START TIMEOUT - retrying\r\n");
        }
    }
    else
    {
        uint8_t bytesReceived = SX1278_available(&loraModule);

        if (bytesReceived > 0U)
        {
            /* SX1278_read() appends a terminator after as many as 255 bytes. */
            uint8_t message[SX1278_MAX_PACKET];

            (void)SX1278_read(&loraModule, message, bytesReceived);
            LORA_APP_Print("RX OK: ");
            LORA_APP_Print((char *)message);
            LORA_APP_Print("\r\n");

            /* Re-enter RX mode after the completed packet. */
            receiverStarted = false;
        }
    }
#endif
}
