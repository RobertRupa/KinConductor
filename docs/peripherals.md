# Peripherals, interfaces and automation

[Documentation index](./README.md) · [Project README](../README.md)

- SSD1306 OLED at I2C address `0x3C` (128x64).
- DS18B20, DHT22 or DHT11 sensors on GPIO32 and GPIO33, selectable in the Sensors
	page. DHT22 is sampled no faster than every two seconds.
- GPS on UART1, 9600 baud, when firmware is built with `-DUSE_GPS`.
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

The GPS compile flag repurposes UART1, which is also the board's RS-232 interface.
Most GPS breakouts use 3.3 V TTL UART, not RS-232 voltage levels; connect via a
proper level translator and do not connect TTL directly to the board's RS-232
connector.

The ESP32-WROOM-32 on KC868-A6 does not provide Zigbee or Thread radio support.
The `kc868-a6-coprocessor` profile configures UART1 at 115200 baud as a
diagnostic link for an external ESP32-C6/H2 or other radio coprocessor. It
uses the board resource-profile options (`KC868_BOARD_*`) and currently locks
PN532 transport to SPI (`KC868_BOARD_PN532_SPI`) while keeping the existing
RS-232/RS-485 pin routing unchanged. It still reuses the RS-232 UART pins for
UART1 diagnostics, so use an appropriate electrical interface and do not use
the GPS/RS-232 function simultaneously. The coprocessor must run its own
Zigbee NCP or Thread RCP firmware; this project does not implement those
protocol stacks.

The firmware probes optional devices at startup and updates the dashboard and
serial monitor at the configured sample interval. Addresses and sensor models
are defined in `src/main.cpp` and `include/kc868_a6_pins.h`. GPIO32 and GPIO33
are the selectable sensor pins; other MCU GPIOs are assigned to fixed board
circuits (expanders, UART, SPI, I2C, DAC or boot/reset) and are not offered as
arbitrary sensor pins.

In the Sensors page, GPIO32 and GPIO33 can each be assigned to OFF, DS18B20,
DHT22 or DHT11. DHT readings are sampled no faster than once every two seconds; their
temperature and humidity appear in the sensor preview and can be used as IFTTT
conditions. The two pins are electrically independent, so configure only the
sensor actually connected to each one.

Four [calibrated current-measurement channels](./current-measurement.md) use
the existing analog inputs without replacing the raw ADC properties.
Signal and amperes properties feed IO, IFTTT and OLED; no sensor is assumed
connected and all channels remain OFF until explicitly configured.

Sensor enable flags, sample interval, I2C clock (50/100 kHz), UART1 mode/baud,
and RS-485 baud are editable in the Sensors and Interfaces pages and persist in
NVS. The radio selection remains a compile-time option because the board shares
the radio chip-select pin. The Interfaces page also lists detected I2C/SPI devices,
serial buses, their addresses or GPIO mapping, and detection state.

The OLED section can disable the SSD1306, select automatic rotation or a fixed
status/sensor/air-quality/custom-grid page, choose grid fields, brightness/
contrast and rotation, and set the page interval. The IFTTT page stores
up to four rules in NVS and restores them on restart. Conditions can use either
configured temperature pin, DHT22/BME680 humidity, BME680 temperature, ENS160
AQI, S2, received RF24 packets, digital inputs or raw ADC readings. Actions can
set a relay, show an OLED page or send a saved RF24 payload when a condition
changes from false to true. RF24 transmission is available only in the
`-DUSE_RADIO_NRF24` build and uses the pipe/channel above.

The newer **IO** page uses registered capabilities for module readings and
typed action forms. **Settings** generates all current application fields from
the firmware schema. Native accordions organize the 33 fields into buses/UDP,
Bluetooth, sensors, outputs, inputs and OLED; network identity has its own
section. Expand a section to edit it; the application settings still save as
one flat record. **Screen** edits pages and binds cells to registered ADC,
DI/relay and sensor properties; positions are zero-based.
Select `displayMode:5` / “Interface grid” after saving to render those pages
on SSD1306. The layout has 1–8 pages, up to 4 rows/columns per page and 16
widgets total, with ASCII labels of at most 16 characters. Text is clipped to
the cell; the existing brightness, rotation and cycle interval still apply.
Saving a layout alone does not change the selected screen mode.

Widgets now also select Classic/FreeSans/FreeMono fonts, text size 1–3 and a
unit override (up to eight ASCII characters); empty unit uses the sensor unit.

IFTTT can select registered property IDs and “Interface action” with
descriptor-generated parameters. Legacy rule formats remain supported.
The generic relay channel uses 1–6; direct API actions are real operations,
not previews. See [architecture and limits](./refactoring.md#capability-driven-surfaces--2026-10-31).

### BLE, UDP, serial and PWM automation

Choose **DHT11** from either GPIO32/33 sensor list when that model is connected.
For BLE, select an ID in the scanner list and explicitly save it. Use
`ble.presence/<MAC>/in_range == 1` for entry or `== 0` for leave; timeout is
configurable in Interfaces/Settings. Before the first sighting there is no
valid leave signal. Private/random MAC addresses may change.

Wi-Fi state is `transport.main/wifi/connected` (1/0). Set a nonzero
`udpListenPort` for UDP reception. For repeated receive-trigger actions use
comparison **Changed** on `transport.main/udp/rx/count` or
`transport.main/serial/1/rx/count` / `/serial/2/rx/count`, not `count > 0`
(which only crosses once). Last text is also available in IO/grid bindings.

Generic send action examples:

```json
{"id":"transport.main/udp/send","parameters":{"host":"192.168.1.20","port":1234,"data":"hello"}}
{"id":"transport.main/serial/send","parameters":{"port":2,"data":"hello\n"}}
{"id":"fan.main/set","parameters":{"percent":50}}
```

UART1 send/receive is blocked while its mode is GPS or coprocessor. UART2
uses RS485, not raw 3.3 V UART pins. Text is bounded to 256 UTF-8 bytes;
serial uses newline or 25 ms idle framing. No remote acknowledgement is implied.
IO and IFTTT action data uses a multiline textarea with a 256-byte UTF-8
limit; browser textareas normalize line breaks to LF. This is not a binary
payload editor or a configurable CRLF encoder.

PWM is disabled by default on **DA-2 / GPIO25**. Enable/configure it only for
a suitable circuit or oscilloscope. The GPIO produces **3.3 V logic PWM**,
not direct AC control or a calibrated 0–10 V DC signal. The board's DA
terminals include LM258 conditioning; verify their actual waveform/filtering
and controller requirements before choosing onboard conditioning or a suitable
external PWM→0–10 V converter. See [schematic notes](./manufacturer-sources.md).
Beeper defaults to DA-1/GPIO26 and can select
DA-2; simultaneous enabled use of the same pin is rejected. Saving settings
resets PWM duty to zero. Details and limits are in the
[extension architecture](./refactoring.md#presence-transports-and-output-configuration).

Bluetooth settings support OFF, BLE scan of nearby advertisers and a
`KC868-A6` BLE beacon. Scan results show device name, address and RSSI; Classic
Bluetooth discovery is not implemented in this firmware. BLE scanning and
beacon advertising are exclusive modes; the board's Bluetooth uses the ESP32
radio and does not need external GPIO wiring.

The scan endpoint returns a bounded cache of up to 24 devices from the shared
nonblocking scanner, not a fresh blocking discovery on each click. A healthy
empty cache is not an error. Backend failures return HTTP 503 with an explicit
error displayed by the UI. Temporary MAC buffers are copied into JSON rather
than borrowed; all returned addresses remain valid after serialization.

### Saved BLE devices — v1.1.1

In **Interfaces**, select a scanned MAC or enter it manually, then choose
**Add ID**. Up to **eight distinct saved MACs** persist in NVS; duplicates,
invalid addresses and a ninth entry are rejected. Remove an individual device
using **Remove ID** next to its saved entry. An empty input does not clear the list.

Each saved device provides a stable Boolean property such as
`ble.presence/AA:BB:CC:DD:EE:01/in_range`. IFTTT lists these individually with
the MAC and `in_range`; IO and OLED grids use the same IDs. New IFTTT selectors
do not show the old generic BLE alias. Four IFTTT rules remain the system limit,
independent of the eight-device tracking limit.

Each device has its own last sighting and timeout state, using the shared
`blePresenceTimeoutSeconds` setting. A continuously observed advertisement
refreshes only its own state. Devices are tracked even if the 24-entry scan
display cache is full. Before the first sighting the quality is `warming_up`;
scanner disabled/failure is `unavailable`, never a valid departure. Reordering
or adding devices preserves sightings for unchanged MACs. Removed MAC IDs
are no longer available, so referencing rules cannot fire; edit those rules
or re-add the same device. Private rotating MACs remain unreliable identities.

`GET /api/bluetooth/targets` returns the saved addresses plus per-device
`property`, `in_range`, and `quality`. `POST` replaces the complete list:

```json
{"schemaVersion":1,"addresses":["AA:BB:CC:DD:EE:01","AA:BB:CC:DD:EE:02"]}
```

The stored `ble_targets` record also retains `legacyAddress` for compatibility.
When the record is absent, the previous `ble_target` MAC is imported; the new
record is written on the next successful save. Exact write/readback is required
before runtime publication; invalid/corrupt records return explicit errors,
not an empty successful list. Older firmware does not understand the new list.

Existing rules using `ble.presence/in_range` still refer to the original
legacy MAC, **not the first item after reordering**. Removing that MAC disables
its alias rather than retargeting it. The old single-target endpoint remains
compatible for zero/one saved entry; writes with multiple entries return
HTTP 409 directing callers to the list endpoint, to prevent accidental loss.
For newly created devices use the MAC-specific properties.

### Rule-specific UDP triggers

Choose **IFTTT → UDP packet on rule port**, then configure:

- Local receive port: 1–65535, bound on all local interfaces (`0.0.0.0`).
- Optional sender IPv4 address; empty accepts any sender.
- Sender port: 0 accepts any, otherwise 1–65535.
- Text match: exact or contains; empty text accepts any supported payload.
  Maximum 256 UTF-8 bytes; printable text plus TAB/CR/LF, not a binary protocol.

This is separate from `udpListenPort` in Interfaces/Settings, which controls
the shared transport counter/text listener. Rule listeners work when that
shared listener is disabled. Port 5353 is reserved for mDNS, and an active
rule port cannot equal the shared UDP port. Conflicting rule/settings saves
return HTTP 409 without replacing the previous record.

Example rule for `/api/ifttt` (the endpoint accepts an array of up to four):

```json
{
  "enabled": true,
  "source": "udpPacket",
  "udp": {"port": 5000, "remoteIp": "", "remotePort": 0, "match": "exact", "text": "start"},
  "action": "capability",
  "command": {"id": "transport.main/udp/send", "parameters": {"host": "192.168.1.20", "port": 5001, "data": "accepted"}}
}
```

Destination host, port and data belong to the **Interface action →
transport.main/udp/send**, not the receive-port fields. Save the rules
explicitly after adding/editing them. Rules sharing a local port share one
socket; at most four distinct ports are opened. Each matching packet queues
an action, even when consecutive packets contain the same text, unlike an
edge comparison on a numeric counter. Read match counters in IO or bind
`udp.rules/1/matches` through `/4/matches` to OLED/IFTTT.

Processing is bounded to four datagrams per socket per tick and four pending
matches per rule. Overflow/bind failures surface as module errors; there is
no reliable-delivery or burst-throughput guarantee. Disconnect closes sockets,
reconnect reopens them; configuration replacement resets counters and pending
matches. Empty UDP datagrams are not observable with this SDK's WiFiUDP API.

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

### WebGUI controls and build visibility — v1.1.2

RF24 panels, last-packet diagnostics and new radio rule options start hidden.
Only an explicit positive firmware capability enables them; a false/missing
flag, malformed response or failed request cannot show a stale RF24 monitor.
Stored radio rules remain readable, but cannot transmit in maintained builds.

Sampling accepts 500–60,000 ms, OLED page interval 2–60 seconds, beeper
frequency 250–5,000 Hz and test duration 50–3,000 ms. These are numeric inputs,
not incomplete preset lists that lose valid saved values. UDP/PWM/BLE numeric
fields are required; an empty input cannot silently become zero.
Settings schema mode/type/pin fields reuse named choices from the corresponding
specialist form, including DHT11. Grid coordinates are zero-based and constrained
by the current dimensions; empty coordinates are rejected and resizing updates
their bounds. Text/number controls have consistent minimum sizing, while native
checkboxes and the OLED brightness slider retain their dedicated presentation.
