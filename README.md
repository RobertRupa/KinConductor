# KC868-A6 PlatformIO project

Firmware starter for the KinCony KC868-A6 based on ESP32-WROOM-32. Build and
upload with PlatformIO:

```sh
pio run
pio run -t uploadfs
pio run -t upload
pio run -e kc868-a6
pio run -e kc868-a6-lora
pio run -e kc868-a6-nrf24
pio run -e kc868-a6-gps
pio run -e kc868-a6-coprocessor
pio device monitor
```

Select the serial upload port in the PlatformIO toolbar or add
`upload_port = /dev/ttyUSB0` to `platformio.ini` for your system.
Use the matching `-e` environment when uploading a feature-specific build.

## Board resources

| Resource | ESP32 / bus | Notes |
| --- | --- | --- |
| Relay outputs 1-6 | I2C PCF8574, address `0x24`, P0-P5 | Active-low; startup writes all-high (OFF). P6-P7 are reserved. |
| Digital inputs 1-8 | I2C PCF8574, address `0x22`, P0-P7 | Read as an active-low raw byte. |
| Analog inputs A1-A4 | GPIO36, GPIO39, GPIO34, GPIO35 | ADC1, board conditioning accepts 0-5 V; readings are raw 12-bit counts. |
| Configurable sensor 1 | GPIO32 | Select OFF, DS18B20 or DHT22 in the Sensors page. Default: DHT22. |
| Configurable sensor 2 | GPIO33 | Select OFF, DS18B20 or DHT22 in the Sensors page. Default: OFF. |
| SSD1306 OLED | I2C address `0x3C`, SDA GPIO4, SCL GPIO15 | One OLED configuration section; status, sensor, air or custom grid pages. |
| BME680 | I2C address `0x77`, SDA GPIO4, SCL GPIO15 | Temperature, humidity, pressure and gas resistance. |
| ENS160 | I2C address `0x53`, SDA GPIO4, SCL GPIO15 | AQI, TVOC and eCO2. |
| QMC5883L compass | I2C address `0x0D`, SDA GPIO4, SCL GPIO15 | X/Y/Z magnetic readings. |
| DAC1, DAC2 | GPIO26, GPIO25 | ESP32 DAC outputs, initialized to zero. |
| I2C | SDA GPIO4, SCL GPIO15 | PCF8574 input and relay expanders. |
| RS-485 | RX GPIO14, TX GPIO27 | UART2 (`Serial2`), 9600 8N1 starter setting. |
| RS-232 | RX GPIO17, TX GPIO16 | UART1 (`Serial1`), 9600 8N1 starter setting. |
| TTL header pins | GPIO12, GPIO13 | 3.3 V pins; no separate software-UART runtime is enabled. GPIO12 is an ESP32 strapping pin; GPIO13 is shared with PN532 SPI chip-select. |
| Shared SPI | SCK GPIO18, MISO GPIO19, MOSI GPIO23, CS GPIO5 | LoRa SX1278 or nRF24L01 module interface. |
| LoRa SX1278 | RESET GPIO21, DIO0 GPIO2 | Shares SPI; optional module. |
| nRF24L01 | CE GPIO22 | Shares SPI; optional module. |
| PN532 NFC | CS GPIO13, SCK GPIO18, MISO GPIO19, MOSI GPIO23 | Set the PN532 module to SPI mode. |
| USB/programming UART | RX GPIO3, TX GPIO1 | UART0, used by USB serial bridge. |
| Boot button | GPIO0 | ESP32 boot strap / download-mode button. |
| Reset | EN | Hardware reset; not a normal GPIO. |
| Wi-Fi / Bluetooth | ESP32-WROOM-32 radio | No external GPIO assignment. |

The manufacturer schematic is used for the peripheral wiring. The older
manufacturer forum pinout lists the LoRa RESET/DIO0 lines differently (GPIO14
and GPIO21); the schematic instead labels RESET as GPIO21 and DIO0 as GPIO2.
GPIO14 is also the schematic's RS-485 RX line, so the radio forum mapping must
not be applied together with RS-485. Verify the PCB revision before enabling
optional radio modules.

The default build profile is `kc868-a6-nrf24`, matching the current board
configuration: OLED, nRF24, DHT22 on GPIO32 and DA-1 beeper. LoRa and nRF24
share the SPI bus and board CS GPIO5, so select one radio per build.
The `kc868-a6-lora` and `kc868-a6-nrf24` environments enable their respective
drivers. The LoRa profile defaults to 433 MHz; override it with
`-DLORA_FREQUENCY_HZ=...` for the frequency of your SX1278 module. GPIO2 is an
ESP32 strapping pin; keep external circuitry from forcing an incompatible
level during reset. PN532 uses SPI with its own CS on GPIO13 and may share the
SPI bus with either radio.

At startup, firmware scans I2C addresses `0x08` through `0x77` and reports
detected addresses in the Interfaces page. GPIO12/13 are separate from the
RS-232 UART on GPIO16/17, but firmware does not enable them as a TTL UART. They
are 3.3 V pins; GPIO12 is a boot-strapping pin and GPIO13 is also the PN532 SPI
chip-select. Do not connect TTL logic directly to RS-232 voltage levels.

The RF24 IFTTT action sends a 32-bit `uint32_t` payload at 1 Mbps on channel 76
to pipe address `E8E8F0F0E1`. The receiving nRF24 device must use the same
channel, address and payload size; this is not a generic arbitrary remote
control protocol. Received 32-bit packets can be used as numeric IFTTT input.
The Interfaces page keeps a 16-packet receive history. Selected received
payloads can be saved in NVS and manually replayed or selected as an IFTTT
action; replay always uses the configured channel and pipe above.
Connect BME680 to VCC/GND and I2C SDA GPIO4/SCL GPIO15 (address `0x77`), ENS160
to the same I2C pins (address `0x53`), and QMC5883L to the same I2C pins
(address `0x0D`). PN532 must use SPI: CS GPIO13, SCK GPIO18, MISO GPIO19,
MOSI GPIO23, plus VCC/GND. OLED settings appear once in the Interfaces form;
the corresponding inventory item only reports detection status.

## First start and web panel

1. Build and flash the firmware, then upload the web files to LittleFS:

	```sh
	pio run
	pio run -t upload
	pio run -t uploadfs
	```

2. Join the open `KC868-A6-XXXX` access point and open `http://192.168.4.1`.
	Setup mode does not request Wi-Fi or web login passwords.
3. Choose a Wi-Fi network in the panel. Credentials are stored in ESP32 NVS;
	after a successful connection, open the IP shown in serial or
	`http://kc868-a6.local`.

The open setup access point starts when no saved network is available. It
requires no console to discover a password. Web authentication is bypassed only
while this AP is active; anyone nearby can access setup and relay controls, so
finish provisioning promptly. On the regular Wi-Fi network, the dashboard uses
HTTP Basic Authentication with the generated `admin` password stored in NVS
and printed to serial. HTTP is not encrypted; keep it on a trusted LAN and do
not forward its port to the internet.

The dashboard's right-hand event stream reports IFTTT triggers and action
failures, relay/DI/S2 changes, GPIO sensor configuration, and beeper tests. It
keeps the 24 most recent events in RAM and refreshes with the status view.

## Optional devices

- SSD1306 OLED at I2C address `0x3C` (128x64).
- DS18B20 or DHT22 sensors on GPIO32 and GPIO33, selectable in the Sensors
	page. DHT22 is sampled no faster than every two seconds.
- GPS on UART1, 9600 baud, using profile `kc868-a6-gps`.
- QMC5883L compass at I2C address `0x0D`.
- DS3231 RTC at I2C address `0x68`.
- BME680 environmental/air sensor, default I2C address `0x77`.
- ENS160 air-quality sensor at I2C address `0x53`; AQI, TVOC and eCO2 are read
  directly from its registers.
- PN532 NFC reader through SPI. The board's relay expander is also at I2C
  address `0x24`, which is the PN532 I2C address. Therefore PN532 must use SPI
  on this unmodified board. I2C mode requires isolating/rerouting one device or
  modifying the board's I2C topology; a simple parallel connection will not
  work.

The GPS profile repurposes UART1, which is also the board's RS-232 interface.
Most GPS breakouts use 3.3 V TTL UART, not RS-232 voltage levels; connect via a
proper level translator and do not connect TTL directly to the board's RS-232
connector.

The ESP32-WROOM-32 on KC868-A6 does not provide Zigbee or Thread radio support.
The `kc868-a6-coprocessor` profile configures UART1 at 115200 baud as a
diagnostic link for an external ESP32-C6/H2 or other radio coprocessor. It
reuses the RS-232 UART pins, so use an appropriate electrical interface and do
not use the GPS/RS-232 function simultaneously. The coprocessor must run its own
Zigbee NCP or Thread RCP firmware; this project does not implement those
protocol stacks.

The firmware probes optional devices at startup and updates the dashboard and
serial monitor at the configured sample interval. Addresses and sensor models
are defined in `src/main.cpp` and `include/kc868_a6_pins.h`. GPIO32 and GPIO33
are the selectable sensor pins; other MCU GPIOs are assigned to fixed board
circuits (expanders, UART, SPI, I2C, DAC or boot/reset) and are not offered as
arbitrary sensor pins.

In the Sensors page, GPIO32 and GPIO33 can each be assigned to OFF, DS18B20 or
DHT22. DHT22 readings are sampled no faster than once every two seconds; its
temperature and humidity appear in the sensor preview and can be used as IFTTT
conditions. The two pins are electrically independent, so configure only the
sensor actually connected to each one.

Sensor enable flags, sample interval, I2C clock (50/100 kHz), UART1 mode/baud,
and RS-485 baud are editable in the Sensors and Interfaces pages and persist in
NVS. The radio selection remains a build profile because the board shares the
radio chip-select pin. The Interfaces page also lists detected I2C/SPI devices,
serial buses, their addresses or GPIO mapping, and detection state.

The OLED section can disable the SSD1306, select automatic rotation or a fixed
status/sensor/air-quality/custom-grid page, choose grid fields, brightness/
contrast and rotation, and set the page interval. The IFTTT page stores
up to four rules in NVS and restores them on restart. Conditions can use either
configured temperature pin, DHT22/BME680 humidity, BME680 temperature, ENS160
AQI, S2, received RF24 packets, digital inputs or raw ADC readings. Actions can
set a relay, show an OLED page or send a saved RF24 payload when a condition
changes from false to true. RF24 transmission is available only in the
`kc868-a6-nrf24` profile and uses the pipe/channel above.

Bluetooth settings support OFF, BLE scan of nearby advertisers and a
`KC868-A6` BLE beacon. Scan results show device name, address and RSSI; Classic
Bluetooth discovery is not implemented in this firmware. BLE scanning and
beacon advertising are exclusive modes; the board's Bluetooth uses the ESP32
radio and does not need external GPIO wiring.

The Interfaces page can enable a beeper on DA-1 (DAC1/GPIO26), set its tone and
test duration, then play a short test tone. This uses PWM and disables analog
DAC output on DA-1 while enabled. Use only a buzzer whose voltage and current
rating are compatible with the board's DA-1 output stage. S2 is GPIO0 and can be
enabled as an active-low input; GPIO0 is also an ESP32 boot strap, so holding S2
while resetting the board can enter download mode.

The firmware starter initializes both UARTs, I2C, ADC, DAC, sensor GPIOs and
SPI, sets relay outputs OFF, and reports digital/analog inputs once per second.
No Ethernet controller appears in the supplied KC868-A6 schematic; network
connectivity here is the ESP32's built-in Wi-Fi/Bluetooth.

## Sources

- [KC868-A6 product page and schematic](https://www.kincony.com/esp32-6-channel-relay-module-kc868-a6.html)
- [Manufacturer ESP32 I/O pin definition](https://www.kincony.com/forum/showthread.php?tid=1962)
