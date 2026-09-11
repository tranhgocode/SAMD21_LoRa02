/**
 * @file node_packet.c
 * @brief LoRa node packet V1 wire-format codec.
 */

#include "node_packet.h"

#include <string.h>

#include "crc16.h"

/*
 * The first five bytes form the fixed header:
 *
 *   byte 0   1    2    3    4      5 ...
 *        Type Src  Dest ID   Len    Payload | Seq(2) | CRC(2)
 *
 * Because Payload has variable length, the Seq and CRC offsets are calculated
 * at runtime.
 */
#define LORA_PACKET_TYPE_OFFSET       0U
#define LORA_PACKET_SOURCE_OFFSET     1U
#define LORA_PACKET_DESTINATION_OFFSET 2U
#define LORA_PACKET_ID_OFFSET         3U
#define LORA_PACKET_LENGTH_OFFSET     4U
#define LORA_PACKET_PAYLOAD_OFFSET    5U
#define LORA_PACKET_SEQUENCE_SIZE     2U
#define LORA_PACKET_CRC_SIZE          2U

/* V1 nodes use only addresses 0x01 through 0xFE. */
static bool LORA_PACKET_IsNodeAddress(uint8_t address)
{
    return (address >= LORA_PACKET_MIN_NODE_ADDRESS) &&
           (address <= LORA_PACKET_MAX_NODE_ADDRESS);
}

/* An ERROR payload accepts only the four codes defined by the V1 spec. */
static bool LORA_PACKET_IsKnownErrorCode(uint8_t errorCode)
{
    return (errorCode >= (uint8_t)LORA_PACKET_ERROR_DHT_INIT_FAILED) &&
           (errorCode <= (uint8_t)LORA_PACKET_ERROR_PACKET_BUILD_FAILED);
}

/*
 * Validate the contract associated with each Type:
 * - POLL and ACK travel from the gateway to a node.
 * - DATA and ERROR travel from a node to the gateway.
 * - Each Type has a fixed payload_length.
 * - POLL always uses Seq = 0 because the gateway does not yet know the
 *   sequence of the node's response.
 */
static bool LORA_PACKET_IsMessageValid(const LORA_PACKET_Message *message)
{
    if (message->payload_length > LORA_PACKET_MAX_PAYLOAD_LENGTH)
    {
        return false;
    }

    switch (message->type)
    {
        case LORA_PACKET_TYPE_POLL:
            return (message->source == LORA_PACKET_GATEWAY_ADDRESS) &&
                   LORA_PACKET_IsNodeAddress(message->destination) &&
                   (message->payload_length == 0U) &&
                   (message->sequence == 0U);

        case LORA_PACKET_TYPE_DATA:
            return LORA_PACKET_IsNodeAddress(message->source) &&
                   (message->destination == LORA_PACKET_GATEWAY_ADDRESS) &&
                   (message->payload_length == LORA_PACKET_DATA_PAYLOAD_LENGTH);

        case LORA_PACKET_TYPE_ACK:
            return (message->source == LORA_PACKET_GATEWAY_ADDRESS) &&
                   LORA_PACKET_IsNodeAddress(message->destination) &&
                   (message->payload_length == 0U);

        case LORA_PACKET_TYPE_ERROR:
            return LORA_PACKET_IsNodeAddress(message->source) &&
                   (message->destination == LORA_PACKET_GATEWAY_ADDRESS) &&
                   (message->payload_length == LORA_PACKET_ERROR_PAYLOAD_LENGTH) &&
                   LORA_PACKET_IsKnownErrorCode(message->payload[0]);

        default:
            return false;
    }
}

bool LORA_PACKET_Encode(const LORA_PACKET_Message *message,
                        uint8_t *output,
                        size_t output_capacity,
                        size_t *output_length)
{
    uint8_t frame[LORA_PACKET_MAX_LENGTH];
    size_t sequenceOffset;
    size_t crcOffset;
    size_t frameLength;
    uint16_t crc;

    /* Do not dereference any pointer before checking it for NULL. */
    if ((message == NULL) || (output == NULL) || (output_length == NULL))
    {
        return false;
    }

    /* Validate the complete contract before calculating or writing output. */
    if (!LORA_PACKET_IsMessageValid(message))
    {
        return false;
    }

    /* A frame contains 9 mandatory bytes plus its payload bytes. */
    frameLength = LORA_PACKET_MIN_LENGTH + message->payload_length;
    if (output_capacity < frameLength)
    {
        return false;
    }

    /* Write the five fixed header fields into the temporary frame. */
    frame[LORA_PACKET_TYPE_OFFSET] = (uint8_t)message->type;
    frame[LORA_PACKET_SOURCE_OFFSET] = message->source;
    frame[LORA_PACKET_DESTINATION_OFFSET] = message->destination;
    frame[LORA_PACKET_ID_OFFSET] = message->transaction_id;
    frame[LORA_PACKET_LENGTH_OFFSET] = message->payload_length;
    (void)memcpy(&frame[LORA_PACKET_PAYLOAD_OFFSET],
                 message->payload,
                 message->payload_length);

    /* Multi-byte wire fields always use big-endian byte order. */
    sequenceOffset = LORA_PACKET_PAYLOAD_OFFSET + message->payload_length;
    frame[sequenceOffset] = (uint8_t)(message->sequence >> 8U);
    frame[sequenceOffset + 1U] = (uint8_t)message->sequence;

    /* CRC covers Type through Seq and excludes the two CRC bytes. */
    crcOffset = sequenceOffset + LORA_PACKET_SEQUENCE_SIZE;
    if (!CRC16_CalculateCcittFalse(frame, crcOffset, &crc))
    {
        return false;
    }

    frame[crcOffset] = (uint8_t)(crc >> 8U);
    frame[crcOffset + 1U] = (uint8_t)crc;

    /*
     * Copy the temporary frame to the caller only after every step succeeds.
     * This keeps output and output_length unchanged when encoding fails.
     */
    (void)memcpy(output, frame, frameLength);
    *output_length = frameLength;
    return true;
}

bool LORA_PACKET_Decode(const uint8_t *input,
                        size_t input_length,
                        LORA_PACKET_Message *message)
{
    LORA_PACKET_Message decoded = {0};
    uint8_t payloadLength;
    size_t sequenceOffset;
    size_t crcOffset;
    uint16_t calculatedCrc;
    uint16_t receivedCrc;

    /* Decode safely in this order: pointers -> frame length -> Len -> data. */
    if ((input == NULL) || (message == NULL))
    {
        return false;
    }

    /* A V1 frame must be between 9 bytes and the 64-byte radio limit. */
    if ((input_length < LORA_PACKET_MIN_LENGTH) ||
        (input_length > LORA_PACKET_MAX_LENGTH))
    {
        return false;
    }

    /* Len at offset 4 is safe to read only after confirming 9 input bytes. */
    payloadLength = input[LORA_PACKET_LENGTH_OFFSET];
    if (payloadLength > LORA_PACKET_MAX_PAYLOAD_LENGTH)
    {
        return false;
    }

    /* Reject extra or missing bytes before accessing Payload, Seq, or CRC. */
    if (input_length != (LORA_PACKET_MIN_LENGTH + payloadLength))
    {
        return false;
    }

    /*
     * Parse into a local value instead of writing directly to the caller's
     * output. If validation fails, the caller retains its previous message.
     */
    decoded.type = (LORA_PACKET_Type)input[LORA_PACKET_TYPE_OFFSET];
    decoded.source = input[LORA_PACKET_SOURCE_OFFSET];
    decoded.destination = input[LORA_PACKET_DESTINATION_OFFSET];
    decoded.transaction_id = input[LORA_PACKET_ID_OFFSET];
    decoded.payload_length = payloadLength;
    (void)memcpy(decoded.payload,
                 &input[LORA_PACKET_PAYLOAD_OFFSET],
                 payloadLength);

    /* Seq follows the payload and is reconstructed in big-endian order. */
    sequenceOffset = LORA_PACKET_PAYLOAD_OFFSET + payloadLength;
    decoded.sequence = (uint16_t)((uint16_t)input[sequenceOffset] << 8U);
    decoded.sequence |= input[sequenceOffset + 1U];

    /* Reject an invalid Type, direction, address, Len, or error code. */
    if (!LORA_PACKET_IsMessageValid(&decoded))
    {
        return false;
    }

    /* Recalculate CRC over the same byte range used by the encoder. */
    crcOffset = sequenceOffset + LORA_PACKET_SEQUENCE_SIZE;
    if (!CRC16_CalculateCcittFalse(input, crcOffset, &calculatedCrc))
    {
        return false;
    }

    receivedCrc = (uint16_t)((uint16_t)input[crcOffset] << 8U);
    receivedCrc |= input[crcOffset + 1U];
    /* The two received CRC bytes also use big-endian order. */
    if (receivedCrc != calculatedCrc)
    {
        return false;
    }

    /* Commit the result only after the frame passes every validation step. */
    *message = decoded;
    return true;
}
