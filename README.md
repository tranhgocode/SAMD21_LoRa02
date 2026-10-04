# LoRa V1 Sensor Node

## Overview

This repository contains firmware for a continuously powered LoRa sensor node
built with an ATSAMD21G17D, an Ai-Thinker LoRa-02/SX1278, and a DHT11 sensor.
After initialization, the node waits 60 seconds, reads DHT11 once, sends one
`DATA` or `ERROR` packet, immediately arms RX, and waits for a matching `ACK`.
It ends the cycle on ACK, ACK timeout, or TX failure, then waits another
60 seconds from the end of the cycle. There is no application retry.

This is **MVP2: continuously powered autonomous uplink**. It does not enter MCU
sleep or use RTC wakeup; those belong to MVP3. The legacy `POLL` packet remains
in the wire codec but no longer triggers sampling. A gateway must accept
unsolicited `DATA`/`ERROR` and echo the node's ID/Seq in its ACK; a gateway that
only polls nodes must be updated separately.

- ACK timeout is 1000 ms from successful blocking TX completion; an ACK
  observed at exactly 1000 ms is late. Wrong frames never extend the deadline.
- TX uses the existing driver's bounded 3000 ms timeout argument.
- The node creates an 8-bit ID and a 16-bit Seq in RAM, initially zero. Both
  advance once per finished cycle, including TX failure, and wrap naturally.
  Reset restarts them; duplicate detection across resets is outside this MVP.
- DATA/ERROR payloads, byte order and CRC-16/CCITT-FALSE remain unchanged.
- Sensor initialization/read/value failure sends one ERROR and still waits
  for ACK. A local build failure ends the cycle without transmitting.

## Note: Using More Than One Node

Each physical node must use a unique address. Before building firmware for a
node, change `LORA_APP_NODE_ADDRESS` in `src/app/lora_app.c`:

```c
/* Node 1 */
#define LORA_APP_NODE_ADDRESS 0x01U

/* Node 2 */
#define LORA_APP_NODE_ADDRESS 0x02U
```

Build and flash each board separately after selecting its address. Valid node
addresses are `0x01` through `0xFE`, `0x00` belongs to the gateway and `0xFF`
is reserved. The gateway must set `Dest` to the intended node address in each
`ACK`. Rebuilding overwrites the default `.hex` output, so copy or
rename each image, for example `node_01.hex` and `node_02.hex`, before building
the next node.

## Architecture

Hardware access is kept in the integration and driver layers. The state,
response, packet, and CRC modules are pure C and can be tested on a host PC.

```mermaid
flowchart TD
    MAIN[main.c] --> APP[lora_app]
    APP --> STATE[node_state]
    APP --> RESPONSE[node_response]
    STATE --> PACKET[node_packet]
    RESPONSE --> PACKET
    PACKET --> CRC[crc16]
    APP --> DHT[DHT11 driver]
    APP --> RADIO[SX1278 driver]
    APP --> UART[SERCOM5 UART]
    RADIO --> PLATFORM[SAMD21 SPI and GPIO]
```

| Path | Responsibility |
| --- | --- |
| `src/main.c` | Initializes Harmony and repeatedly runs the app and `SYS_Tasks()` |
| `src/app/lora_app.*` | Integrates radio RX/TX, DHT11, UART logging, and timer ticks |
| `src/app/app_time.h` | Reserves TC4/TC5 for a free-running elapsed-time counter |
| `src/app/node_state.*` | Controls `WAIT_INTERVAL`, `WAIT_TX_RESULT`, and `WAIT_ACK` |
| `src/app/node_response.*` | Builds `DATA` or `ERROR` responses |
| `src/protocol/node_packet.*` | Validates, encodes, and decodes V1 packets |
| `src/protocol/crc16.*` | Calculates CRC-16/CCITT-FALSE |
| `src/drivers/` | Contains DHT11, SX1278, and SAM D21 hardware access |
| `src/config/default/` | Contains generated MPLAB Harmony configuration |
| `lora_TX.X/` | Contains the MPLAB X project |

## Packet Flow

```mermaid
sequenceDiagram
    participant G as Gateway
    participant N as Sensor Node
    participant D as DHT11

    N->>N: Wait 60 seconds; create ID and Seq
    N->>D: Read temperature and humidity once
    alt Valid sensor sample
        N-->>G: DATA (node ID, current Seq)
    else Sensor failure
        N-->>G: ERROR (node ID, current Seq)
    end
    N->>N: Arm RX immediately after TX completion
    alt Matching ACK before 1000 ms
        G->>N: ACK (same ID and Seq)
        N->>N: Finish cycle; ID++, Seq++, WAIT_INTERVAL
    else TX failure or ACK timeout
        N->>N: Log locally; ID++, Seq++, WAIT_INTERVAL
    end
```

All packets use this wire layout:

```text
Type | Src | Dest | ID | Len | Payload | Seq | CRC
 1 B   1 B   1 B   1 B   1 B    Len B    2 B   2 B
```

Multi-byte fields are big-endian. The gateway address is `0x00`, the current
node address is `0x02`, and the maximum packet length is 64 bytes. An invalid
length, Type, address, direction, or CRC causes the packet to be ignored.

## Elapsed Time

`src/app/app_time.h` reserves the unused **TC4/TC5 pair** as a 32-bit counter
clocked by GCLK0 (48 MHz) with a /1024 prescaler. Continuous synchronized COUNT
reads and fractional millisecond accumulation count sensor, TX and UART work;
loop iteration counts do not determine deadlines. SysTick remains available to
the existing DHT11/radio delay routines. The CPU and radio stay powered during
`WAIT_INTERVAL`; the application does not poll RX in that state.

The timer follows the [Microchip SAM D21/DA1 datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-D21-DA1-Family-Data-Sheet-DS40001882.pdf)
TC pairing and READREQ COUNT synchronization contract. Clock/reset/synchronization
initialization waits are bounded and failure prevents application startup.
The timebase must be sampled at least once per hardware counter wrap (about
25 hours); normal operation samples it every application iteration. Timing
accuracy depends on the existing GCLK0 oscillator configuration. Generated
Harmony configuration is unchanged. Timer/RF accuracy still requires a board test.

## Pin Table

All signals use 3.3 V logic. Signal direction is relative to the SAM D21.

| SAM D21 pin | Connected device pin | Direction | Purpose |
| --- | --- | --- | --- |
| `3.3V` | LoRa-02 `VCC` | Power | Radio supply |
| `GND` | LoRa-02 `GND` | Power | Common ground |
| `PA16` | LoRa-02 `MOSI` | Output | SERCOM1 SPI data to radio |
| `PA17` | LoRa-02 `SCK` | Output | SERCOM1 SPI clock |
| `PA19` | LoRa-02 `MISO` | Input | SERCOM1 SPI data from radio |
| `PA18` | LoRa-02 `NSS/CS` | Output | Active-low chip select |
| `PA10` | LoRa-02 `RESET` | Output | Active-low radio reset |
| `PA11` | LoRa-02 `DIO0` | Input | `RxDone` and `TxDone` indication |
| `3.3V` | DHT11 `VCC` | Power | Sensor supply |
| `GND` | DHT11 `GND` | Power | Common ground |
| `PA00` | DHT11 `DATA` | Bidirectional | Single-wire sensor data |
| `PA22` | UART adapter RX | Output | SERCOM5 debug UART TX |

Use a pull-up resistor on DHT11 DATA if the sensor module does not include one.
Attach a 433 MHz antenna before transmitting, and never apply 5 V to the radio
or MCU GPIO pins.

## Build

Required toolchain:

- MPLAB X IDE v6.30
- MPLAB XC32 v5.10
- Microchip SAMD21 DFP 3.7.262.

Open `lora_TX.X` in MPLAB X and build the `default` configuration, or run the
following commands from the repository root after adding the MPLAB/XC32 tools
to `PATH`:

```powershell
make -C lora_TX.X CONF=default build
make -C lora_TX.X CONF=default clean
```

The production image is generated at:

```text
lora_TX.X/dist/default/production/lora_TX.X.production.hex
```

Program the board through MPLAB X and monitor SERCOM5 at 115200 baud, 8 data
bits, no parity, and 1 stop bit.

## Host Tests

With GNU make and a C11 GCC-compatible compiler on PATH, run from the root:

```powershell
make -C tests test
make -C tests/packet test
```

The suites cover packet/CRC contracts, sensor error responses, autonomous
scheduling, ACK mismatch and boundary deadlines, TX/RX failure, ID/Seq and time
wrap, and timer conversion. They compile with `-Wall -Wextra -Werror -pedantic`.
`make -C tests clean` removes only the named test executables. Tests are portable
host C; the hardware timer register setup and RF link still need board validation.

## Log Output

UART lines start with `status:` for normal events or `error:` for failures.
Binary packets are logged with an explicit length and hexadecimal bytes.
The readiness message is:

```text
status: sensor node ready, first uplink in 60 seconds
```

Each cycle logs `status: tx len=... hex=...`, then successful TX logs
`status: RX ready, waiting for ACK`. Terminal outcomes include:

```text
status: ACK accepted
status: ACK timeout
error: LoRa TX failed
```

Other diagnostics include DHT11 initialization failure, RX setup failure and
`status: RX frame ignored`. RX observations are timestamped before UART logging;
post-TX logs count toward the ACK deadline, and terminal logging finishes before
the next 60-second interval starts.

Host tests and firmware builds do not replace hardware verification. No hardware
UART trace has been captured for MVP2. A compatible gateway and two radios are
required to verify valid/wrong/missing ACKs, sensor/TX errors, actual timing and
long-running cycles. MCU sleep, RTC wakeup and power measurements remain MVP3.
