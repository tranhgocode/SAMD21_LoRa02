/**
 * @file node_response.h
 * @brief Pure construction of DATA and ERROR responses for a sensor node.
 */

#ifndef NODE_RESPONSE_H
#define NODE_RESPONSE_H

#include <stddef.h>
#include <stdint.h>

#include "protocol/node_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Sensor outcomes reported by the hardware integration layer. */
typedef enum
{
    NODE_RESPONSE_SENSOR_OK = 0U,
    NODE_RESPONSE_SENSOR_INIT_FAILED,
    NODE_RESPONSE_SENSOR_READ_FAILED,
    NODE_RESPONSE_SENSOR_VALUE_INVALID
} NODE_RESPONSE_SensorStatus;

/** Fixed-point sample or failure status supplied to the response builder. */
typedef struct
{
    NODE_RESPONSE_SensorStatus status;
    int16_t temperature_x10;
    uint16_t humidity_x10;
} NODE_RESPONSE_SensorResult;

/** Complete transaction input needed to construct one response. */
typedef struct
{
    uint8_t node_address;
    uint8_t transaction_id;
    uint16_t sequence;
    NODE_RESPONSE_SensorResult sensor;
} NODE_RESPONSE_Request;

/** Outcome of a response build attempt. */
typedef enum
{
    NODE_RESPONSE_BUILD_INVALID_ARGUMENT = 0U,
    NODE_RESPONSE_BUILD_DATA_READY,
    NODE_RESPONSE_BUILD_ERROR_READY,
    NODE_RESPONSE_BUILD_LOCAL_ERROR
} NODE_RESPONSE_BuildResult;

/**
 * Build one DATA or ERROR frame without calling sensor or radio drivers.
 *
 * If DATA encoding fails, the function attempts an ERROR response containing
 * LORA_PACKET_ERROR_PACKET_BUILD_FAILED. LOCAL_ERROR means that no valid
 * radio frame could be produced and the integration layer must log locally.
 * On INVALID_ARGUMENT or LOCAL_ERROR, output and output_length remain
 * unchanged.
 */
NODE_RESPONSE_BuildResult NODE_RESPONSE_Build(
    const NODE_RESPONSE_Request *request,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif /* NODE_RESPONSE_H */
