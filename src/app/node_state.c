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

static void NODE_STATE_CompleteTransaction(NODE_STATE_Context *context,
                                           NODE_STATE_Action *action,
                                           NODE_STATE_ActionType actionType)
{
    /* The explicit cast documents the intended 0xFFFF-to-zero wrap. */
    context->response_sequence =
        (uint16_t)(context->response_sequence + 1U);
    context->state = NODE_STATE_WAIT_POLL;
    context->pending_transaction_id = 0U;
    context->pending_sequence = 0U;
    context->ack_started_tick_ms = 0U;
    action->type = actionType;
}

static void NODE_STATE_HandleWaitPoll(NODE_STATE_Context *context,
                                      const NODE_STATE_Event *event,
                                      NODE_STATE_Action *action)
{
    LORA_PACKET_Message packet;

    if (event->type != NODE_STATE_EVENT_FRAME_RECEIVED)
    {
        action->ignore_reason = NODE_STATE_IGNORE_UNEXPECTED_EVENT;
        return;
    }

    if (!LORA_PACKET_Decode(event->frame, event->frame_length, &packet))
    {
        action->ignore_reason = NODE_STATE_IGNORE_INVALID_PACKET;
        return;
    }

    if (packet.destination != context->node_address)
    {
        action->ignore_reason = NODE_STATE_IGNORE_WRONG_DESTINATION;
        return;
    }

    if (packet.type != LORA_PACKET_TYPE_POLL)
    {
        action->ignore_reason = NODE_STATE_IGNORE_UNEXPECTED_TYPE;
        return;
    }

    /* Freeze the ID and sequence until TX and ACK processing is complete. */
    context->pending_transaction_id = packet.transaction_id;
    context->pending_sequence = context->response_sequence;
    context->state = NODE_STATE_WAIT_TX_RESULT;
    action->transaction_id = packet.transaction_id;
    action->sequence = context->response_sequence;

    if (NODE_STATE_IsSensorCoolingDown(context, event->tick_ms))
    {
        action->type = NODE_STATE_ACTION_SEND_ERROR;
        action->error_code = LORA_PACKET_ERROR_DHT_READ_FAILED;
        return;
    }

    context->last_sensor_request_tick_ms = event->tick_ms;
    context->has_sensor_request = true;
    action->type = NODE_STATE_ACTION_READ_SENSOR;
}

static void NODE_STATE_HandleWaitTxResult(NODE_STATE_Context *context,
                                          const NODE_STATE_Event *event,
                                          NODE_STATE_Action *action)
{
    if (event->type == NODE_STATE_EVENT_TX_SUCCEEDED)
    {
        context->state = NODE_STATE_WAIT_ACK;
        context->ack_started_tick_ms = event->tick_ms;
        return;
    }

    if (event->type == NODE_STATE_EVENT_TX_FAILED)
    {
        NODE_STATE_CompleteTransaction(context,
                                       action,
                                       NODE_STATE_ACTION_TX_FAILED);
        return;
    }

    action->ignore_reason = NODE_STATE_IGNORE_UNEXPECTED_EVENT;
}

static void NODE_STATE_HandleWaitAck(NODE_STATE_Context *context,
                                     const NODE_STATE_Event *event,
                                     NODE_STATE_Action *action)
{
    LORA_PACKET_Message packet;
    uint32_t elapsedMs = event->tick_ms - context->ack_started_tick_ms;

    /* Timeout wins at the exact boundary, so an ACK at 1000 ms is late. */
    if (elapsedMs >= NODE_STATE_ACK_TIMEOUT_MS)
    {
        NODE_STATE_CompleteTransaction(context,
                                       action,
                                       NODE_STATE_ACTION_ACK_TIMEOUT);
        return;
    }

    if (event->type == NODE_STATE_EVENT_TIMER_TICK)
    {
        return;
    }

    if (event->type != NODE_STATE_EVENT_FRAME_RECEIVED)
    {
        action->ignore_reason = NODE_STATE_IGNORE_UNEXPECTED_EVENT;
        return;
    }

    if (!LORA_PACKET_Decode(event->frame, event->frame_length, &packet))
    {
        action->ignore_reason = NODE_STATE_IGNORE_INVALID_PACKET;
        return;
    }

    if (packet.destination != context->node_address)
    {
        action->ignore_reason = NODE_STATE_IGNORE_WRONG_DESTINATION;
        return;
    }

    if (packet.type != LORA_PACKET_TYPE_ACK)
    {
        action->ignore_reason = NODE_STATE_IGNORE_UNEXPECTED_TYPE;
        return;
    }

    if ((packet.transaction_id != context->pending_transaction_id) ||
        (packet.sequence != context->pending_sequence))
    {
        action->ignore_reason = NODE_STATE_IGNORE_ACK_MISMATCH;
        return;
    }

    NODE_STATE_CompleteTransaction(context,
                                   action,
                                   NODE_STATE_ACTION_TRANSACTION_COMPLETE);
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
    NODE_STATE_Action nextAction = {0};

    if ((context == NULL) || (event == NULL) || (action == NULL))
    {
        return false;
    }

    switch (context->state)
    {
        case NODE_STATE_WAIT_POLL:
            NODE_STATE_HandleWaitPoll(context, event, &nextAction);
            break;

        case NODE_STATE_WAIT_TX_RESULT:
            NODE_STATE_HandleWaitTxResult(context, event, &nextAction);
            break;

        case NODE_STATE_WAIT_ACK:
            NODE_STATE_HandleWaitAck(context, event, &nextAction);
            break;

        default:
            return false;
    }

    *action = nextAction;
    return true;
}
