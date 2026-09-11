/**
 * @file node_response.c
 * @brief Pure construction of DATA and ERROR responses for a sensor node.
 */

#include "node_response.h"

static bool NODE_RESPONSE_IsNodeAddress(uint8_t address)
{
    return (address >= LORA_PACKET_MIN_NODE_ADDRESS) &&
           (address <= LORA_PACKET_MAX_NODE_ADDRESS);
}

static bool NODE_RESPONSE_IsSensorStatusValid(
    NODE_RESPONSE_SensorStatus status)
{
    return status <= NODE_RESPONSE_SENSOR_VALUE_INVALID;
}

static void NODE_RESPONSE_InitializeMessage(
    const NODE_RESPONSE_Request *request,
    LORA_PACKET_Message *message)
{
    LORA_PACKET_Message initialized = {0};

    initialized.source = request->node_address;
    initialized.destination = LORA_PACKET_GATEWAY_ADDRESS;
    initialized.transaction_id = request->transaction_id;
    initialized.sequence = request->sequence;
    *message = initialized;
}

static NODE_RESPONSE_BuildResult NODE_RESPONSE_BuildError(
    const NODE_RESPONSE_Request *request,
    LORA_PACKET_ErrorCode errorCode,
    uint8_t *output,
    size_t outputCapacity,
    size_t *outputLength)
{
    LORA_PACKET_Message message;

    NODE_RESPONSE_InitializeMessage(request, &message);
    message.type = LORA_PACKET_TYPE_ERROR;
    message.payload_length = LORA_PACKET_ERROR_PAYLOAD_LENGTH;
    message.payload[0] = (uint8_t)errorCode;

    if (!LORA_PACKET_Encode(&message,
                            output,
                            outputCapacity,
                            outputLength))
    {
        return NODE_RESPONSE_BUILD_LOCAL_ERROR;
    }

    return NODE_RESPONSE_BUILD_ERROR_READY;
}

static LORA_PACKET_ErrorCode NODE_RESPONSE_MapSensorError(
    NODE_RESPONSE_SensorStatus status)
{
    switch (status)
    {
        case NODE_RESPONSE_SENSOR_INIT_FAILED:
            return LORA_PACKET_ERROR_DHT_INIT_FAILED;

        case NODE_RESPONSE_SENSOR_READ_FAILED:
            return LORA_PACKET_ERROR_DHT_READ_FAILED;

        case NODE_RESPONSE_SENSOR_VALUE_INVALID:
            return LORA_PACKET_ERROR_DHT_VALUE_INVALID;

        case NODE_RESPONSE_SENSOR_OK:
        default:
            return LORA_PACKET_ERROR_PACKET_BUILD_FAILED;
    }
}

NODE_RESPONSE_BuildResult NODE_RESPONSE_Build(
    const NODE_RESPONSE_Request *request,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    LORA_PACKET_Message message;
    uint16_t temperatureBits;

    if ((request == NULL) || (output == NULL) || (output_length == NULL))
    {
        return NODE_RESPONSE_BUILD_INVALID_ARGUMENT;
    }

    if (!NODE_RESPONSE_IsNodeAddress(request->node_address) ||
        !NODE_RESPONSE_IsSensorStatusValid(request->sensor.status))
    {
        return NODE_RESPONSE_BUILD_INVALID_ARGUMENT;
    }

    if (request->sensor.status != NODE_RESPONSE_SENSOR_OK)
    {
        return NODE_RESPONSE_BuildError(
            request,
            NODE_RESPONSE_MapSensorError(request->sensor.status),
            output,
            output_capacity,
            output_length);
    }

    NODE_RESPONSE_InitializeMessage(request, &message);
    message.type = LORA_PACKET_TYPE_DATA;
    message.payload_length = LORA_PACKET_DATA_PAYLOAD_LENGTH;

    /* Cast first so a negative temperature is serialized as two's complement. */
    temperatureBits = (uint16_t)request->sensor.temperature_x10;
    message.payload[0] = (uint8_t)(temperatureBits >> 8U);
    message.payload[1] = (uint8_t)temperatureBits;
    message.payload[2] = (uint8_t)(request->sensor.humidity_x10 >> 8U);
    message.payload[3] = (uint8_t)request->sensor.humidity_x10;

    if (LORA_PACKET_Encode(&message,
                           output,
                           output_capacity,
                           output_length))
    {
        return NODE_RESPONSE_BUILD_DATA_READY;
    }

    return NODE_RESPONSE_BuildError(
        request,
        LORA_PACKET_ERROR_PACKET_BUILD_FAILED,
        output,
        output_capacity,
        output_length);
}
