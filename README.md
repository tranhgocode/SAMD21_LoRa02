# LoRa-02 (SX1278) với SAM D21 Curiosity Nano

Project thử nghiệm truyền và nhận LoRa ở tần số **433 MHz** bằng module
Ai-Thinker LoRa-02 (SX1278) và vi điều khiển ATSAMD21G17D.

## GPIO mapping

| Tín hiệu LoRa-02 | GPIO SAM D21 | Chức năng | Hướng nhìn từ SAM D21 | Trạng thái ban đầu |
|---|---:|---|---|---|
| `MOSI` | `PA16` | `SERCOM1_PAD0` | Output | SPI data từ MCU đến LoRa-02 |
| `SCK` | `PA17` | `SERCOM1_PAD1` | Output | SPI clock |
| `MISO` | `PA19` | `SERCOM1_PAD3` | Input | SPI data từ LoRa-02 về MCU |
| `NSS` / `CS` | `PA18` | GPIO output | Output | High, không chọn module |
| `RESET` | `PA10` | GPIO output | Output | High, reset tích cực mức thấp |
| `DIO0` | `PA11` | GPIO input | Input | Báo `TxDone` hoặc `RxDone` |
| `VCC` | `3.3V` | Nguồn | — | Chỉ sử dụng 3.3 V |
| `GND` | `GND` | Mass | — | Nối chung GND với board |

Các chân `DIO1` đến `DIO5` của LoRa-02 không được sử dụng trong bài test này.

### Sơ đồ nối nhanh

```text
SAM D21 Curiosity Nano                  LoRa-02 / SX1278

3.3V  --------------------------------> VCC
GND   --------------------------------> GND
PA16  --------------------------------> MOSI
PA17  --------------------------------> SCK
PA19  <-------------------------------- MISO
PA18  --------------------------------> NSS / CS
PA10  --------------------------------> RESET
PA11  <-------------------------------- DIO0
```

## Cấu hình giao tiếp

- SPI: `SERCOM1`, master, Mode 0, MSB first, 1 MHz.
- UART debug: `SERCOM5`, 115200 baud.
- UART TX: `PA22`.
- UART RX: `PB22`.
- LoRa: 433 MHz, SF7, bandwidth 125 kHz, coding rate 4/5, CRC bật.
- Công suất phát: 17 dBm.

## Chọn chế độ TX hoặc RX

Chọn vai trò trong `src/app/lora_app.h`:

```c
#define LORA_APP_ROLE_RECEIVER     0U
#define LORA_APP_ROLE_TRANSMITTER  1U
```

Bộ phát:

```c
#define LORA_APP_ROLE LORA_APP_ROLE_TRANSMITTER
```

Bộ nhận:

```c
#define LORA_APP_ROLE LORA_APP_ROLE_RECEIVER
```

Cần hai board và hai module LoRa-02 để kiểm tra đường truyền: một board nạp
firmware TX, board còn lại nạp firmware RX. Hai phía phải dùng cùng cấu hình
tần số, spreading factor, bandwidth, coding rate, CRC và sync word.

## Kết quả UART dự kiến

Bên phát:

```text
LoRa-02 test: 433 MHz, SF7, BW125, CR4/5, CRC on
Role: TRANSMITTER - sending one packet every 2 seconds.
TX OK: Hello LoRa #0
```

Bên nhận:

```text
LoRa-02 test: 433 MHz, SF7, BW125, CR4/5, CRC on
Role: RECEIVER - waiting for packets.
RX OK: Hello LoRa #0
```

Nếu SPI không giao tiếp được với SX1278, UART sẽ hiển thị:

```text
ERROR: SX1278 not found (check 3.3 V, GND, SPI and NSS).
```

## Lưu ý phần cứng

- Không cấp 5 V vào LoRa-02 hoặc các chân GPIO của SAM D21.
- Gắn antenna 433 MHz trước khi bật chế độ phát.
- Dùng dây SPI ngắn và nối GND chắc chắn.
- Nguồn 3.3 V phải đáp ứng được dòng phát của module; đặt tụ decoupling gần
  chân nguồn LoRa-02 nếu nguồn bị sụt áp hoặc module hoạt động không ổn định.
- `NSS`, `RESET` và `DIO0` phải đúng với mapping trên. Nếu thay đổi trong MPLAB
  Code Configurator, cần generate lại cấu hình và cập nhật lớp `SX1278_hw`.
