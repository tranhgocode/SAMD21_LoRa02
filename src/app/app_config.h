/** Application settings to select before building each sensor node. */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/** Node addresses: 0x01..0xFE; gateway 0x00 and reserved 0xFF are excluded. */
#define APP_CONFIG_NODE_ADDRESS       0x02U

/** RTC sleep after initialization and after each completed transaction. */
#define APP_CONFIG_SLEEP_INTERVAL_MS  5000U

/** TX timeout and ACK deadline measured from successful TX completion. */
#define APP_CONFIG_TX_TIMEOUT_MS      3000U
#define APP_CONFIG_ACK_TIMEOUT_MS     1000U

/** Radio settings must match the gateway's settings. */
#define APP_CONFIG_FREQUENCY_HZ       433000000ULL

/* Include SX1278.h where these symbolic radio settings are consumed.
 * This header avoids hardware includes so node_state remains portable.
 */
#define APP_CONFIG_RADIO_POWER       SX1278_POWER_17DBM
#define APP_CONFIG_RADIO_SF          SX1278_LORA_SF_7
#define APP_CONFIG_RADIO_BW          SX1278_LORA_BW_125KHZ
#define APP_CONFIG_RADIO_CR          SX1278_LORA_CR_4_5
#define APP_CONFIG_RADIO_CRC         SX1278_LORA_CRC_EN

/* Configure RTC/GCLK and pin mappings through MCC, not this header. */

#endif /* APP_CONFIG_H */
