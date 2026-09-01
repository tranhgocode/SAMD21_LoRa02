# LoRa-02 (SX1278) with SAM D21

This project demonstrates a 433 MHz LoRa link using an Ai-Thinker LoRa-02
(SX1278) module and an ATSAMD21G17D microcontroller. The transmitter reads
temperature and relative humidity from a DHT11 sensor, sends the values every
two seconds, and reports its state through the debug UART. The same application
can also be built as a LoRa receiver.

## Hardware

- SAM D21 Curiosity Nano board with an ATSAMD21G17D
- Ai-Thinker LoRa-02 (SX1278) 433 MHz module
- 433 MHz antenna
- DHT11 temperature and humidity sensor for transmitter mode
- Stable 3.3 V supply

## LoRa-02 pin connections

The arrows show the signal direction relative to the SAM D21.

| SAM D21 pin | LoRa-02 pin | Purpose                         |
| ----------- | ----------- | ------------------------------- |
| `3.3V`      | `VCC`       | 3.3 V power supply              |
| `GND`       | `GND`       | Common ground                   |
| `PA16`      | `MOSI`      | SPI data from the MCU           |
| `PA17`      | `SCK`       | SPI clock                       |
| `PA19`      | `MISO`      | SPI data from the LoRa-02       |
| `PA18`      | `NSS / CS`  | Active-low SPI chip select      |
| `PA10`      | `RESET`     | Active-low hardware reset       |
| `PA11`      | `DIO0`      | `TxDone` or `RxDone` indication |

The LoRa-02 `DIO1` through `DIO5` pins are not used by this application

## DHT11 connection

The DHT11 data line is configured on `PA00`. Power the sensor from 3.3 V and
connect its ground to the board ground. Add a pull-up resistor on the data line
if the DHT11 module does not already include one.

| SAM D21 pin | DHT11 pin | Purpose                 |
| ----------- | --------- | ----------------------- |
| `3.3V`      | `VCC`     | Sensor power            |
| `GND`       | `GND`     | Common ground           |
| `PA00`      | `DATA`    | Single-wire sensor data |

## Interface configuration

- LoRa carrier frequency: 433 MHz
- Spreading factor: SF7
- Signal bandwidth: 125 kHz
- Coding rate: 4/5
- Payload CRC: enabled
- Transmit power setting: 17 dBm
- SPI: `SERCOM1`, master mode, Mode 0, MSB first, 1 MHz
- Debug UART: `SERCOM5`, 115200 baud, 8-N-1 (transmit only)
- UART TX: `PA22`
- `PB22` is assigned to the SERCOM5 RX function in the pin configuration, but
  this application does not enable or use UART reception

## Selecting transmitter or receiver mode

Set `LORA_APP_ROLE` in `src/app/lora_app.h` before building the firmware.

For the transmitter:

```c
#define LORA_APP_ROLE LORA_APP_ROLE_TRANSMITTER
```

For the receiver:

```c
#define LORA_APP_ROLE LORA_APP_ROLE_RECEIVER
```

A complete link test requires two boards and two LoRa-02 modules. Flash one
board as the transmitter and the other as the receiver. Both radios must use
the same frequency, spreading factor, bandwidth, coding rate, CRC setting, and
sync word.

## Building and running

1. Open `lora_TX.X` in MPLAB X IDE.
2. Select the installed XC32 compiler and build the `default` configuration.
3. Connect the hardware according to the tables above.
4. Attach a 433 MHz antenna before enabling the transmitter.
5. Program the board and open the debug UART at 115200 baud, 8 data bits, no
   parity, and 1 stop bit.

## UART log format

Every complete UART log line begins with a lowercase category prefix:

- `status: ` reports normal operation.
- `error: ` reports an initialization, sensor, timeout, or communication error.

Example transmitter output:

```text
status: LoRa-02 configuration: 433 MHz, SF7, BW125, CR4/5, CRC enabled
status: transmitter ready; sending DHT11 data every 2 seconds
status: sent: T=25.0C H=60.0%
```

Example receiver output:

```text
status: LoRa-02 configuration: 433 MHz, SF7, BW125, CR4/5, CRC enabled
status: receiver ready; waiting for LoRa packets
status: received: T=25.0C H=60.0%
```

If the radio cannot be detected, the UART reports:

```text
error: SX1278 was not detected; check power and SPI wiring
```

## Hardware notes

- Never apply 5 V to the LoRa-02 or to a SAM D21 GPIO pin.
- Attach a 433 MHz antenna before transmitting.
- Keep SPI wires short and use a reliable common-ground connection.
- Ensure that the 3.3 V supply can provide the radio's peak transmit current.
  Place a decoupling capacitor close to the LoRa-02 if the supply is unstable.
- If the pin mapping is changed in MPLAB Code Configurator, regenerate the
  Harmony configuration and update the SX1278 hardware abstraction layer.
