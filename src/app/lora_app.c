/**
 * @file lora_app.c
 * @brief LoRa V1 sensor-node integration for DHT11 and SX1278.
 */

#include "lora_app.h"

#include <stdint.h>
#include <string.h>

#include "definitions.h"
#include "app/node_response.h"
#include "app/node_state.h"
#include "drivers/sensors/DHT11.h"
#include "drivers/sx1278/SX1278.h"

#define LORA_APP_NODE_ADDRESS            0x01U
#define LORA_APP_FREQUENCY_HZ            433000000ULL
#define LORA_APP_PACKET_LENGTH           LORA_PACKET_MAX_LENGTH
#define LORA_APP_RX_START_TIMEOUT_MS     2U
#define LORA_APP_TX_TIMEOUT_MS           3000U
#define LORA_APP_IDLE_DELAY_MS           1U
#define LORA_APP_SX1278_VERSION_REGISTER 0x42U
#define LORA_APP_SX1278_EXPECTED_VERSION 0x12U
#define LORA_APP_FRAME_LOG_CAPACITY      224U

static SX1278_hw_t loraHardware;
static SX1278_t loraModule =
{
    .hw = &loraHardware
};
static DHT11_HandleTypeDef dht11;
static NODE_STATE_Context nodeContext;
static bool loraReady;
static bool dht11Ready;
static bool receiverStarted;
static uint32_t appTickMs;

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

static char *LORA_APP_AppendHexByte(char *destination, uint8_t value)
{
    static const char hexDigits[] = "0123456789ABCDEF";

    *destination = hexDigits[value >> 4U];
    destination++;
    *destination = hexDigits[value & 0x0FU];
    destination++;
    return destination;
}

static void LORA_APP_PrintFrame(const char *direction,
                                const uint8_t *frame,
                                size_t frameLength)
{
    char line[LORA_APP_FRAME_LOG_CAPACITY];
    char *cursor = line;
    size_t directionLength = strlen(direction);
    size_t index;

    memcpy(cursor, "status: ", sizeof("status: ") - 1U);
    cursor += sizeof("status: ") - 1U;
    memcpy(cursor, direction, directionLength);
    cursor += directionLength;
    memcpy(cursor, " len=", sizeof(" len=") - 1U);
    cursor += sizeof(" len=") - 1U;
    cursor = LORA_APP_AppendUnsigned(cursor, (uint32_t)frameLength);
    memcpy(cursor, " hex=", sizeof(" hex=") - 1U);
    cursor += sizeof(" hex=") - 1U;

    for (index = 0U; index < frameLength; index++)
    {
        cursor = LORA_APP_AppendHexByte(cursor, frame[index]);
        if ((index + 1U) < frameLength)
        {
            *cursor = ' ';
            cursor++;
        }
    }

    memcpy(cursor, "\r\n", sizeof("\r\n"));
    LORA_APP_Print(line);
}

static void LORA_APP_PrintRejectedLength(size_t frameLength)
{
    char line[48];
    char *cursor = line;

    memcpy(cursor,
           "error: rejected RX frame len=",
           sizeof("error: rejected RX frame len=") - 1U);
    cursor += sizeof("error: rejected RX frame len=") - 1U;
    cursor = LORA_APP_AppendUnsigned(cursor, (uint32_t)frameLength);
    memcpy(cursor, "\r\n", sizeof("\r\n"));
    LORA_APP_Print(line);
}

static NODE_RESPONSE_SensorResult LORA_APP_ReadSensor(void)
{
    NODE_RESPONSE_SensorResult result =
    {
        .status = NODE_RESPONSE_SENSOR_INIT_FAILED
    };
    float temperature;
    float humidity;

    if (!dht11Ready)
    {
        return result;
    }

    if (!DHT11_Read(&dht11))
    {
        result.status = NODE_RESPONSE_SENSOR_READ_FAILED;
        return result;
    }

    if ((!DHT11_GetTemperature(&dht11, &temperature)) ||
        (!DHT11_GetHumidity(&dht11, &humidity)) ||
        (temperature < -3276.8f) || (temperature > 3276.7f) ||
        (humidity < 0.0f) || (humidity > 100.0f))
    {
        result.status = NODE_RESPONSE_SENSOR_VALUE_INVALID;
        return result;
    }

    result.status = NODE_RESPONSE_SENSOR_OK;
    result.temperature_x10 = (int16_t)((temperature * 10.0f) +
        ((temperature >= 0.0f) ? 0.5f : -0.5f));
    result.humidity_x10 = (uint16_t)((humidity * 10.0f) + 0.5f);
    return result;
}

static void LORA_APP_LogTerminalAction(const NODE_STATE_Action *action)
{
    switch (action->type)
    {
        case NODE_STATE_ACTION_TRANSACTION_COMPLETE:
            LORA_APP_Print("status: ACK accepted\r\n");
            break;

        case NODE_STATE_ACTION_ACK_TIMEOUT:
            LORA_APP_Print("status: ACK timeout\r\n");
            break;

        case NODE_STATE_ACTION_TX_FAILED:
            LORA_APP_Print("error: LoRa TX failed\r\n");
            break;

        default:
            break;
    }
}

static void LORA_APP_NotifyTxResult(bool succeeded)
{
    NODE_STATE_Event event =
    {
        .type = succeeded ? NODE_STATE_EVENT_TX_SUCCEEDED :
                            NODE_STATE_EVENT_TX_FAILED,
        .frame = NULL,
        .frame_length = 0U,
        .tick_ms = appTickMs
    };
    NODE_STATE_Action action;

    if (!NODE_STATE_HandleEvent(&nodeContext, &event, &action))
    {
        LORA_APP_Print("error: state rejected TX result\r\n");
        return;
    }

    LORA_APP_LogTerminalAction(&action);
}

static void LORA_APP_BuildAndTransmit(const NODE_STATE_Action *action)
{
    NODE_RESPONSE_Request request =
    {
        .node_address = LORA_APP_NODE_ADDRESS,
        .transaction_id = action->transaction_id,
        .sequence = action->sequence
    };
    uint8_t frame[LORA_PACKET_MAX_LENGTH];
    size_t frameLength = 0U;
    NODE_RESPONSE_BuildResult buildResult;
    bool transmitted;

    if (action->type == NODE_STATE_ACTION_READ_SENSOR)
    {
        request.sensor = LORA_APP_ReadSensor();
    }
    else
    {
        request.sensor.status = NODE_RESPONSE_SENSOR_READ_FAILED;
    }

    buildResult = NODE_RESPONSE_Build(&request,
                                      frame,
                                      sizeof(frame),
                                      &frameLength);
    if ((buildResult != NODE_RESPONSE_BUILD_DATA_READY) &&
        (buildResult != NODE_RESPONSE_BUILD_ERROR_READY))
    {
        LORA_APP_Print("error: response build failed\r\n");
        LORA_APP_NotifyTxResult(false);
        return;
    }

    LORA_APP_PrintFrame("tx", frame, frameLength);
    transmitted = SX1278_transmit(&loraModule,
                                  frame,
                                  (uint8_t)frameLength,
                                  LORA_APP_TX_TIMEOUT_MS) != 0;
    receiverStarted = false;
    LORA_APP_NotifyTxResult(transmitted);
}

static void LORA_APP_HandleAction(const NODE_STATE_Action *action)
{
    if ((action->type == NODE_STATE_ACTION_READ_SENSOR) ||
        (action->type == NODE_STATE_ACTION_SEND_ERROR))
    {
        LORA_APP_BuildAndTransmit(action);
        return;
    }

    LORA_APP_LogTerminalAction(action);
}

static void LORA_APP_IdleTick(void)
{
    NODE_STATE_Event event =
    {
        .type = NODE_STATE_EVENT_TIMER_TICK,
        .frame = NULL,
        .frame_length = 0U
    };
    NODE_STATE_Action action;

    SX1278_hw_DelayMs(LORA_APP_IDLE_DELAY_MS);
    appTickMs += LORA_APP_IDLE_DELAY_MS;
    event.tick_ms = appTickMs;

    if (NODE_STATE_HandleEvent(&nodeContext, &event, &action))
    {
        LORA_APP_HandleAction(&action);
    }
}

bool LORA_APP_Initialize(void)
{
    uint8_t version;

    loraReady = false;
    dht11Ready = false;
    receiverStarted = false;
    appTickMs = 0U;

    if (!NODE_STATE_Initialize(&nodeContext,
                               LORA_APP_NODE_ADDRESS,
                               DHT11_MIN_INTERVAL_MS))
    {
        LORA_APP_Print("error: node state initialization failed\r\n");
        return false;
    }

    LORA_APP_Print("\r\nstatus: LoRa-02 configuration: 433 MHz, SF7, BW125, CR4/5, CRC enabled\r\n");

    /* Keep the existing PA00 DHT11 initialization entry point. */
    dht11Ready = DHT11_Init(&dht11,
                            DHT11_DATA_PIN,
                            DHT11_DEFAULT_TIMEOUT);
    if (!dht11Ready)
    {
        LORA_APP_Print("error: DHT11 initialization failed\r\n");
    }

    SX1278_init(&loraModule,
                LORA_APP_FREQUENCY_HZ,
                SX1278_POWER_17DBM,
                SX1278_LORA_SF_7,
                SX1278_LORA_BW_125KHZ,
                SX1278_LORA_CR_4_5,
                SX1278_LORA_CRC_EN,
                LORA_APP_PACKET_LENGTH);

    version = SX1278_SPIRead(&loraModule,
                             LORA_APP_SX1278_VERSION_REGISTER);
    loraReady = (version == LORA_APP_SX1278_EXPECTED_VERSION);
    if (!loraReady)
    {
        LORA_APP_Print("error: SX1278 was not detected; check power and SPI wiring\r\n");
        return false;
    }

    LORA_APP_Print("status: sensor node ready; waiting for POLL\r\n");
    return true;
}

void LORA_APP_Tasks(void)
{
    uint8_t bytesReceived;
    uint8_t frame[SX1278_MAX_PACKET];
    NODE_STATE_Event event;
    NODE_STATE_Action action;

    if (!loraReady)
    {
        LORA_APP_IdleTick();
        return;
    }

    if (!receiverStarted)
    {
        receiverStarted = SX1278_receive(&loraModule,
                                         LORA_APP_PACKET_LENGTH,
                                         LORA_APP_RX_START_TIMEOUT_MS) != 0;
        if (!receiverStarted)
        {
            LORA_APP_Print("error: receiver start failed\r\n");
            LORA_APP_IdleTick();
            return;
        }
    }

    bytesReceived = SX1278_available(&loraModule);
    if (bytesReceived == 0U)
    {
        LORA_APP_IdleTick();
        return;
    }

    bytesReceived = SX1278_read(&loraModule, frame, bytesReceived);
    receiverStarted = false;
    if (bytesReceived <= LORA_PACKET_MAX_LENGTH)
    {
        LORA_APP_PrintFrame("rx", frame, bytesReceived);
    }
    else
    {
        LORA_APP_PrintRejectedLength(bytesReceived);
    }

    event.type = NODE_STATE_EVENT_FRAME_RECEIVED;
    event.frame = frame;
    event.frame_length = bytesReceived;
    event.tick_ms = appTickMs;
    if (!NODE_STATE_HandleEvent(&nodeContext, &event, &action))
    {
        LORA_APP_Print("error: state rejected RX event\r\n");
        return;
    }

    if (action.ignore_reason != NODE_STATE_IGNORE_NONE)
    {
        LORA_APP_Print("status: RX frame ignored\r\n");
    }
    LORA_APP_HandleAction(&action);
}
