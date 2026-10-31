# Build profiles and supported configurations

[Documentation index](./README.md) · [Setup](./setup.md) · [Hardware](./hardware.md)

The source of truth is [platformio.ini](../platformio.ini), the board resource
map and the runtime capabilities API. A successful compile does **not**
establish electrical or peripheral compatibility.

## Environment matrix

| PlatformIO environment | Intended meaning | Support level |
| --- | --- | --- |
| `kc868-a6` | Standard A6, no coprocessor flag | Maintained base; use for the known A6 board |
| `kc868-a6-coprocessor` | A6 with coprocessor resource markers and UART1 default 115200 | Maintained build; **no** P17/NCP/RCP bridge |
| `kc868-a4`, `kc868-a8`, `kc868-a8s` | A6-based compile aliases with different visible model identity | Unvalidated pin/electrical mapping |
| `kc868-a16`, `kc868-a32`, `kc868-a64`, `kc868-a128` | Same A6-based baseline aliases | Unvalidated pin/electrical mapping |
| `kc868-a6-base` | Shared inherited configuration | Parent environment, not the preferred deployment target |

The source default in `[platformio]` is `kc868-a6-coprocessor`.
Select `-e kc868-a6` explicitly for normal A6 updates. All Ax aliases
still use `board = esp32dev` and the A6 pin map; they change WebGUI model
identity and warning text, **not** physical relay/input channel counts,
expander routing or board design. Verify the manufacturer schematic for
your exact model/revision before using an alias on hardware.

## Compile-time versus runtime options

| Choice | Mechanism | Consequence |
| --- | --- | --- |
| Board identity | `KC868_TARGET_MODEL` environment flag | WebGUI identity/favicon/diagnostics; not remapping |
| Coprocessor marker | `USE_ZIGBEE_THREAD_COPROCESSOR`, `KC868_BOARD_PROFILE_COPROCESSOR` | Diagnostic UART1 defaults; no Thread/Zigbee stack |
| GPS code | `USE_GPS` | UART1 role; requires compatible wired receiver; not available from setting alone |
| LoRa code | `USE_RADIO_LORA` | SPI radio implementation, shared CS GPIO5 |
| nRF24 code | `USE_RADIO_NRF24` and explicit `nrf24/RF24` in `lib_deps` | SPI radio; absent from maintained builds |
| PN532 selection | `KC868_BOARD_PN532_SPI` | Resource-plan selector; actual board uses SPI |
| Sensor/UI/rule options | WebGUI `/api/settings`, separately versioned NVS records | Persist across reboot and normal firmware upload |

**Do not** enable `USE_RADIO_LORA` and `USE_RADIO_NRF24` together.
They contend for the same SPI chip-select. GPIO12 is an ESP32 boot strap;
GPIO13 is used as PN532 CS. Wiring P17 UART over GPIO12/13 is proposed, not
implemented; physically routing an MCU UART to P17 would displace an existing
serial function. UART1 on GPIO16/17 is behind the A6's RS-232 interface and
must not be treated as a direct 3.3 V TTL header. UART2 is assigned to
RS-485. See [Hardware](./hardware.md) and [Peripherals](./peripherals.md).

## Example: separate experimental build

Instead of modifying a maintained profile, append a clearly named local
environment to `platformio.ini` for testing and review:

```ini
[env:kc868-a6-lora-experimental]
extends = env:kc868-a6
build_flags =
    ${env:kc868-a6.build_flags}
    -DUSE_RADIO_LORA
```

Then run `pio run -e kc868-a6-lora-experimental`.
This only demonstrates selecting the existing compile flag; it is **not** a
hardware qualification, antenna/frequency or safety recommendation. Consult
the specific module datasheet and set the LoRa frequency as required by its
installed band and firmware configuration. Experimental builds can exceed
the 1,572,864-byte OTA slot; check final `firmware.bin` length and
available runtime heap. RF24 requires adding its library to the experimental
environment in addition to the flag. Neither maintained profile compiles RF24.

## Runtime and persistent configuration

The WebGUI exposes 33 application settings with validated limits, plus
independent records for hostname, OLED screen layout, current calibration,
BLE target list and IFTTT rules. See the complete
[Settings reference](./settings-reference.md) and
[API reference](./api-reference.md). The main settings API is a **flat**
JSON record, even though WebGUI uses grouped forms. Older saved NVS
values override a new profile's initial UART or feature defaults.
To inspect rather than overwrite settings, use authenticated
`GET /api/settings`, `GET /api/network` and `GET /api/board`.
