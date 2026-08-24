/*
 * File:   DHT11.h
 * Author: Lap4all
 *
 * Created on May 28, 2026, 6:41 PM
 */

#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include <stdbool.h>
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DEFINE BEGIN */
#define DHT11_START_LOW_US      18000U     /**< HOST pulls DATA LOW for at least 18 ms       */
#define DHT11_START_HIGH_US     30U        /**< HOST releases DATA HIGH for about 20~40 us   */
#define DHT11_RESPONSE_WAIT_US  100U       /**< Timeout waiting for DHT11 to pull DATA LOW   */
#define DHT11_BIT_THRESHOLD_US  40U        /**< HIGH-level threshold to distinguish bit 0/1  */
#define DHT11_TOTAL_BITS        40U        /**< Total data bits: 5 bytes x 8 bits            */
#define DHT11_DEFAULT_TIMEOUT   200U       /**< Timeout for each signal wait, in us          */
#define DHT11_MIN_INTERVAL_MS   1500U      /**< Recommended interval between two reads       */
/* DEFINE END */

/* TYPEDEF BEGIN */
typedef enum {
    DHT11_PIN_INPUT  = 0,
    DHT11_PIN_OUTPUT = 1,
} DHT11_PinModeTypeDef;

/**
 * @brief Structure storing data read from the DHT11.
 */
typedef struct {
    uint8_t temperature_int;    /**< Integer part of temperature  */
    uint8_t temperature_dec;    /**< Decimal part of temperature  */
    uint8_t checksum;           /**< Checksum from DHT11          */

    float   temperature;        /**< Temperature, in degrees C    */

    bool    is_valid;           /**< true if checksum is valid    */
} DHT11_DataTypeDef;

/**
 * @brief Handle for managing one DHT11 sensor.
 */
typedef struct {
    PORT_PIN            pin;                /**< PORT pin connected to DHT11 DATA          */
    DHT11_DataTypeDef   data;               /**< Data from the most recent read            */
    uint32_t            timeout_us;         /**< Timeout when waiting for signal level, us */
} DHT11_HandleTypeDef;
/* TYPEDEF END */

/* FUNCTION PROTOTYPES BEGIN */
/**
 * @brief Initialize a DHT11 handle.
 *
 * @return true if initialization succeeds, false if handle is NULL.
 */
bool DHT11_Init(DHT11_HandleTypeDef *handle, PORT_PIN pin, uint32_t timeout_us);

/**
 * @brief Read temperature from the DHT11.
 * @return true if the read succeeds and checksum is valid, false on NULL/timeout/no response/checksum error.
 * @note Call this function at least DHT11_MIN_INTERVAL_MS apart.
 *       The library returns true/false and no longer returns detailed error codes.
 */
bool DHT11_Read(DHT11_HandleTypeDef *handle);

/**
 * @brief Get temperature from the most recent valid read.
 * @return true if the value is available, false if pointer is NULL or data is not valid yet.
 */
bool DHT11_GetTemperature(const DHT11_HandleTypeDef *handle, float *temperature);

/**
 * @brief Reset data in the handle while keeping pin and timeout unchanged.
 * @return true if reset succeeds, false if handle is NULL.
 */
bool DHT11_Reset(DHT11_HandleTypeDef *handle);

/**
 * @brief Check whether data in the handle is valid.
 */
bool DHT11_IsDataValid(const DHT11_HandleTypeDef *handle);

/* FUNCTION PROTOTYPES END */

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
