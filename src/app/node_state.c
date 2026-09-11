/**
 * @file node_state.c
 * @brief Pure state-machine logic for the LoRa sensor node.
 */

#include "node_state.h"

static bool NODE_STATE_IsNodeAddress(uint8_t address)
{
    return (address >= LORA_PACKET_MIN_NODE_ADDRESS) &&
           (address <= LORA_PACKET_MAX_NODE_ADDRESS);
}

static bool NODE_STATE_IsSensorCoolingDown(const NODE_STATE_Context *context,
                                           uint32_t tickMs)
{
    uint32_t elapsedMs;

    if (!context->has_sensor_request)
    {
        return false;
    }

    /* Unsigned subtraction keeps elapsed time correct across tick wrap. */
    elapsedMs = tickMs - context->last_sensor_request_tick_ms;
    return elapsedMs < context->sensor_cooldown_ms;
}

bool NODE_STATE_Initialize(NODE_STATE_Context *context,
                           uint8_t node_address,
                           uint32_t sensor_cooldown_ms)
{
    NODE_STATE_Context initialized = {0};

    if ((context == NULL) || !NODE_STATE_IsNodeAddress(node_address))
    {
        return false;
    }

    initialized.state = NODE_STATE_WAIT_POLL;
    initialized.node_address = node_address;
    initialized.sensor_cooldown_ms = sensor_cooldown_ms;
    *context = initialized;
    return true;
}

bool NODE_STATE_HandleEvent(NODE_STATE_Context *context,
                            const NODE_STATE_Event *event,
                            NODE_STATE_Action *action)
{
    LORA_PACKET_Message packet;
    NODE_STATE_Action nextAction = {0};

    if ((context == NULL) || (event == NULL) || (action == NULL))
    {
        return false;
    }

    if (!LORA_PACKET_Decode(event->frame, event->frame_length, &packet))
    {
        nextAction.ignore_reason = NODE_STATE_IGNORE_INVALID_PACKET;
        *action = nextAction;
        return true;
    }

    if (packet.destination != context->node_address)
    {
        nextAction.ignore_reason = NODE_STATE_IGNORE_WRONG_DESTINATION;
        *action = nextAction;
        return true;
    }

    if (packet.type != LORA_PACKET_TYPE_POLL)
    {
        nextAction.ignore_reason = NODE_STATE_IGNORE_UNEXPECTED_TYPE;
        *action = nextAction;
        return true;
    }

    nextAction.transaction_id = packet.transaction_id;
    nextAction.sequence = context->response_sequence;

    if (NODE_STATE_IsSensorCoolingDown(context, event->tick_ms))
    {
        nextAction.type = NODE_STATE_ACTION_SEND_ERROR;
        nextAction.error_code = LORA_PACKET_ERROR_DHT_READ_FAILED;
        *action = nextAction;
        return true;
    }

    context->last_sensor_request_tick_ms = event->tick_ms;
    context->has_sensor_request = true;
    nextAction.type = NODE_STATE_ACTION_READ_SENSOR;
    *action = nextAction;
    return true;
}
