# KinConductor

PlatformIO firmware for the KinCony KC868-A6 ESP32-WROOM-32 controller.
The current refactor provides typed peripheral contracts, ADC/IO and sensor
capabilities, dynamic IO/action and settings forms, configurable OLED page grids,
up to eight independent BLE presence targets, Wi-Fi/UDP/serial automation, configurable PWM outputs,
calibrated current-input interfaces, configurable hostname/mDNS, hardware
header telemetry, versioned configuration persistence and board-resource diagnostics.
The full expansion plan is **not complete**.

## Documentation

The documentation is a repository-local Markdown wiki, without an additional
site generator or dependencies. English is the default language for repository
documentation. Start with the [documentation index](./docs/README.md).

| Chapter | Contents |
| --- | --- |
| [Build and upload](./docs/build.md) | PlatformIO profiles, tests, firmware and LittleFS deployment |
| [First start](./docs/first-start.md) | Wi-Fi provisioning, mDNS, web login and access precautions |
| [WebGUI guide and screenshots](./docs/webgui.md) | Live page screenshots, configuration guide and firmware/LittleFS flashing |
| [Hardware](./docs/hardware.md) | GPIO pinout, buses, wiring, conflicts and manufacturer sources |
| [Peripherals and automation](./docs/peripherals.md) | Sensors, serial ports, radios, OLED, BLE, beeper and local IFTTT |
| [Architecture and refactoring](./docs/refactoring.md) | Implemented contracts, NVS migration and current limitations |
| [Current measurement](./docs/current-measurement.md) | Four channel configurations, two-point calibration, loop receivers and sensor limits |
| [Manufacturer resources](./docs/manufacturer-sources.md) | KinCony forum/KCS documentation, schematic evidence and revision warnings |
| [Release images and CI](./docs/releases.md) | Tagged builds, image manifests, SHA256 and deployment precautions |
| [Implementation specification](./REFACTOR_TODO.md) | Dependency-ordered 50-task plan and acceptance criteria |
| [Completed work](./TODO_REFACTORED.md) | Verified implementation slices and historical measurements |
| [LLM project context](./docs/llm-memory.md) | Project instructions, memory and peripheral workflow skill |

The capability-driven WebGUI includes **IO**, **Screen** and **Settings**.
Matching base firmware **without RF24** and LittleFS assets were uploaded on
2026-10-31. See [the extension slice](./docs/refactoring.md#presence-transports-and-output-configuration)
and [deployment checks](./docs/build.md#extension-deployment--2026-10-31).
Version **1.1.0** adds hardware information in the header, persistent network
identity in **Settings**, and current-sensor configuration in **Sensors**.
Recent releases add responsive dashboard inputs, grouped
Settings accordions, reliable BLE scan JSON and
[rule-specific UDP triggers](./docs/peripherals.md#rule-specific-udp-triggers).
Set the shared UDP listen port in **Interfaces** (0 disables that listener).
For independent listeners, choose **IFTTT → UDP packet on rule port** and set
the local port, sender filters and text match. Listeners bind to `0.0.0.0`;
outgoing destination address/port/data belong to IO/IFTTT send actions.
The default LAN name is `http://kinconductor.local/`.
Version **1.1.1** adds a saved BLE list in **Interfaces**. Each MAC appears
separately in IFTTT as `BLE · MAC · in_range (1/0)` and is available in IO/OLED.
Use `== 1` for entry and `== 0` for leave; initial unseen devices remain
`warming_up`, not a valid leave event. See [BLE list details](./docs/peripherals.md#saved-ble-devices--v111).
Version **1.1.2** keeps RF24 monitors, diagnostics and IFTTT radio options
hidden unless firmware explicitly reports compiled support. Settings inputs
now cover the full backend ranges, mode fields have named choices, and
UDP/serial action data supports multiline LF text. Screen-grid inputs have
consistent sizing and position limits that update when the grid changes.
Both firmware and LittleFS must be updated to deploy these fixes; v1.1.2
has not been uploaded to the physical board from this session.
Version **1.1.3** identifies the selected KC868-Ax build in the WebGUI and
favicon, warns when a non-A6 alias still uses the A6 baseline pin map, and
reports remaining application-partition capacity plus per-core CPU load where
supported. These aliases are build baselines, not validated electrical
profiles for the other boards.
The [release workflow](./.github/workflows/release.yml) builds every configured
PlatformIO environment in a separate job. It runs only when a `v*` tag is
pushed; branch pushes, pull requests and manual runs do not trigger it. A
matching tag builds firmware and LittleFS, runs tests, and publishes all ten
profile archives to GitHub Releases after every job passes. The A4/A8/A8S/A16/
A32/A64/A128 aliases reuse the A6 baseline pin map and are not electrically
validated for those boards. Version **1.1.4** updates this release workflow.

The working tree prepares **1.1.7** with one 2.25 MiB factory application
partition and 1.625 MiB LittleFS instead of the unused second OTA slot. This
firmware has no OTA update or rollback path; updates are serial-only. Migration
from the previous partition layout requires uploading the firmware/partition
table and matching LittleFS. NVS remains at the same offset and size; physical
board migration has not been tested. See the [one-time migration procedure](./docs/build.md)
before upgrading an existing installation.

## Browser factory flasher

[KinConductorFlasher](https://robertrupa.github.io/KinConductorFlasher/) is a
browser-based installer for published KinConductor firmware releases. It
verifies the selected release manifest and image hashes, then installs the
factory firmware and matching LittleFS image over USB. Use desktop Chrome or
Edge over HTTPS; the browser must support Web Serial. The catalog contains
published releases, so it may not include this repository's untagged changes.

**Warning: factory installation can erase NVS**, including saved Wi-Fi
credentials and calibration, and overwrites OTA metadata. Back up settings,
verify the board and serial port, and disconnect unsafe relay loads before
proceeding. This installer is for first installation or deliberate recovery,
not routine configuration-preserving updates. For this checkout's current
single-partition layout, follow the [PlatformIO deployment instructions](./docs/build.md)
and the required one-time migration steps instead.

## Quick start

```sh
pio run -e kc868-a6-coprocessor
npm test
```

The default profile is `kc868-a6-coprocessor`. It currently uses UART1 on
GPIO16/17 for diagnostics. UART2 supports a configurable RS-485/P17 route and
raw serial or Modbus RTU; P17 coprocessor, Zigbee/Thread/custom bridge
protocols remain unimplemented. Existing NVS settings take precedence over
profile defaults.

Additional **base ESP32-WROOM aliases** are available for KC868-A models listed
by KinCony KCS ESP32 documentation: `kc868-a4`, `kc868-a8`, `kc868-a8s`,
`kc868-a16`, `kc868-a32`, `kc868-a64`, `kc868-a128`.
These profiles currently reuse the A6 firmware layout and are intended for
build-time baseline compatibility work, not validated pinout-equivalent runtime
support on those boards.
The WebGUI model badge/title/favicon follow the selected build; diagnostics
warns that non-A6 aliases still display the A6 GPIO map. The header now shows
the remaining application-partition capacity and, where FreeRTOS runtime counters
are enabled, CPU load by core. See [build and profile limits](./docs/build.md).

Before uploading, disconnect unsafe loads, confirm the board and port,
and read the [deployment instructions](./docs/build.md).
Both firmware and LittleFS assets are required for the current WebGUI.

## License and attribution

This project is available free of charge under the [KC868-A6 Attribution
License](./LICENSE). Use, modification, and commercial distribution are allowed
subject to its attribution condition: credit Robert Rupa in project Authors or
Contributors information and in package metadata.
The software is provided as-is, without warranty or liability to the extent
permitted by applicable law. Third-party components retain their own licenses.
