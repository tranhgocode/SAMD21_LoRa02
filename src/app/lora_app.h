/**
 * @file lora_app.h
 * @brief Simple 433 MHz LoRa transmitter/receiver test application.
 */

#ifndef LORA_APP_H
#define LORA_APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LORA_APP_ROLE_RECEIVER     0U
#define LORA_APP_ROLE_TRANSMITTER  1U

/* Flash one board with TRANSMITTER and the other with RECEIVER. */
#ifndef LORA_APP_ROLE
#define LORA_APP_ROLE LORA_APP_ROLE_TRANSMITTER
#endif

/** Configure and verify the LoRa-02. Returns false when RegVersion is invalid. */
bool LORA_APP_Initialize(void);

/** Run one iteration of the selected transmitter or receiver test. */
void LORA_APP_Tasks(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_APP_H */
