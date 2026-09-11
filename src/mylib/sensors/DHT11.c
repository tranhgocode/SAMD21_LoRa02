
#include "DHT11.h"


/*PRIVATE MACROS BEGIN*/
/** Set DATA pin as OUTPUT and drive it HIGH (idle state) */
#define DHT11_PIN_SET_OUTPUT_HIGH(pin)                      \
    do {                                                    \
        PORT_PinOutputEnable((pin));   /* Switch to OUTPUT */  \
        PORT_PinSet((pin));            /* Drive HIGH       */  \
    } while(0)

/** Set DATA pin as OUTPUT and drive it LOW */
#define DHT11_PIN_SET_OUTPUT_LOW(pin)                       \
    do {                                                    \
        PORT_PinOutputEnable((pin));   /* Switch to OUTPUT */  \
        PORT_PinClear((pin));          /* Drive LOW        */  \
    } while(0)

/** Switch DATA pin to INPUT (release the line for DHT11 control) */
#define DHT11_PIN_SET_INPUT(pin)                            \
    do {                                                    \
        PORT_PinInputEnable((pin));    /* Switch to INPUT   */  \
    } while(0)

#define DHT11_PIN_READ(pin)     PORT_PinRead((pin))     /** Read the current logic level of DATA pin */
/*PRIVATE MACROS END*/

/*PRIVATE FUNCTION BEGIN*/
static void     DHT11_SendStartSignal(DHT11_HandleTypeDef *handle);
static bool     DHT11_WaitResponse(DHT11_HandleTypeDef *handle);
static bool     DHT11_ReadBit(DHT11_HandleTypeDef *handle, uint8_t *bit);
static bool     DHT11_ReadAllBits(DHT11_HandleTypeDef *handle, uint8_t *raw_data);
static bool     DHT11_VerifyChecksum(const uint8_t *raw_data);
static void     DHT11_ParseData(DHT11_HandleTypeDef *handle, const uint8_t *raw_data);
static void     DHT11_DelayUs(uint32_t usec);
static void     DHT11_DelayMs(uint32_t msec);
/*PRIVATE FUNCTION END*/

static void DHT11_DelayUs(uint32_t usec)
{
    if (usec == 0U)
    {
        return;
    }

    SysTick->LOAD = (CPU_CLOCK_FREQUENCY / 1000000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (usec > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
            /* Wait for one microsecond. */
        }
        usec--;
    }

    SysTick->CTRL = 0U;
}

static void DHT11_DelayMs(uint32_t msec)
{
    if (msec == 0U)
    {
        return;
    }

    SysTick->LOAD = (CPU_CLOCK_FREQUENCY / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (msec > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
            /* Wait for one millisecond. */
        }
        msec--;
    }

    SysTick->CTRL = 0U;
}


/**
 * @brief   Send the Start signal to wake up the DHT11.
 * @param[in]   handle  Pointer to the DHT11 handle.
 * @details Start signal sequence:
 *          1. Pull DATA LOW for at least 18 ms (using DHT11_START_LOW_US).
 *          2. Release DATA HIGH and wait for DHT11 response (20~40 us).
 *          3. Switch DATA to INPUT to receive signals from DHT11.
 */
static void DHT11_SendStartSignal(DHT11_HandleTypeDef *handle)
{
    DHT11_PIN_SET_OUTPUT_LOW(handle->pin);          /* Step 1: Pull DATA LOW to signal start            */
    DHT11_DelayMs(DHT11_START_LOW_US / 1000U);      /* Hold DATA LOW for at least 18 ms */

    PORT_PinSet(handle->pin);                       /* Set latch HIGH to enable pull-up when switching to INPUT */
    DHT11_PIN_SET_INPUT(handle->pin);               /* Step 2: Release bus so DHT11 can pull DATA LOW           */
    DHT11_DelayUs(DHT11_START_HIGH_US);             /* Wait 30 us for DHT11 to prepare its response             */
}


/**
 * @brief   Wait for and validate the response signal from DHT11.
 * @param[in]   handle  Pointer to the DHT11 handle.
 * @return  true   DHT11 responds with the expected timing.
 * @return  false  DHT11 does not respond or a timeout occurs.
 * @details DHT11 responds by:
 *          1. Pulling DATA LOW for about 80 us.
 *          2. Pulling DATA HIGH for about 80 us.
 *          Then it starts transmitting 40 data bits.
 */
static bool DHT11_WaitResponse(DHT11_HandleTypeDef *handle)
{
    uint32_t timeout_cnt = 0;                       /* Timeout counter, unit: microseconds */

    /* 1. Wait for DHT11 to pull DATA LOW (response) */
    timeout_cnt = 0;
    while (DHT11_PIN_READ(handle->pin) == 1)        /* Wait for DATA to change from HIGH to LOW */
    {
        DHT11_DelayUs(1U);                          /* Wait in 1 us steps */
        timeout_cnt++;
        if (timeout_cnt >= DHT11_RESPONSE_WAIT_US)  /* More than 100 us without LOW means error */
        {
            return false;                           /* DHT11 does not respond */
        }
    }

    /* 2. Wait for DHT11 to release DATA HIGH after about 80 us LOW */
    timeout_cnt = 0;
    while (DHT11_PIN_READ(handle->pin) == 0)        /* Wait for DATA to change from LOW to HIGH */
    {
        DHT11_DelayUs(1U);
        timeout_cnt++;
        if (timeout_cnt >= handle->timeout_us)      /* Timeout means error */
        {
            return false;
        }
    }

    /* 3: Wait for DHT11 to pull DATA LOW to start the first bit --- */
    timeout_cnt = 0;
    while (DHT11_PIN_READ(handle->pin) == 1)        /* Wait for the 80 us HIGH response to finish    */
    {
        DHT11_DelayUs(1U);
        timeout_cnt++;
        if (timeout_cnt >= handle->timeout_us)
        {
            return false;
        }
    }

    return true;                                    /* DHT11 is ready to transmit data              */
}

/**
 * @brief   Read one data bit from DHT11.
 *
 * @param[in]   handle  Pointer to the DHT11 handle.
 * @param[out]  bit     Pointer storing the read bit value (0 or 1).
 * @return  true   Bit read successfully.
 * @return  false  Timeout while waiting for signal.
 * @details Each bit is encoded by HIGH duration after a LOW pulse of about 50 us:
 *          - HIGH about 26-28 us means bit 0.
 *          - HIGH about 70 us means bit 1.
 *          Distinguishing threshold: DHT11_BIT_THRESHOLD_US (40 us).
 */
static bool DHT11_ReadBit(DHT11_HandleTypeDef *handle, uint8_t *bit)
{
    uint32_t timeout_cnt = 0; /* Timeout counter, approximately in microseconds */

    /* 1. Wait for the LOW pulse of about 50 us to end (start of each bit) */
    timeout_cnt = 0;
    while (DHT11_PIN_READ(handle->pin) == 0)        /* Wait for DATA to change from LOW to HIGH */
    {
        DHT11_DelayUs(1U);
        timeout_cnt++;
        if (timeout_cnt >= handle->timeout_us)      /* Timeout means transmission error */
        {
            return false;
        }
    }

    /*
     * 2. After the rising edge, wait past the bit '0' window (~26-28 us),
     *    then read DATA. If DATA is still HIGH, it is bit '1'; if LOW, it is bit '0'.
     */
    DHT11_DelayUs(DHT11_BIT_THRESHOLD_US);
    *bit = (DHT11_PIN_READ(handle->pin) == 1U) ? 1U : 0U;

    /* 3. Wait for the HIGH pulse to end before reading the next bit */
    timeout_cnt = 0;
    while (DHT11_PIN_READ(handle->pin) == 1)        /* Count HIGH time until DATA returns LOW */
    {
        DHT11_DelayUs(1U);
        timeout_cnt++;
        if (timeout_cnt >= handle->timeout_us)      /* Timeout means error */
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief   Read all 40 data bits (5 bytes) from DHT11.
 * @param[in]   handle      Pointer to the DHT11 handle.
 * @param[out]  raw_data    5-byte array storing raw data from the sensor.
 * @return  true   All 40 bits were read successfully.
 * @return  false  Timeout while reading any bit.
 */
static bool DHT11_ReadAllBits(DHT11_HandleTypeDef *handle, uint8_t *raw_data)
{
    bool     ret    = true;     /* Return value from each ReadBit call        */
    uint8_t  bit    = 0;        /* Bit value just read (0 or 1)               */
    uint8_t  i      = 0;        /* Bit loop counter: 0 to 39                  */
    uint8_t  byte_i = 0;        /* Current byte index (0-4): i / 8            */
    uint8_t  bit_i  = 0;        /* Bit position in byte (7 to 0, MSB first)   */

    /* Clear old result before writing new data */
    for (i = 0; i < 5U; i++)
    {
        raw_data[i] = 0x00U;
    }

    /* Read 40 bits sequentially, 8 bits per byte, MSB first (bit 7 to bit 0) */
    for (i = 0; i < DHT11_TOTAL_BITS; i++)
    {
        ret = DHT11_ReadBit(handle, &bit);          /* Read 1 bit from DHT11 */
        if (!ret)
        {
            return ret;                             /* Stop immediately on error */
        }

        byte_i = i / 8U;                            /* Determine byte index (0-4) */
        bit_i  = 7U - (i % 8U);                     /* MSB first */

        /* Write the bit to the correct position in the byte */
        if (bit == 1U)
        {
            raw_data[byte_i] |= (uint8_t)(1U << bit_i); /* Set bit */
        }
        /* If bit == 0, no action is needed because the buffer was cleared */
    }

    return true;
}

/**
 * @brief   Verify checksum of the received 5 data bytes.
 * @param[in]   raw_data    5-byte raw data array from DHT11.
 * @return  true    Checksum is valid: raw_data[4] == (raw_data[0]+[1]+[2]+[3]) & 0xFF.
 * @return  false   Checksum is invalid, data is not reliable.
 */
static bool DHT11_VerifyChecksum(const uint8_t *raw_data)
{
    /* Sum the first 4 bytes and keep only the lower 8 bits (mod 256) */
    uint8_t sum = (uint8_t)((uint16_t)raw_data[0]    /* Sensor byte 0       */
                           + (uint16_t)raw_data[1]   /* Sensor byte 1       */
                           + (uint16_t)raw_data[2]   /* Temperature Integer */
                           + (uint16_t)raw_data[3]); /* Temperature Decimal */

    return (sum == raw_data[4]); /* Compare with checksum byte */
}

/**
 * @brief   Parse raw data and update handle->data.
 * @param[in,out]   handle      DHT11 handle whose data will be updated.
 * @param[in]       raw_data    5-byte raw data array read from DHT11.
 * @details Assign raw bytes to the correct fields, calculate float values,
 *          and set is_valid = true if checksum is correct.
 */
static void DHT11_ParseData(DHT11_HandleTypeDef *handle, const uint8_t *raw_data)
{
    /* Store humidity bytes into the data structure */
    handle->data.humidity_int    = raw_data[0];     /* Byte 0: Integer part of humidity        */
    handle->data.humidity_dec    = raw_data[1];     /* Byte 1: Decimal part of humidity        */

    /* Store temperature bytes into the data structure */
    handle->data.temperature_int = raw_data[2];     /* Byte 2: Integer part of temperature     */
    handle->data.temperature_dec = raw_data[3];     /* Byte 3: Decimal part of temperature     */
    handle->data.checksum        = raw_data[4];     /* Byte 4: Checksum                        */

    /* Verify checksum before calculating float values */
    handle->data.is_valid = DHT11_VerifyChecksum(raw_data);

    if (handle->data.is_valid)
    {
        /* Calculate values: integer part + decimal part / 10 */
        handle->data.humidity = (float)raw_data[0] + ((float)raw_data[1] / 10.0f); /* %RH */
        handle->data.temperature = (float)raw_data[2] + ((float)raw_data[3] / 10.0f); /* deg C */
    }
    else
    {
        /* Invalid checksum: keep float values at 0.0f to avoid stale data use */
        handle->data.humidity    = 0.0f;
        handle->data.temperature = 0.0f;
    }
}

/**
 * @brief   Initialize a DHT11 handle.
 */
bool DHT11_Init(DHT11_HandleTypeDef *handle, PORT_PIN pin, uint32_t timeout_us)
{
    if (handle == NULL) /* Check NULL pointer first                  */
    {
        return false;
    }
    /* Initialize configuration fields */
    handle->pin                  = pin; /* Store GPIO pin */
    handle->timeout_us           = (timeout_us > 0U) ? timeout_us : DHT11_DEFAULT_TIMEOUT; /* Use default value if timeout = 0 */

    /* Clear the data structure */
    handle->data.humidity_int    = 0U;
    handle->data.humidity_dec    = 0U;
    handle->data.temperature_int = 0U;
    handle->data.temperature_dec = 0U;
    handle->data.checksum        = 0U;
    handle->data.humidity        = 0.0f;
    handle->data.temperature     = 0.0f;
    handle->data.is_valid        = false;

    /* Set DATA to idle state: OUTPUT HIGH (bus ready) */
    DHT11_PIN_SET_OUTPUT_HIGH(handle->pin);

    return true;
}

/**
 * @brief   Read data from DHT11.
 */
bool DHT11_Read(DHT11_HandleTypeDef *handle)
{
    bool                ret              = true;       /* Result of read steps     */
    uint8_t             raw_data[5U]     = {0U};       /* 5-byte raw data buffer   */

    /* 1. Check NULL pointer --- */
    if (handle == NULL)
    {
        return false;
    }

    /* 2. Caller must ensure DHT11_MIN_INTERVAL_MS between calls */

    /* 3. Send Start signal */
    DHT11_SendStartSignal(handle);   /* Pull LOW 18 ms, HIGH 30 us, then INPUT */

    /* 4. Wait for DHT11 response */
    ret = DHT11_WaitResponse(handle);
    if (!ret)
    {
        DHT11_PIN_SET_OUTPUT_HIGH(handle->pin);  /* Return bus to idle state before exit */
        return ret;
    }

    /* 5. Read 40 data bits */
    ret = DHT11_ReadAllBits(handle, raw_data);
    if (!ret)
    {
        DHT11_PIN_SET_OUTPUT_HIGH(handle->pin);  /* Return bus to idle state */
        return ret;
    }

    /* 6. Return bus to idle state */
    DHT11_PIN_SET_OUTPUT_HIGH(handle->pin); /* DATA to OUTPUT HIGH (bus idle) */

    /* 7. Parse data and verify checksum */
    DHT11_ParseData(handle, raw_data); /* Assign raw bytes, calculate float values, check checksum */

    /* 8. Return result if checksum is valid */
    if (handle->data.is_valid)
    {
        return true;
    }

    return false; /* Checksum false */
}

/**
 * @brief   Get temperature value from handle.
 */
bool DHT11_GetTemperature(const DHT11_HandleTypeDef *handle, float *temperature)
{
    if ((handle == NULL) || (temperature == NULL))  /* Check both pointers */
    {
        return false;
    }

    if (!handle->data.is_valid)                     /* Do not return value if checksum is invalid */
    {
        return false;
    }

    *temperature = handle->data.temperature;        /* Copy value to output */
    return true;
}

/**
 * @brief   Get relative humidity value from handle.
 */
bool DHT11_GetHumidity(const DHT11_HandleTypeDef *handle, float *humidity)
{
    if ((handle == NULL) || (humidity == NULL))
    {
        return false;
    }

    if (!handle->data.is_valid)
    {
        return false;
    }

    *humidity = handle->data.humidity;
    return true;
}

/**
 * @brief   Reset handle state.
 */
bool DHT11_Reset(DHT11_HandleTypeDef *handle)
{
    if (handle == NULL)
    {
        return false;
    }

    /* Clear only data, keeping pin and timeout unchanged */
    handle->data.humidity_int    = 0U;
    handle->data.humidity_dec    = 0U;
    handle->data.temperature_int = 0U;
    handle->data.temperature_dec = 0U;
    handle->data.checksum        = 0U;
    handle->data.humidity        = 0.0f;
    handle->data.temperature     = 0.0f;
    handle->data.is_valid        = false;

    return true;
}

/**
 * @brief   Check whether data is valid.
 */
bool DHT11_IsDataValid(const DHT11_HandleTypeDef *handle)
{
    if (handle == NULL)
    {
        return false; /* NULL pointer means invalid */
    }
    return handle->data.is_valid;
}
