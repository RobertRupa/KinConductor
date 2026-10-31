# Troubleshooting builds, flash and WebGUI

[Documentation index](./README.md) · [Setup](./setup.md) · [API](./api-reference.md)

| Symptom | Check and resolution |
| --- | --- |
| `pio: command not found` | Activate the Python venv or place its `bin` on `PATH`; verify `pio --version` |
| First native test fails finding ArduinoJson | Build `pio run -e kc868-a6` once; tests read cached `.pio/libdeps`; do not run dependency installation concurrently |
| Upload port unavailable / permission denied | Confirm `pio device list`, physical adapter and `dialout` group/login; avoid assuming the first USB device is the board |
| Wrong firmware variant selected | Specify `-e kc868-a6`; the default is the *diagnostic coprocessor* profile |
| Compile exceeds OTA capacity | Current OTA app slots are 0x180000 (1,572,864 bytes); inspect full `firmware.bin` size and disable nonessential experimental drivers |
| Browser: UI file not found | Build and flash matching `-t buildfs` / `-t uploadfs`; do not change partitions or NVS |
| HTML returned from `/api/capabilities` | You may be viewing static files/incorrect server, or have mismatched firmware and LittleFS; update both from the same commit |
| Web Basic Auth prompts repeatedly | Use the generated `admin` password printed on serial; previously saved NVS credentials persist across firmware updates; setup AP is a different authentication mode |
| No `kinconductor.local` resolution | Confirm Wi-Fi/IP via serial or `/api/network`; check mDNS availability on the LAN and try direct device IP |
| BLE list not updating | Enable scan mode (1), not beacon/off; absence after first sighting is gated by timeout; random/private MAC may change |
| UDP packet not triggering | Check shared `udpListenPort` vs per-rule local ports, sender IP/port/text filters and edge-trigger conditions; outgoing host/port is a separate action |
| Sensor value `--`, `unavailable`, `warming_up` | Verify hardware and pin assignments, sensor enable flags, detection, calibration and property quality; never interpret missing data as zero |
| PWM/buzzer conflict | Check GPIO25/26 exclusivity in `/api/board`; PWM logic level is not AC speed control |
| RF24 controls missing | Expected on maintained builds: radio support is not compiled; no RF24 dependency is included |
| A4/A8/A16/A32 etc. GPIO map inaccurate | These are A6 baseline aliases, not hardware-validated board ports; consult exact PCB revision and manufacturer schematic |

**Safety:** a flashing reset can change physical outputs and restore
automation from NVS. Disconnect dangerous loads and do not erase the
whole chip just to fix WebGUI/authentication. Preserve saved configuration;
use targeted diagnostics first.
