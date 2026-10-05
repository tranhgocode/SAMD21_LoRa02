# Cấu hình MCC cho node SAM D21 + LoRa-02 + DHT11

Tài liệu hướng dẫn cấu hình toàn bộ clock, pin và ngoại vi đang dùng trong dự án; phần RTC/GCLK1 chuẩn bị cho MVP3 ngủ Standby và đánh thức bằng RTC. Giá trị SPI, UART, GPIO và NVMCTRL được đối chiếu với mã PLIB trên đĩa. Tên tùy chọn có thể khác nhẹ giữa các phiên bản MCC; chọn theo chức năng và kiểm tra mã sau Generate.

## 1. Công cụ và trạng thái cấu hình

| Thành phần | Phiên bản được ghi trong project |
|---|---|
| MPLAB X | 6.30 |
| XC32 | 5.10 |
| MCC | 5.6.4 |
| MCC Core | 5.8.4 |
| Harmony CSP | 3.25.2 |
| Harmony CMSIS_5 | 5.9.1-dev |
| SAMD21_DFP trong Project Properties | 3.7.262 |

Manifest của lần Generate trước ghi SAMD21_DFP 3.6.144, trong khi `nbproject/configurations.xml` chọn 3.7.262. Khi mở MCC, kiểm tra pack phù hợp với project hiện có; tránh nâng/hạ package ngoài phạm vi chỉnh clock và ngoại vi. Không nhầm version của package Harmony CMSIS_5 với pack ARM CMSIS trong Project Properties.

**Trạng thái tại lúc viết tài liệu:** cấu hình MCC đã lưu tần số RTC `1024 Hz`, GCLK1 chạy Standby và compare `61440` (`0xF000`). Tuy nhiên, mã sinh trong `src/config/default/` vẫn lấy RTC từ GCLK0 `48 MHz`, compare `0x200`. Cần **Save → Generate → kiểm tra PLIB → Clean and Build** để cấu hình lưu trong MCC có hiệu lực trong firmware.

Nguồn đối chiếu trong repository:

- Cấu hình MCC: `lora_TX.X/lora_TX_default/components/*.yml` và `lora_TX.X/lora_TX_default/mcc-config.mc4`.
- Phiên bản: `src/config/default/harmony-manifest-success.yml`, `lora_TX.X/nbproject/configurations.xml`.
- Pin: `src/config/default/pin_configurations.csv`, `peripheral/port/plib_port.c`.
- Clock/ngoại vi: các PLIB dưới `src/config/default/peripheral/`.
- Timer do ứng dụng quản lý: `src/app/app_time.h`; SysTick trong driver DHT11 và SX1278.

## 2. Mở MCC và kiểm tra Project Graph

1. Mở project `lora_TX.X` trong MPLAB X và chọn configuration `default`.
2. Bấm nút **MCC** trên thanh công cụ; chờ cấu hình đang lưu tải xong.
3. Kiểm tra Device là **ATSAMD21G17D**.
4. Trong **Project Graph**, kiểm tra có System/Core, SERCOM1, SERCOM5, RTC, PM, NVMCTRL và EVSYS. PORT/NVIC/clock được cấu hình qua System/Core và các plugin.
5. Nếu dựng lại từ đầu, thêm module còn thiếu từ **Device Resources → Harmony → Peripherals**. Nếu module đã có, chọn module đó để chỉnh; không tạo thêm instance trùng.

| Thành phần | Vai trò |
|---|---|
| System/Core, SYSCTRL, GCLK | Clock CPU và clock ngoại vi; cấu hình device |
| PORT | GPIO và ánh xạ chân SERCOM |
| SERCOM1 | SPI Master giao tiếp LoRa-02/SX1278 |
| SERCOM5 | USART gửi log debug |
| RTC | Hẹn wake cho MVP3 |
| PM | API vào Idle/Standby, đọc nguyên nhân reset |
| NVMCTRL | Wait states và hành vi Flash khi ngủ |
| NVIC | Ngắt RTC, SPI và UART |
| EVSYS | Có trong project nhưng chưa nối event channel/user |
| TC4/TC5, SysTick | Ứng dụng cấu hình trực tiếp; xem mục 10 |

Quy ước checkbox: **ô có dấu tick = bật**, **ô trống = tắt**. Bấm một lần vào ô để đổi trạng thái. Dấu `+`/`−` chỉ mở/thu nhóm tùy chọn.

## 3. Cấu hình clock

Mở **Project Graph → Plugins → Clock Configuration** để vào **Clock Easy View**. Các lựa chọn Standby thường nằm trong nút bánh răng của oscillator/generator. [Hướng dẫn giao diện Clock của Microchip](https://developerhelp.microchip.com/xwiki/bin/view/software-tools/harmony/low-power-application-on-samd21/step1/)

### 3.1. Sơ đồ clock cần đạt

```text
DFLL48M, 48 MHz, Open Loop
  └─ GCLK0, chia 1, 48 MHz
       ├─ CPU / AHB / APBA / APBB / APBC
       ├─ SERCOM1_CORE → SPI 1 MHz
       ├─ SERCOM5_CORE → UART 115200 baud
       └─ TC4/TC5 → ứng dụng chia 1024 → 46875 tick/giây

OSCULP32K, danh định 32768 Hz
  └─ GCLK1, chia 32, 1024 Hz, chạy trong Standby
       └─ RTC, DIV1 → 1024 tick/giây → compare 0xF000
```

### 3.2. Clock CPU: DFLL48M và GCLK0

Giữ cấu hình CPU hiện tại để tốc độ SPI/UART và các hàm delay không đổi.

| Mục | Giá trị |
|---|---|
| DFLL Enable | Bật |
| DFLL Mode | Open Loop, theo cấu hình hiện tại |
| DFLL On Demand | Tắt, theo mã sinh hiện tại |
| DFLL Run in Standby | Tắt |
| DFLL Calibration | Giữ cơ chế MCC nạp calibration từ device |
| GCLK0 Enable | Bật |
| GCLK0 Source | DFLL48M / DFLL |
| GCLK0 Divider | 1; MCC có thể biểu diễn không chia bằng 0 |
| GCLK0 Frequency | 48000000 Hz |
| GCLK0 Run in Standby | Tắt |
| CPU/AHB/APBA/APBB/APBC divider | DIV1 |
| GCLK output to pin | Tắt |

OSC8M và OSC32K đang không dùng trong mã clock sinh hiện tại. Với phương án OSCULP32K dưới đây, không cần thêm XOSC32K hay thạch anh ngoài. Nếu đổi nguồn DFLL hoặc clock CPU, phải kiểm tra lại delay, baud, SPI và Flash wait states.

### 3.3. GCLK1 cho RTC trong Standby

Chọn **GCLK Generator 1 / GCLK1**, đặt:

| Mục | Giá trị |
|---|---|
| Enable Generator | Bật |
| Clock Source | **OSCULP32K** |
| Division Factor | **32** |
| Division Selection / Divider Mode | Chia trực tiếp theo số nguyên; không chọn chia lũy thừa |
| Output Frequency | **1024 Hz** |
| Run in Standby / Keep running in Standby | **Bật** |
| Output to pin / Output Enable | Tắt |
| Write Lock | Tắt nếu có tùy chọn |

Kiểm tra `32768 / 32 = 1024`. Một số giao diện gọi đây là clock "1 kHz", nhưng giá trị dùng để tính thời gian là **1024 Hz**. Nếu tần số không đúng, kiểm tra nguồn và kiểu chia trước khi tiếp tục.

OSCULP32K là nguồn nội luôn chạy, kể cả Standby; bit RUNSTDBY cần kiểm tra ở **GCLK1**. Chọn nguồn bằng tên trong UI, không chép số enum từ YAML vào thanh ghi vì cách đánh số có thể khác nhau. Nguồn nội có sai số; thời gian và dòng tiêu thụ cần đo trên board. [Datasheet SAM D21/DA1, mục 17.6.1](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-D21-DA1-Family-Data-Sheet-DS40001882H.pdf#page=162)

### 3.4. Peripheral Clock Configuration

Trong Clock Easy View, bấm **Peripheral Clock Configuration**, tìm từng dòng và đặt:

| Peripheral | Enable | Generator | Tần số |
|---|---|---|---|
| RTC | Bật | **GCLK1** | **1024 Hz** |
| SERCOM1_CORE | Bật | GCLK0 | 48000000 Hz |
| SERCOM5_CORE | Bật | GCLK0 | 48000000 Hz |
| TC4/TC5 | Do `app_time.h` bật/chọn khi chạy | GCLK0 | 48000000 Hz trước prescaler |

Không chuyển SPI/UART sang GCLK1. Giữ các generator khác chưa dùng ở trạng thái tắt. Clock bus APB cần cho ngoại vi được MCC quản lý; TC4/TC5 được ứng dụng bật riêng. [Hướng dẫn chọn clock RTC của Microchip](https://developerhelp.microchip.com/xwiki/bin/view/software-tools/harmony/low-power-application-on-samd21/step2/)

## 4. Pin Configuration / PORT

Mở **Project Graph → Plugins → Pin Configuration → Pin Settings**. Chọn đúng Pin ID; số chân dưới đây là số trong CSV cấu hình MCU, không phải số chân trên header của board.

| Pin ID | Số chân | Custom Name | Function | Thiết lập / kết nối |
|---|---|---|---|---|
| PA00 | 1 | `DHT11_DATA` | GPIO | In/Out: Output và bật Input Enable; tới DATA DHT11 |
| PA10 | 15 | `LORA_RESET` | GPIO | Output, latch High; tới RESET LoRa |
| PA11 | 16 | `LORA_DIO0` | GPIO | Input, không pull-up/down; tới DIO0 LoRa |
| PA16 | 25 | `SPI_MOSI` | SERCOM1_PAD0, mux C | Tới MOSI LoRa |
| PA17 | 26 | `SPI_SCK` | SERCOM1_PAD1, mux C | Tới SCK LoRa |
| PA18 | 27 | `LORA_NSS` | GPIO | Output, latch High; tới NSS/CS LoRa |
| PA19 | 28 | `SPI_MISO` | SERCOM1_PAD3, mux C | Tới MISO LoRa |
| PA22 | 31 | `UART_TX` | SERCOM5_PAD0, mux D | Tới RX của USB-UART |
| PB22 | 37 | `UART_RX` | SERCOM5_PAD2, mux D | Đã mux; bộ thu UART hiện tắt |

Giữ Drive Strength **NORMAL**. Với chân SERCOM, chọn chức năng peripheral; trạng thái "High Impedance" trong Pin Settings không có nghĩa phải ép pin thành GPIO.

PA00 hiện được sinh với Output, Input Enable và Pull Enable; latch ban đầu Low. Khi `DHT11_Init()` chạy, driver đưa DATA lên High; khi đọc, driver đổi hướng và đặt latch High trước khi nhả bus, dùng pull-up nội. Nếu tái tạo đúng cấu hình hiện tại: bật Input Enable/Pull Enable, Output, latch Low. Pull nội của SAM D21 dùng latch để chọn hướng kéo; đây không phải một pull-down cố định trong suốt giao dịch. Dùng điện trở pull-up ngoài tới 3.3 V nếu module DHT11 chưa có, như yêu cầu đấu nối trong README.

NSS và RESET phải bắt đầu ở High. DIO0 hiện được đọc bằng polling, không cấu hình EIC. Giữ PA30/PA31 cho SWD của board, không gán lại để debug/program vẫn hoạt động.

Nguồn phần cứng: **3.3 V**, chung GND; nối antenna 433 MHz trước khi phát. UART_TX của MCU nối RX adapter, không nối TX với TX.

## 5. SERCOM1: SPI Master cho SX1278

Chọn **SERCOM1 → Configuration Options**:

| Tùy chọn | Giá trị |
|---|---|
| SERCOM Operation Mode | SPI Master |
| Clock Frequency | GCLK0, 48 MHz |
| SPI Clock Speed / Baud Rate | **1000000 Hz** |
| Character Size | 8 bit |
| Data Order | MSB first |
| Clock Polarity | SCK Low khi idle, **CPOL = 0** |
| Clock Phase | Lấy mẫu cạnh đầu, **CPHA = 0** |
| SPI Mode | Mode 0 |
| Data Out Pinout / DOPO | MOSI PAD0, SCK PAD1, **DOPO = 0** |
| Data In Pinout / DIPO | **PAD3 / DIPO = 3** |
| Receiver Enable | Bật |
| Interrupt Mode | **Bật** |
| Hardware Slave Select / MSSEN, nếu có | Tắt; CS dùng GPIO PA18 |
| Run in Standby | Tắt |
| DMA | Không bật cho cấu hình hiện tại |

Tốc độ kiểm tra từ mã sinh: `48000000 / (2 × (23 + 1)) = 1000000 Hz`; `SERCOM1_SPIM_BAUD_VALUE` hiện bằng **23**.

Driver dùng `SERCOM1_SPI_WriteRead()` và chờ `SERCOM1_SPI_IsBusy()`. Không tắt ngắt SERCOM1 khi đang truyền vì ISR hoàn tất giao dịch. NSS được `SX1278_hw.c` điều khiển thủ công. [Microchip: cấu hình SPI Master, CPOL/CPHA và DIPO/DOPO](https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/sam/samd21-mcu-overview/peripherals/sercom-spi-master/configuration/)

## 6. SERCOM5: USART debug

Chọn **SERCOM5 → Configuration Options**:

| Tùy chọn | Giá trị |
|---|---|
| Operation Mode | USART, Internal Clock / asynchronous |
| Clock | GCLK0, 48 MHz |
| Baud Rate yêu cầu | **115200** |
| Character Size | 8 bit |
| Parity | None |
| Stop Bits | 1 |
| Data Order | LSB first |
| Transmitter Enable | **Bật** |
| Receiver Enable | **Tắt**, theo firmware hiện tại |
| Transmit Pinout / TXPO | PAD0 / TXPO = 0 |
| Receive Pinout / RXPO | PAD2 / RXPO = 2; chưa dùng vì RX tắt |
| Interrupt Mode | **Bật** |
| Ring Buffer Mode | Tắt |
| Sampling | 16×; mã hiện tại dùng SAMPR = 1 |
| Fractional Baud, nếu UI có | Bật, đối chiếu SAMPR và giá trị sinh |
| Immediate Buffer Overflow Notification / IBON | Bật, theo mã hiện tại |
| Run in Standby | Tắt |
| DMA / LIN / Smart Card | Không bật |

Mã sinh hiện dùng `SERCOM5_USART_INT_BAUD_VALUE = 26`. Nhập tốc độ 115200 trong MCC, để MCC tính thanh ghi theo clock; không tự nhập BAUD=115200 vào thanh ghi.

Terminal trên PC chọn **115200 baud, 8-N-1**, không hardware flow control. Hiện firmware gọi `SERCOM5_USART_Write()` trực tiếp. Không cần thêm SYS_CONSOLE/STDIO; `xc32_monitor.c` hiện chỉ có stub `read()`/`write()` trả về -1, nên không mặc định coi `printf()` đã được nối UART.

Trước khi ngủ, firmware MVP3 cần chờ `SERCOM5_USART_TransmitComplete()` để byte cuối rời chân TX, ngoài kiểm tra `WriteIsBusy()`.

## 7. RTC: bộ đếm đánh thức MVP3

Chọn **RTC → Hardware Settings**. Đặt đúng các ô trong bảng, tương ứng màn hình RTC đang dùng:

| Tùy chọn | Bật/tắt hoặc giá trị |
|---|---|
| Generate Frequency Correction API | Tắt |
| Continuous Synchronization for RTC Count/Clock Register | Tắt |
| RTC Operation Mode | **32-bit Counter with Single 32-bit Compare** |
| Enable Interrupts? | **Bật** |
| Compare 0 Interrupt Enable | **Bật** |
| Synchronization Ready Interrupt Enable | Tắt |
| Overflow Interrupt Enable | Tắt |
| RTC Prescaler | **DIV1** |
| Compare Value | **F000** trong ô đã có tiền tố `0x` |
| Clear on compare Match | **Bật** |
| Periodic Interval 0 đến 7 Event Output Enable | Tắt toàn bộ |
| Compare 0 Event Output Enable | Tắt |
| Overflow Event Output Enable | Tắt |
| Enable/Start Timer at initialization, nếu có | Tắt; ứng dụng chủ động Start |

Tính giá trị theo clock danh định:

```text
RTC input = 32768 / 32 = 1024 Hz
RTC counter = 1024 / DIV1 = 1024 tick/giây
Compare = 60 × 1024 = 61440 = 0xF000
```

Đây là mốc khoảng 60 giây. Clear-on-match cho phép lặp tự động, nhưng ứng dụng MVP3 phải Stop sau wake, đặt counter về 0 và Start trước lần ngủ kế tiếp để khoảng ngủ tính từ cuối giao dịch. Độ chính xác thực còn phụ thuộc oscillator, đồng bộ/ngắt và trình tự arm timer; không dùng cấu hình này để cam kết chu kỳ tuyệt đối chính xác 60.000 giây.

RTC có callback/ngắt compare để báo đến hạn; không cần bật Event Output hoặc EVSYS cho wake bằng ngắt. [Microchip: RTC PLIB và callback](https://onlinedocs.microchip.com/oxy/GUID-450989FA-38E4-4D68-AB61-15ADB29AD718-en-US-6/GUID-42D6CA1F-4B6E-451B-BD1A-ABDC7277EDDF.html)

## 8. NVIC, PM, NVMCTRL và EVSYS

### 8.1. NVIC / Interrupt Configuration

Mở phần **Interrupt Configuration / Interrupt Manager** trong System/Core hoặc plugin tương ứng; giữ:

| Nguồn ngắt | Enable | Priority | Handler |
|---|---|---|---|
| RTC | Bật | 3 | `RTC_InterruptHandler` |
| SERCOM1 | Bật | 3 | `SERCOM1_SPI_InterruptHandler` |
| SERCOM5 | Bật | 3 | `SERCOM5_USART_InterruptHandler` |

Giữ handler do MCC quản lý. Đăng ký callback RTC từ ứng dụng, không tự thay vector. Không thêm ngắt SysTick/TC4 cho các timer polling đang dùng.

### 8.2. PM / Power Manager

Giữ **PM** trong Project Graph. Mã hiện có `PM_IdleModeEnter()`, `PM_StandbyModeEnter()` và `PM_ResetCauseGet()`. PM có thể không có tùy chọn cần chỉnh trên thiết bị này; chế độ Standby được ứng dụng chọn khi gọi API, không tự xảy ra khi tick một ô MCC.

MVP3 chọn **Standby**: clock nhanh ngừng, RTC dùng GCLK1 tiếp tục chạy. Trình tự vào ngủ cần kiểm soát ngắt pending/cờ wake để không mất wake sát lúc `__WFI()`. [Microchip: Power Manager SAM D21](https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/sam/samd21-mcu-overview/peripherals/pm/)

### 8.3. NVMCTRL

Chọn **NVMCTRL** và giữ cấu hình hiện tại trong lần chỉnh clock/RTC:

| Tùy chọn | Giá trị hiện tại |
|---|---|
| Flash Wait States / RWS | **1**, với clock 48 MHz và nguồn board 3.3 V |
| Read Mode | NO_MISS_PENALTY |
| Power Reduction Mode During Sleep / SLEEPPRM | **WAKE ON ACCESS** |
| Cache Disable | Tắt, tức cache đang bật |
| Manual Write / MANW | Bật |

`SYS_Initialize()` đặt RWS=3 tạm thời trước khi đổi clock; sau đó `NVMCTRL_Initialize()` đặt RWS=1. Đối chiếu giá trị cuối, không sửa dòng RWS tạm chỉ để giống bảng.

Nếu cần giảm độ trễ wake, MCC có lựa chọn **WAKEUP INSTANT**; đây là thay đổi tối ưu riêng, chưa phải giá trị hiện tại. Không cần tắt cache hoặc đổi Read Mode trong bước cấu hình MVP3 cơ bản. [Microchip: lựa chọn wake của NVMCTRL trong ứng dụng low power](https://developerhelp.microchip.com/xwiki/bin/view/software-tools/harmony/low-power-application-on-samd21/step4/)

### 8.4. EVSYS

EVSYS hiện có trong Project Graph nhưng `EVSYS_Initialize()` rỗng và không được gọi trong luồng init hiện tại. Giữ tất cả channel/user/generator ở trạng thái không cấu hình. RTC wake qua NVIC nên không cần nối RTC event vào EVSYS.

Không thêm EIC, WDT, ADC, I2C, USB, DMA hoặc TCC chỉ để làm MVP3 theo timer. Nếu một tính năng mới cần chúng, cấu hình riêng và kiểm tra xung đột pin/clock.

## 9. Device Configuration / Fuse bits

Trong System/Core, tìm **Device Configuration / Configuration Bits / Fuses**. Giữ các giá trị đang sinh trong `initialization.c`:

| Mục | Giá trị |
|---|---|
| NVMCTRL_BOOTPROT | SIZE_0BYTES |
| NVMCTRL_EEPROM_SIZE | SIZE_0BYTES |
| NVMCTRL_REGION_LOCKS | 0xFFFF |
| BOD33USERLEVEL | 0x7 |
| BOD33_EN | ENABLED |
| BOD33_ACTION | RESET |
| BOD33_HYST | DISABLED |
| WDT_ENABLE | DISABLED |
| WDT_ALWAYSON | DISABLED |
| WDT_WEN | DISABLED |
| WDT_PER và WDT_EWOFFSET | CYC16384; chưa tác động vì WDT tắt |
| WDT_WINDOW_0 và WDT_WINDOW_1 | SET và 0x4; giữ giá trị hiện tại |

Không đổi BOD/Watchdog/fuse để thử giảm dòng ở cùng bước chỉnh RTC. Việc tối ưu chúng cần đánh giá điều kiện nguồn và đo riêng.

## 10. TC4/TC5, SysTick và tham số ngoài MCC

### 10.1. TC4/TC5 dành cho `app_time.h`

Hiện không có PLIB TC trong Project Graph. `LORA_APP_TIME_Initialize()` tự cấu hình:

| Mục | Giá trị ứng dụng đang dùng |
|---|---|
| Peripheral | TC4 ghép TC5 thành bộ đếm 32 bit |
| GCLK | GCLK0, 48 MHz |
| Prescaler | DIV1024 |
| Counter frequency | 46875 Hz |
| Continuous COUNT read sync | Bật qua RCONT/RREQ |
| Interrupt | Không dùng |
| Run in Standby | Không bật |

Giữ TC4/TC5 dành riêng cho ứng dụng; không thêm PWM/timer PLIB khác vào cặp này. Bộ đếm hiện tại phục vụ thời gian khi MCU thức; nó không đếm khoảng ngủ Standby. MVP3 phải dùng RTC hoặc cập nhật logic thời gian sau wake, thay vì chỉ thêm lệnh ngủ vào vòng chờ 60 giây.

### 10.2. SysTick

Driver DHT11 và SX1278 tự dùng SysTick để delay microsecond/millisecond, polling COUNTFLAG và tắt SysTick sau delay. Không bật SysTick interrupt hoặc thêm scheduler dùng SysTick đồng thời nếu chưa sửa driver. Giữ `CPU_CLOCK_FREQUENCY = 48000000U` đúng với clock thực đã cấu hình.

### 10.3. LoRa và cảm biến

Các giá trị sau được thiết lập trong `src/app/lora_app.c`/driver, **không phải tùy chọn MCC**:

| Tham số | Giá trị hiện tại |
|---|---|
| Địa chỉ node / gateway | 0x02 / 0x00 |
| LoRa frequency | 433 MHz |
| TX power | 17 dBm |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Hardware CRC | Bật |
| TX timeout ứng dụng | 3000 ms |
| ACK timeout | 1000 ms |
| Khoảng chờ sau giao dịch | 60000 ms |
| DHT11 | DATA trên PA00, GPIO đổi hướng bằng driver |

Gateway cần cùng thông số radio. Giữ tên GPIO để các macro `LORA_NSS_*`, `LORA_RESET_*`, `LORA_DIO0_*` và `DHT11_DATA_PIN` tiếp tục tồn tại.

## 11. Save, Generate và kiểm tra mã sinh

1. Lưu cấu hình MCC sau khi chỉnh clock, RTC, pin và ngoại vi.
2. Vào **Resource Management [MCC]**, bấm **Generate**. [Hướng dẫn Generate của Microchip](https://developerhelp.microchip.com/xwiki/bin/view/software-tools/harmony/low-power-application-on-samd21/step5/)
3. Nếu có cửa sổ merge, xem từng thay đổi. Giữ phần ứng dụng trong `src/main.c`, đặc biệt `LORA_APP_Initialize()` và `LORA_APP_Tasks()`. Không chấp nhận ghi đè hàng loạt phần source tự viết.
4. Đọc lại các file trong bảng dưới. Chỉ sửa cấu hình sinh mã qua MCC, không sửa PLIB để thay cho Generate.
5. Đối chiếu lại Pin Settings/CSV với dây thật; chạy Clean and Build.

| File cần kiểm tra | Giá trị mong đợi sau Generate |
|---|---|
| `peripheral/clock/plib_clock.c` | DFLL48M/GCLK0 giữ 48 MHz; thêm GCLK1 từ OSCULP32K chia 32, RUNSTDBY bật |
| Cùng file clock | RTC chọn generator 1; SERCOM1/5 giữ generator 0 |
| `peripheral/rtc/plib_rtc.h` | `RTC_COUNTER_CLOCK_FREQUENCY` tính ra **1024** |
| `peripheral/rtc/plib_rtc_timer.c` | Mode 0, DIV1, MATCHCLR, COMP=**0xF000**, bật CMP0; chưa Start tự động |
| `peripheral/sercom/spi_master/plib_sercom1_spi_master.c` | SPI Master, PAD0/PAD3, CPOL0/CPHA0, baud value 23 |
| `peripheral/sercom/usart/plib_sercom5_usart.c` | TX bật, RX tắt, 8-N-1, baud theo 115200/48 MHz |
| `peripheral/nvic/plib_nvic.c` | RTC/SERCOM1/SERCOM5 enable, priority 3 |
| `interrupts.c` | Vector RTC/SERCOM1/SERCOM5 trỏ đúng handler PLIB |
| `peripheral/port/plib_port.h` và `.c` | Đủ tên GPIO và mux đúng bảng pin |
| `peripheral/nvmctrl/plib_nvmctrl.c` | RWS=1 và các thiết lập đã chọn ở mục 8.3 |
| `initialization.c` | PORT, CLOCK, NVMCTRL, SPI, USART, RTC và NVIC được khởi tạo |
| `definitions.h` | Include các PLIB, CPU clock 48000000U |

MCC có thể sinh thanh ghi bằng số thay vì macro tên. Đối chiếu trong device pack: nguồn OSCULP32K của `GCLK_GENCTRL.SRC` là **3**, DFLL48M là **7**; `GEN(1)` là GCLK1. Kênh GCLK cho RTC là ID **4**, SERCOM1_CORE là **21**, SERCOM5_CORE là **25**. Không đổi nhầm ID kênh với số generator.

Nếu MCC đã hiển thị 1024 Hz nhưng `plib_rtc.h` còn 48000000, hoặc COMP còn `0x200`, cấu hình chưa sinh vào đúng project/configuration. Kiểm tra project đang chọn, Save/Generate và đường dẫn output `src/config/default/`.
