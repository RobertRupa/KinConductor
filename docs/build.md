# Build, upload and validation

[Documentation index](./README.md) · [Project README](../README.md)

PlatformIO firmware for the KinCony KC868-A6 based on ESP32-WROOM-32. Build
and upload with PlatformIO:

```sh
pio run
pio run -e kc868-a6-coprocessor -t upload --upload-port /dev/ttyUSB0
pio run -e kc868-a6-coprocessor -t uploadfs --upload-port /dev/ttyUSB0
pio run -e kc868-a6
pio run -e kc868-a6-coprocessor
pio device monitor
```

Select the serial upload port in the PlatformIO toolbar or add
`upload_port = /dev/ttyUSB0` to `platformio.ini` for your system.
Use the matching `-e` environment when uploading a feature-specific build.

## Version 1.1.6 working tree: serial profiles and boiler interface

The repository has no firmware OTA updater; the former second application slot
was unused. The partition table now has one **2.25 MiB** factory application
and **1.625 MiB** LittleFS partition. NVS remains at offset `0x9000`, size
`0x5000`. Firmware updates are serial-only and no longer have an OTA rollback
slot. The partition migration has not been tested on hardware; follow the
[one-time migration procedure](#deployment-precautions-and-current-profiles).

All ten configured profiles build cleanly with no compiler or partition
warnings. The maintained A6/coprocessor firmware images are **1,522,080 B**
and **1,522,240 B**;
each LittleFS image is **1,703,936 B**. The A6 build uses **81,472 B** of static
DRAM. Release builds use `-Os`, `-g0` and `-fno-exceptions`; the toolchain
already disables RTTI. Static DRAM is not runtime heap or stack measurement.

## Version 1.1.0: historical validation

The official pioarduino platform and tested libraries are pinned for this
release; see [tagged releases and image packaging](./releases.md).
Both firmware profiles and matching LittleFS images build successfully.
Tests: 32 native groups plus BLE/transport suites with ASan/UBSan; 15 Node
tests cover diagnostic/capability UI, header telemetry, calibration, hostname
pattern and release artifacts. `actionlint` 1.7.7 validates the workflow locally.

Final application binaries: base **1,532,032 B**, coprocessor **1,532,192 B**.
OTA slot: 1,572,864 B; remaining 40,832 / 40,672 B (about 40 KiB).
Static RAM: 75,096 B. LittleFS: 917,504 B per profile.
Measure binary files, not only PlatformIO's linked-section percentage.
This remains a tight budget, not capacity for all planned radio/protocol stacks.

Base firmware without RF24/coprocessor and latest assets were flashed to the
approved `/dev/ttyUSB0`, both hashes verified, NVS preserved.
Live KC868-A6 device: seven modules / 51 properties; header confirms
ESP32-D0WD-V3, two cores at 240 MHz, no PSRAM, 4 MiB flash.
Thirty consecutive capability responses parsed successfully after fixing a
transient truncated-JSON response caused by heap reallocation: the pool is
target-size-aware and the serialized String reserves its exact length once.
Allocation/serialization failures now return explicit HTTP errors.
Observed live heap minimum during this stress check: 21,772 B; this is not a
worst-case multi-peripheral guarantee.

Hostname canonicalization/save/reconnect and actual LAN mDNS resolution
The temporary `kinconductor-test.local` mDNS name resolved to the controller;
the original hostname was restored.
Current-config invalid requests preserve the previous record; browser ADC
capture, voltage-mode save and uncalibrated quality passed, then all channels
were restored OFF. Hostname/calibration survived the subsequent asset-upload
reset. At 390 px, current inputs are 38 px high, checkboxes 20×20, no horizontal
overflow or header overlap. Browser tests used DOM-triggered clicks/submissions,
not complete pointer/accessibility coverage.
No physical current sensor or AC RMS calibration was performed.
GitHub publication/Actions execution has **not** been performed in this session.

## Deployment precautions and current profiles

Confirm that `/dev/ttyUSB0` is the intended board, not another serial device.
Disconnect loads that could be unsafe during reset; relays initialize OFF,
but persisted IFTTT rules can act after startup. Uploading resets the controller.
Do not erase NVS or alter the partition table as part of routine deployment.
Uploading LittleFS replaces the web-assets filesystem.

The v1.1.5 release removed the unused second OTA app slot: firmware updates
are serial-only and have no OTA rollback path. Its factory app partition is
2.25 MiB and LittleFS is 1.625 MiB at `0x250000`; NVS remains at `0x9000`,
`0x5000`. PlatformIO's normal `upload` writes only the application, not the
partition table. For one-time migration from the prior dual-slot layout, build
the A6 profile and use one esptool invocation to write the partition table,
application and filesystem together:

```sh
~/.platformio/penv/bin/python -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash \
  0x8000 .pio/build/kc868-a6/partitions.bin \
  0x10000 .pio/build/kc868-a6/firmware.bin \
  0x250000 .pio/build/kc868-a6/littlefs.bin
```

This leaves NVS (`0x9000`–`0xdfff`) untouched and resets once after the writes.
Use the PlatformIO Python/esptool path and serial port appropriate to your OS.
Do not flash `firmware.factory.bin` for migration because it spans NVS. This
layout change has not been tested on physical hardware. After migration,
routine `upload` and `uploadfs` commands can be used separately.

`kc868-a6-coprocessor` is the default; `kc868-a6` keeps normal UART1 defaults.
The former selects a diagnostic UART1 default of 115200 baud on GPIO16/17,
not an implemented P17 or Zigbee/Thread bridge. Stored settings override
defaults. `KC868_BOARD_PROFILE_COPROCESSOR` is currently an unused marker;
`KC868_BOARD_PN532_SPI` only selects the resource-plan transport.
Other `KC868_BOARD_*` options currently affect reservation validation, not
hardware routing. UART2 P17 routing is runtime-configurable in Settings but
has not been physically validated. Do not infer DAC or PN532-I2C support from
resource-planner reservations.

Separate GPS/LoRa/nRF24 environments were removed. GPS and radio code still
uses compile flags; selecting GPS mode in the settings remains possible.
If enabling `USE_RADIO_NRF24`, also declare `nrf24/RF24` in `lib_deps`;
the maintained profiles do not currently include that dependency.
Do not enable LoRa and nRF24 together.


### Local validation and project metadata

Run the native characterization/contract tests after restoring the normal
firmware dependencies:

```sh
pio run -e kc868-a6
bash test/run_native.sh
```

The runner uses `g++` (or `CXX`), the existing ArduinoJson dependency, no RTTI
or exceptions, and AddressSanitizer/UndefinedBehaviorSanitizer. A total of
32 test groups plus BLE-presence/transport suites cover registry lifecycle/errors/limits, typed actions, stale
timestamps including `millis()` rollover, ADC failure recovery, exact legacy
JSON, all numeric settings bounds, profile defaults, record versions and
configuration failure/recovery. The production NVS adapter is exercised with
an isolated native Preferences test double; physical NVS power-loss behavior
and hardware reconfiguration still require board tests.
It removes its temporary executable on exit.

Optional npm shortcuts are defined in [package.json](../package.json):

```sh
npm test             # Native suites and 15 JavaScript/UI/release tests
npm run build:a6     # Base firmware, without RF24/coprocessor flags
npm run build       # Default coprocessor firmware
npm run build:matrix
npm run build:fs    # Build the LittleFS image with the updated WebGUI
```

Node.js 18+ is only required for the JavaScript tests/npm shortcuts; firmware
still builds with PlatformIO alone. No npm dependencies or installation are
required. DOM tests use Node's built-in runner and execute the production
diagnostic fetch/render functions; they are not browser/layout tests.
The current capability slice also received an integrated-browser smoke test
against explicitly synthetic APIs. Device testing remains separate.
Deploy both firmware and the updated LittleFS assets to see matching APIs/UI.
The earlier board-diagnostics slice added 24 B static RAM and about 4.3 KiB linked flash; complete
firmware images occupy 92.94–93.17% of the unchanged OTA slot. See
[the completed progress report](../TODO_REFACTORED.md)
for historical per-profile measurements and the latest deployment record.

### Initial capability slice: historical build measurements

Both maintained profiles and the base LittleFS image built successfully.
The latest complete firmware images are 1,485,872 bytes (base) and
1,486,048 bytes (coprocessor), about 94.47–94.48% of the unchanged 1,572,864-byte
OTA slot. Only about 85 KiB remains; this is not a budget for enabling every
planned extension simultaneously. The base image grew by 24,112 bytes from
the previously deployed image. Static RAM is 69,040 bytes (+2,416 bytes);
this is not a measurement of runtime heap usage or worst-case concurrent APIs.
The LittleFS image is 917,504 bytes, the unchanged partition image size.
No dependencies, partition table or release version were changed.

After a future base upload, verify `/api/capabilities`, `/api/settings/schema`
and `/api/screen` in addition to status/board/settings. Old firmware does not
implement these routes, and uploading only the new assets is insufficient.
The browser synthetic preview was not installed firmware and was stopped after
testing. The later extension deployment below includes these APIs.

After upload, verify the serial startup log and, when the device is reachable,
the status/board endpoints. Upload success verifies flash transfer, not physical
peripheral operation, long-term reliability or completion of the full plan.

## Earlier diagnostic deployment — 2026-10-31

The final installed firmware is **`kc868-a6` without coprocessor flags**, as
requested for this deployment. The project default remains
`kc868-a6-coprocessor`; use an explicit `-e kc868-a6` for subsequent base-board
uploads. A prior coprocessor upload was replaced with the base image.

- `npm test`: 25 native test groups and four diagnostic UI tests passed.
- Both maintained firmware profiles built successfully.
- Firmware transfer on `/dev/ttyUSB0`: 1,461,760 bytes, flash hash verified.
- Current LittleFS assets were uploaded and their flash hash verified.
- NVS was preserved; no full-chip erase was performed.
- Serial startup: OLED and RTC available, LittleFS mounted with index present,
  Wi-Fi connected. BME680/ENS160/compass/PN532 were not detected.
- Local authenticated HTTP checks passed: `/api/status` reported configuration
  ready and four ADC channels; `/api/board` reported a valid plan with 26 pins;
  `/api/settings` reported UART1 mode 0 at 9600 baud; `/` contained the new
  diagnostic table.

These are deployment/startup checks, not tests of relay loads, every optional
peripheral, power-loss recovery or the entire 50-task implementation plan.

## Extension deployment — 2026-10-31

After the user confirmed the board/port and safe loads, base **`kc868-a6`**
firmware and matching LittleFS were flashed successfully on `/dev/ttyUSB0`.
No coprocessor/RF24 flags or RF24 dependency were enabled. Symbol inspection
confirms the RF24 driver, packet history and persisted replay functions are absent.
RF24 was already disabled in the maintained profile; this slice additionally
removes its otherwise compiled monitor/storage implementation. No artificial
GPS/radio-specific environment was added.

- Final installed firmware: **1,513,312 bytes**, hash verified. Coprocessor
  build: 1,513,472 bytes. About 96.21–96.22% of the unchanged OTA slot,
  leaving approximately **58 KiB**; do not assume every planned driver fits.
- Static RAM: **74,384 bytes**. Short real-board checks observed free heap
  53,736 bytes / minimum 22,544 bytes; earlier browser/API checks reached
  minimum 22,136 bytes. These are observations, not a long-term or worst-case budget.
- LittleFS: 917,504-byte image, flash hash verified. NVS/partition layout preserved.
  Settings schema v1 was readable and new fields defaulted safely; subsequent
  settings writes use version 2 without discarding existing fields.
- Native suites and nine JS tests passed; both maintained firmware profiles built.
- Authenticated real APIs: six modules, 33 schema fields, Wi-Fi connected,
  BLE target validation/canonical save/readback, `warming_up` before first sighting,
  invalid PWM duty and overlapping output rejection, grid font/unit
  save/readback/rejection. Temporary targets/grid changes were restored.
- Real UDP packet reception updated count/text; `/api/actions` sent a reply
  received by a local socket. The temporary listen-port setting was restored.
- GPIO25 PWM enable + 50% action was accepted and reflected in its property,
  then set to zero and disabled/restored. This verifies local LEDC dispatch,
  **not an oscilloscope waveform, 0–10 V conversion or motor operation**.
- Browser DOM/layout smoke on a temporary real-device proxy verified IO numeric
  fields at 38 px height, 20×20 px checkboxes in IO/IFTTT, canonical sources,
  font choices and the 33-field settings form. At a 390 px viewport the IO
  form had no horizontal document overflow. Browser native click automation
  timed out; programmatic DOM clicks were used, so this is not an exhaustive
  pointer/accessibility test. Proxy/testing helper was stopped and removed.
- Physical DHT11, BLE arrival/departure, UART peers and fan-control voltage
  remain unverified without the respective external devices.

Run firmware upload and filesystem upload as **separate commands**:

```sh
pio run -e kc868-a6 -t upload --upload-port /dev/ttyUSB0
pio run -e kc868-a6 -t uploadfs --upload-port /dev/ttyUSB0
```

A combined `-t upload -t uploadfs` invocation caused PlatformIO's dependency
scanner to omit BusIO headers; separate upload commands succeeded without
installing new packages. The installed WebGUI is served by the board, not
the earlier static/synthetic preview.

## Additional base KC868-Ax build aliases

For baseline compilation with the same ESP32-WROOM family described in KinCony
KCS ESP32 documentation, [platformio.ini](../platformio.ini) now includes:

- `kc868-a4`
- `kc868-a8`
- `kc868-a8s`
- `kc868-a16`
- `kc868-a32`
- `kc868-a64`
- `kc868-a128`

These aliases currently extend the same base firmware layout as A6 and add a
model identifier macro for packaging/diagnostics workflows. They are **not**
proof of full board-level pinout/peripheral compatibility; validate each board
against its schematic/resources before deployment.

The firmware reports the selected model in the dashboard and generates a
matching SVG favicon at runtime. For Ax aliases other than A6, diagnostics
explicitly warns that the displayed GPIO/resource map is still the KC868-A6
baseline. No model-specific pin/IO map has been verified or implemented yet;
do not deploy these aliases on other boards as electrically validated images.
The header's free-flash figure is remaining capacity in the running application
partition, not unused capacity across every flash partition.
Per-core CPU utilization is shown when the target FreeRTOS build provides idle
runtime counters.

Each completed refactor slice ends with updating README, the implementation
checklist and affected project metadata/manifests. Change dependencies,
build flags, partition layout or release version only when the slice actually
requires it; do not advertise planned drivers as implemented.

## Settings, BLE and rule UDP deployment — 2026-10-31

Approved base **kc868-a6 without coprocessor/RF24** and matching final LittleFS
were flashed on `/dev/ttyUSB0`, both hashes verified; NVS was preserved.
These are post-v1.1.0 working-tree fixes, not replacement images for that
immutable tag.

- Both firmware profiles and both matching filesystem images build successfully.
  Base firmware **1,538,272 bytes**, coprocessor **1,538,432 bytes**;
  unchanged 1,572,864-byte OTA slot leaves **34,592 / 34,432 bytes**.
  Static RAM **76,784 bytes**; LittleFS **917,504 bytes**.
- `npm test`: 35 core/config/board/capability/measurement/UDP native groups,
  additional BLE/transport suites with ASan/UBSan, and 19 JavaScript tests pass.
  Local actionlint 1.7.7 passes; no actual GitHub Actions run was performed.
- Real APIs expose eight modules / 55 properties. BLE scans returned 9–13
  devices with canonical MAC addresses; UI cards and selection buttons work
  in DOM smoke checks. Physical range transitions remain unverified.
- Dashboard at 390/860/1000/1200 px: DI cells inside their panel, relay rows
  inside their width, no panel overlap or horizontal document overflow.
  Settings: all 33 fields exactly once, six functional groups, 38 px controls,
  20×20 px checkboxes; collapsing a group reduces its height and hides controls.
- Real UDP tests: separate and shared ports, exact/contains matching, sender
  IP/port filters, repeated packet actions and replies, mDNS/global-port HTTP
  409 rejection without record replacement. The real IFTTT UI saved and
  read back a rule-specific receive port and outgoing host/port/data correctly.
  Original rules were restored after tests. This is not a burst-delivery guarantee.
- Observed minimum heap reached approximately 15 KiB during the short test
  session; this is not a worst-case or long-term concurrency budget.

Browser pointer automation timed out; DOM interaction/layout tests are not
an exhaustive pointer/accessibility audit. Current sensor calibration, fan
waveforms and every optional peripheral remain hardware acceptance work.

## Multi-device BLE release validation — v1.1.1

Local validation: native ASan/UBSan suites plus **21 JavaScript tests** pass.
Both maintained firmware profiles and matching LittleFS build successfully.
Base image **1,543,376 bytes**, coprocessor **1,543,552 bytes**; unchanged
OTA slot leaves **29,488 / 29,312 bytes**. Static RAM **78,448 bytes**.
The maximum eight-module/eight-device capabilities response fits its JSON
budget in the native regression test; target heap stress remains unverified.

A static local preview with synthetic eight-device data checked saved cards,
visible removal buttons and no horizontal overflow at 390/860/1200 px.
It had no firmware APIs and did not interact with hardware. This does not
prove physical BLE range transitions, NVS power-loss behavior or a successful
GitHub Actions run. Firmware was **not uploaded** as part of this release;
update both firmware and LittleFS when deploying.

## WebGUI and tag-release validation — v1.1.2

Both firmware profiles and matching filesystem images build successfully;
native ASan/UBSan suites and **26 JavaScript tests** pass. Local actionlint
1.7.7 validates the tag-release workflow. Release-script tests cover draft
creation/reuse and propagation of permission/server errors.

Synthetic API/browser checks covered all eight tabs at 390/860/1200 px:
no off-screen controls or horizontal document overflow, numeric/text controls
at least 38 px, and no visible RF24 monitor/diagnostics in the disabled profile.
No-op interface saves preserved values 37 s / 2,750 Hz / 750 ms that were
missing from old presets. Blank IFTTT threshold rejected, multiline LF data
preserved, grid resizing refreshed coordinate bounds and empty positions were
restored with an error message. These are DOM/synthetic checks, not physical
firmware or comprehensive pointer/accessibility tests.

Base **1,543,376 bytes**, coprocessor **1,543,552 bytes**, static RAM
**78,448 bytes**, LittleFS **917,504 bytes**; unchanged OTA headroom
**29,488 / 29,312 bytes**. No new dependencies or partitions.
This version was not uploaded; no remote push or actual GitHub Actions run
was performed in this environment.
