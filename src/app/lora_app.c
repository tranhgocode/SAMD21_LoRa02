/**
 * @file lora_app.c
 * @brief Minimal LoRa-02 link test at 433 MHz.
 */

#include "lora_app.h"

#include <stdint.h>
#include <string.h>

#include "definitions.h"
#include "drivers/sx1278/SX1278.h"
#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
#include "../drivers/sensors/DHT11.h"
#endif

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
#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
static DHT11_HandleTypeDef dht11;
static bool dht11Ready;
#endif

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

#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
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

static char *LORA_APP_AppendTenths(char *destination, float value)
{
    /* Format one decimal place without linking the floating-point printf code. */
    uint32_t valueTenths = (uint32_t)((value * 10.0f) + 0.5f);

    destination = LORA_APP_AppendUnsigned(destination, valueTenths / 10U);
    *destination = '.';
    destination++;
    *destination = (char)('0' + (valueTenths % 10U));
    destination++;

    return destination;
}
#endif

bool LORA_APP_Initialize(void)
{
    uint8_t version;

    LORA_APP_Print("\r\nstatus: LoRa-02 configuration: 433 MHz, SF7, BW125, CR4/5, CRC enabled\r\n");

#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
    dht11Ready = DHT11_Init(&dht11, DHT11_DATA_PIN, DHT11_DEFAULT_TIMEOUT);
    if (!dht11Ready)
    {
        LORA_APP_Print("error: DHT11 initialization failed\r\n");
    }
#endif

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
        LORA_APP_Print("error: SX1278 was not detected; check power and SPI wiring\r\n");
        return false;
    }

#if (LORA_APP_ROLE == LORA_APP_ROLE_TRANSMITTER)
    LORA_APP_Print("status: transmitter ready; sending DHT11 data every 2 seconds\r\n");
#else
    LORA_APP_Print("status: receiver ready; waiting for LoRa packets\r\n");
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
        float temperature;
        float humidity;
        uint8_t length;

        if ((!dht11Ready) ||
            (!DHT11_Read(&dht11)) ||
            (!DHT11_GetTemperature(&dht11, &temperature)) ||
            (!DHT11_GetHumidity(&dht11, &humidity)))
        {
            LORA_APP_Print("error: failed to read valid DHT11 data\r\n");
            SX1278_hw_DelayMs(LORA_APP_TX_PERIOD_MS);
            return;
        }

        /* Keep the radio payload compact and independent of the UART log text. */
        memcpy(end, "T=", 2U);
        end += 2;
        end = LORA_APP_AppendTenths(end, temperature);
        memcpy(end, "C H=", 4U);
        end += 4;
        end = LORA_APP_AppendTenths(end, humidity);
        *end = '%';
        end++;
        length = (uint8_t)(end - (char *)message);

        if (SX1278_transmit(&loraModule,
                           message,
                           length,
                           LORA_APP_RADIO_TIMEOUT_MS) != 0)
        {
            *end = '\0';
            LORA_APP_Print("status: sent: ");
            LORA_APP_Print((char *)message);
            LORA_APP_Print("\r\n");
        }
        else
        {
            LORA_APP_Print("error: LoRa transmission timed out\r\n");
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
            LORA_APP_Print("error: receiver start timed out; retrying\r\n");
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
            LORA_APP_Print("status: received: ");
            LORA_APP_Print((char *)message);
            LORA_APP_Print("\r\n");

            /* Re-enter RX mode after the completed packet. */
            receiverStarted = false;
        }
    }
#endif
}
