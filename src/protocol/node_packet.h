/**
 * @file node_packet.h
 * @brief LoRa node packet V1 wire-format codec.
 */

#ifndef NODE_PACKET_H
#define NODE_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Address 0x00 identifies the gateway; 0xFF is reserved and unused in V1. */
#define LORA_PACKET_GATEWAY_ADDRESS       0x00U
#define LORA_PACKET_RESERVED_ADDRESS      0xFFU
#define LORA_PACKET_MIN_NODE_ADDRESS      0x01U
#define LORA_PACKET_MAX_NODE_ADDRESS      0xFEU

/*
 * Every frame contains 9 mandatory bytes:
 * Type(1) + Src(1) + Dest(1) + ID(1) + Len(1) + Seq(2) + CRC(2).
 * Payload appears between Len and Seq, so frame length is 9 + payload_length.
 */
#define LORA_PACKET_MIN_LENGTH            9U
#define LORA_PACKET_MAX_LENGTH            64U
#define LORA_PACKET_MAX_PAYLOAD_LENGTH    55U
#define LORA_PACKET_DATA_PAYLOAD_LENGTH   4U
#define LORA_PACKET_ERROR_PAYLOAD_LENGTH  1U

/** The only four packet types accepted by protocol V1. */
typedef enum
{
    /* The gateway asks a node to take a new sensor sample. */
    LORA_PACKET_TYPE_POLL = 0x01U,
    /* A node returns valid temperature and humidity data. */
    LORA_PACKET_TYPE_DATA = 0x02U,
    /* The gateway acknowledges the current DATA or ERROR response. */
    LORA_PACKET_TYPE_ACK = 0x03U,
    /* A node reports why it could not produce a DATA response. */
    LORA_PACKET_TYPE_ERROR = 0x04U
} LORA_PACKET_Type;

/** Error codes allowed in the one-byte ERROR payload. */
typedef enum
{
    LORA_PACKET_ERROR_DHT_INIT_FAILED = 0x01U,
    LORA_PACKET_ERROR_DHT_READ_FAILED = 0x02U,
    LORA_PACKET_ERROR_DHT_VALUE_INVALID = 0x03U,
    LORA_PACKET_ERROR_PACKET_BUILD_FAILED = 0x04U
} LORA_PACKET_ErrorCode;

/**
 * In-memory representation of a packet.
 *
 * This structure is used only in RAM. The encoder writes every field into the
 * wire format explicitly; never transmit the raw structure because padding
 * and byte order may differ between compilers and MCUs.
 */
typedef struct
{
    /** Determines the transmission direction and payload meaning. */
    LORA_PACKET_Type type;
    /** Address of the sender. */
    uint8_t source;
    /** Address of the receiver. */
    uint8_t destination;
    /** ID assigned by the gateway and echoed in DATA, ERROR, and ACK. */
    uint8_t transaction_id;
    /** Number of payload bytes currently in use. */
    uint8_t payload_length;
    /** Payload storage; only the first payload_length bytes are meaningful. */
    uint8_t payload[LORA_PACKET_MAX_PAYLOAD_LENGTH];
    /** Response sequence, serialized as a 16-bit big-endian value. */
    uint16_t sequence;
} LORA_PACKET_Message;

/**
 * Validate a message and encode it as a V1 frame:
 * Type | Src | Dest | ID | Len | Payload | Seq | CRC.
 *
 * @param message In-memory packet to encode.
 * @param output Buffer that receives the wire bytes.
 * @param output_capacity Capacity of output in bytes.
 * @param output_length Number of encoded bytes on success.
 * @return true when a valid frame was written; false on failure.
 *
 * On failure, output and output_length remain unchanged.
 */
bool LORA_PACKET_Encode(const LORA_PACKET_Message *message,
                        uint8_t *output,
                        size_t output_capacity,
                        size_t *output_length);

/**
 * Decode a complete frame and validate its length, Type/address contract,
 * and CRC.
 *
 * @param input Wire bytes received from the radio.
 * @param input_length Total number of bytes in input.
 * @param message Destination for the decoded packet.
 * @return true when the complete frame is valid; false when it is rejected.
 *
 * On failure, message remains unchanged.
 */
bool LORA_PACKET_Decode(const uint8_t *input,
                        size_t input_length,
                        LORA_PACKET_Message *message);

#ifdef __cplusplus
}
#endif

#endif /* NODE_PACKET_H */
