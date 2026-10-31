# WebGUI settings and parameter reference (firmware 1.1.3)

[Documentation index](./README.md) · [WebGUI](./webgui.md) · [API](./api-reference.md)

This table is derived from `DeviceSettings` and `settingsSchema()`
in [device_settings.h](../include/config/device_settings.h) and
[device_settings.cpp](../src/config/device_settings.cpp).
The **33 IDs are case-sensitive**. Authoritative ranges and enums at runtime
are returned by `GET /api/settings/schema`. `GET /api/settings` returns
the complete **flat** state; `POST /api/settings` accepts a flat JSON
object (including partial updates), validates types and ranges and returns
the resulting complete state. Unknown keys are rejected; booleans must be
JSON `true`/`false`, numeric values nonnegative integers.

## Sensor sampling and buses

| ID | Default (base A6) | Allowed values / units | Effect |
| --- | --- | --- | --- |
| `sensorIntervalMs` | 1000 | 500–60000 ms | Main sensor polling interval |
| `i2cClockHz` | 100000 | **50000 or 100000** Hz | I²C clock |
| `rs485Baud` | 9600 | 9600, 19200, 38400, 57600, 115200 | UART2 / RS-485 rate |
| `uart1Baud` | 9600 | Same five rates | UART1 / RS-232 rate; coprocessor profile defaults to 115200 |
| `uart1Mode` | 0 | 0 RS-232, 1 GPS, 2 coprocessor | Exclusive UART1 role; does not install GPS/bridge firmware |

## Sensor and input configuration

| ID | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| `gpio32SensorType` | 2 | 0 OFF, 1 DS18B20, 2 DHT22, 3 DHT11 | GPIO32 sensor |
| `gpio33SensorType` | 0 | Same as GPIO32 | GPIO33 sensor |
| `temperature1Enabled` | true | boolean, **derived** | Normalized to `gpio32SensorType != 0` |
| `temperature2Enabled` | false | boolean, **derived** | Normalized to `gpio33SensorType != 0` |
| `s2InputEnabled` | false | boolean | S2/BOOT button input |
| `bme680Enabled` | true | boolean | BME680 driver use, if detected |
| `ens160Enabled` | true | boolean | ENS160 driver use, if detected |
| `compassEnabled` | true | boolean | Compass, if detected |
| `gpsEnabled` | false | boolean | GPS runtime enable; requires compiled GPS support and UART1 role |
| `rtcEnabled` | true | boolean | RTC use, if available |
| `pn532Enabled` | true | boolean | PN532 use, if wired without conflicts |

For the coprocessor profile `uart1Mode=2`, `uart1Baud=115200`
at **initial defaults**, but persisted NVS can replace them.
`gpsEnabled` is not a substitute for the `USE_GPS` build flag.

## OLED / screen

| ID | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| `displayEnabled` | true | boolean | Display enable |
| `displayMode` | 0 | 0 auto, 1 status, 2 sensors, 3 air, 4 legacy grid, **5 interface grid** | Selected OLED view |
| `displayPageSeconds` | 5 | 2–60 s | Page rotation interval |
| `displayBrightness` | 160 | 0–255 | Brightness/contrast |
| `displayRotation` | 0 | 0,1,2,3 (0°,90°,180°,270°) | Orientation |
| `displayTextSize` | 1 | 1–2 | Legacy OLED text scale |
| `displayFieldMask` | 63 | 0–63 | Legacy grid field bit mask |

The independent `/api/screen` editor supports 1–8 pages, 1–4
rows/columns per page, up to 16 widgets overall, zero-based positions,
property bindings, Classic/FreeSans/FreeMono fonts, widget text size 1–3
and short ASCII labels/units. Save layout **then** set
`displayMode=5` to render interface widgets; the two text-size controls
apply to different layers.

## Bluetooth, UDP and outputs

| ID | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| `bluetoothMode` | 1 | 0 off, 1 scan, 2 beacon | Mutually exclusive BLE modes |
| `blePresenceTimeoutSeconds` | 60 | 5–3600 s | Saved device presence expiry |
| `udpListenPort` | 0 | 0–65535 | Shared UDP listener; **0 disables**, nonzero binds 0.0.0.0 |
| `beeperEnabled` | false | boolean | Enable buzzer output |
| `beeperPin` | 26 | **25 or 26** GPIO | Beeper output allocation |
| `beeperFrequencyHz` | 2000 | 250–5000 Hz | Beep frequency |
| `beeperDurationMs` | 250 | 50–3000 ms | Beep duration |
| `fanEnabled` | false | boolean | Enable PWM |
| `fanPin` | 25 | **25 or 26** GPIO | PWM allocation |
| `fanFrequencyHz` | 1000 | 100–25000 Hz | PWM frequency |

Beeper and PWM cannot occupy the same GPIO while both enabled. GPIO25/26
belong to the conditioned DA outputs; the MCU PWM signal is **3.3 V
logic**, not a mains motor regulator or direct 0–10 V analog voltage.
Inspect the board conditioning and connected load. The shared UDP port must
not collide with per-rule UDP listeners, which can use distinct ports.

## Other persistent records (not in the 33-field settings schema)

| WebGUI location | API | Behavior and limits |
| --- | --- | --- |
| Settings → Network | `/api/network` | `{schemaVersion:1,hostname:"kinconductor"}`; one DNS label, 1–63 ASCII chars, lowercase normalization, no `.local`; reconnect after change |
| Interfaces → BLE | `/api/bluetooth/targets` | Up to 8 distinct saved MACs; presence properties `ble.presence/<MAC>/in_range`; scanner cache is separate |
| Sensors → Current | `/api/current` | A1–A4, each OFF/voltage/current-loop with explicit two-point calibration; see [Current measurement](./current-measurement.md) |
| IFTTT | `/api/ifttt` | Up to 4 rules, including independent UDP packet listeners and capability actions |
| Screen | `/api/screen` | OLED grid, page/widget limits above; saving does not select display mode |
| Wi-Fi dialog | `/api/wifi` | Network credentials stored in NVS, not in the application-settings JSON |

Be cautious with network/MAC/credentials and exported settings. A feature
may appear in the GUI while its driver is missing, disabled or conflicted;
inspect `/api/capabilities`, `/api/board` and actual device status.

## Example: read then change one parameter

```bash
# Set URL and enter the device-generated admin password without shell history.
BASE=http://kinconductor.local
read -r -s -p 'Web password: ' KC_WEB_PASSWORD; echo
curl -fsS --user "admin:$KC_WEB_PASSWORD" "$BASE/api/settings"
curl -fsS --user "admin:$KC_WEB_PASSWORD" "$BASE/api/settings/schema"
# Example only: changes persistent NVS; review existing rules and wiring first.
# curl -fsS --user "admin:$KC_WEB_PASSWORD" \
#   -H 'Content-Type: application/json' -d '{"udpListenPort":61631}' \
#   "$BASE/api/settings"
unset KC_WEB_PASSWORD
```

Avoid using `curl -u admin:password` as a literal command in shared
scripts or logs. The WebGUI forms use the same backend validators.
