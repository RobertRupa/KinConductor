# KinConductor

PlatformIO firmware for the KinCony KC868-A6 ESP32-WROOM-32 controller.
The current refactor provides typed peripheral contracts, ADC/IO and sensor
capabilities, dynamic IO/action and settings forms, configurable OLED page grids,
BLE presence, Wi-Fi/UDP/serial automation, configurable PWM outputs,
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

The latest capability-driven WebGUI adds **IO**, **Screen** and **Settings**.
Matching base firmware **without RF24** and LittleFS assets were uploaded on
2026-10-31. See [the extension slice](./docs/refactoring.md#presence-transports-and-output-configuration)
and [deployment checks](./docs/build.md#extension-deployment--2026-10-31).
Version **1.1.0** adds hardware information in the header, persistent network
identity in **Settings**, and current-sensor configuration in **Sensors**.
Set the UDP listen port in **Interfaces** (0 disables RX); the listener binds
to all local addresses (`0.0.0.0`). Outgoing destinations belong to IO/IFTTT actions.
The default LAN name is `http://kinconductor.local/`.
The [release workflow](./.github/workflows/release.yml) builds both profiles
from `v*` tags. Local builds/packages are verified; publication and a GitHub
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
