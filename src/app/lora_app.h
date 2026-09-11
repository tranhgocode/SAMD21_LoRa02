/**
 * @file lora_app.h
 * @brief Continuously powered LoRa V1 sensor-node application.
 */

#ifndef LORA_APP_H
#define LORA_APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Configure and verify the LoRa-02. Returns false when RegVersion is invalid. */
bool LORA_APP_Initialize(void);

/** Run one bounded node iteration, then return control to SYS_Tasks(). */
void LORA_APP_Tasks(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_APP_H */
