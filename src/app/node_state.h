/**
 * @file node_state.h
 * @brief Pure state-machine logic for the LoRa sensor node.
 */

#ifndef NODE_STATE_H
#define NODE_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol/node_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum time to wait for a matching ACK after a successful transmission. */
#define NODE_STATE_ACK_TIMEOUT_MS 1000U

/** States in one node response transaction. */
typedef enum
{
    NODE_STATE_WAIT_POLL = 0U,
    NODE_STATE_WAIT_TX_RESULT,
    NODE_STATE_WAIT_ACK
} NODE_STATE_State;

/** Events supplied by the radio and timer integration layer. */
typedef enum
{
    NODE_STATE_EVENT_FRAME_RECEIVED = 0U,
    NODE_STATE_EVENT_TX_SUCCEEDED,
    NODE_STATE_EVENT_TX_FAILED,
    NODE_STATE_EVENT_TIMER_TICK
} NODE_STATE_EventType;

/** Work requested from the hardware integration layer. */
typedef enum
{
    NODE_STATE_ACTION_NONE = 0U,
    NODE_STATE_ACTION_READ_SENSOR,
    NODE_STATE_ACTION_SEND_ERROR,
    NODE_STATE_ACTION_TRANSACTION_COMPLETE,
    NODE_STATE_ACTION_ACK_TIMEOUT,
    NODE_STATE_ACTION_TX_FAILED
} NODE_STATE_ActionType;

/** Diagnostic reason when no hardware action is requested. */
typedef enum
{
    NODE_STATE_IGNORE_NONE = 0U,
    NODE_STATE_IGNORE_INVALID_PACKET,
    NODE_STATE_IGNORE_WRONG_DESTINATION,
    NODE_STATE_IGNORE_UNEXPECTED_TYPE,
    NODE_STATE_IGNORE_ACK_MISMATCH,
    NODE_STATE_IGNORE_UNEXPECTED_EVENT
} NODE_STATE_IgnoreReason;

/** Input event supplied by the radio integration layer. */
typedef struct
{
    NODE_STATE_EventType type;
    const uint8_t *frame;
    size_t frame_length;
    uint32_t tick_ms;
} NODE_STATE_Event;

/** Result returned to the hardware integration layer. */
typedef struct
{
    NODE_STATE_ActionType type;
    NODE_STATE_IgnoreReason ignore_reason;
    uint8_t transaction_id;
    uint16_t sequence;
    LORA_PACKET_ErrorCode error_code;
} NODE_STATE_Action;

/** Persistent state owned by one sensor node. */
typedef struct
{
    NODE_STATE_State state;
    uint8_t node_address;
    uint16_t response_sequence;
    uint8_t pending_transaction_id;
    uint16_t pending_sequence;
    uint32_t ack_started_tick_ms;
    uint32_t sensor_cooldown_ms;
    uint32_t last_sensor_request_tick_ms;
    bool has_sensor_request;
} NODE_STATE_Context;

/**
 * Initialize a node in WAIT_POLL.
 *
 * sensor_cooldown_ms is supplied by the integration layer so this pure module
 * does not depend on the DHT11 driver.
 */
bool NODE_STATE_Initialize(NODE_STATE_Context *context,
                           uint8_t node_address,
                           uint32_t sensor_cooldown_ms);

/**
 * Handle one received frame without invoking radio or sensor drivers.
 *
 * A malformed or irrelevant frame is a handled event and returns true with
 * ACTION_NONE plus an ignore reason. False is reserved for invalid API
 * pointers. On false, action remains unchanged.
 */
bool NODE_STATE_HandleEvent(NODE_STATE_Context *context,
                            const NODE_STATE_Event *event,
                            NODE_STATE_Action *action);

#ifdef __cplusplus
}
#endif

#endif /* NODE_STATE_H */
