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
site generator or dependencies. Start with the [documentation index](./docs/README.md).

| Chapter | Contents |
| --- | --- |
| [Build and upload](./docs/build.md) | PlatformIO profiles, tests, firmware and LittleFS deployment |
| [First start](./docs/first-start.md) | Wi-Fi provisioning, mDNS, web login and access precautions |
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
The [release workflow](./.github/workflows/release.yml) builds both profiles
on branch pushes, pull requests and manual runs. Pushing a matching `v*` tag
builds firmware and LittleFS, runs tests and publishes both profile archives
to GitHub Releases. Version/tag mismatches fail; publication can be retried.
Local builds/packages are verified; publication and a GitHub
Actions run have not been performed from this environment.

## Quick start

```sh
pio run -e kc868-a6-coprocessor
npm test
```

The default profile is `kc868-a6-coprocessor`. It currently uses UART1 on
GPIO16/17 for diagnostics; P17 routing and Zigbee/Thread/custom bridge
protocols are not implemented. Existing NVS settings take precedence over
profile defaults.

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
