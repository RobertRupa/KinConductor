# Architecture and refactoring

## Current measurement, network identity and releases (1.1.0)

The seventh registry module, `current.main`, derives eight typed properties
and condition descriptors from the existing ADC provider. It uses a portable,
strict, versioned two-point calibration codec and a separate NVS record.
No raw ADC provider or existing property IDs were replaced. The Sensors form,
IO, IFTTT and OLED bindings share those descriptors; see
[current-sensor contracts and hardware limits](./current-measurement.md).

The portable network codec validates/canonicalizes a single hostname label,
default `kinconductor`. NVS load/save/readback, Wi-Fi hostname-before-connect,
mDNS lifecycle/retry and `/api/network` are wired through the composition root.
The header shows actual chip/heap/PSRAM/flash values. UDP binding stays
`0.0.0.0`; listener and outbound destinations are documented separately.

The seven-module JSON test exposed a pool limit; real board testing additionally
exposed transient response String truncation under heap pressure. Capacity now
uses target-aware ArduinoJson slot sizes plus copied-string budget, and exact
response reservation/length checks prevent success-shaped malformed JSON.
The startup IFTTT legacy parser now matches the runtime's 8192-byte capacity
and preserves a record on `NoMemory` rather than resetting valid large rules.

Release metadata, official platform/library pins, two-profile tag CI and
image/partition/hash validation are prepared and locally tested. Base A6 and
matching assets were deployed and tested; no external repository publication
or GitHub Actions execution is claimed. See [current validation](./build.md)
and [release procedure](./releases.md).
The larger 50-task expansion plan remains incomplete.

[Documentation index](./README.md) · [Project README](../README.md)

[REFACTOR_TODO.md](../REFACTOR_TODO.md) contains the implementation
specification and staged checklist for LLM agents: peripheral interfaces,
capability-driven IFTTT and UI, OLED drivers, new devices and transports,
relay RAM/NVS startup policies, migrations, board diagnostics and acceptance
tests. It also assesses cellular/GNSS, Meshtastic, Zigbee/Thread, audio,
CC1101, IR/IrDA, memory and hardware upgrades, multi-core execution, and
NTP plus scheduled IFTTT triggers. This is a plan; the listed extensions
are not implemented yet. Section 21 specifies the P17 hardware-UART
coprocessor profiles (H2/C5/C6), a custom offload bridge and optimized
single/dual-expansion builds.
Sections 7.5 and 8.1–8.3 specify configurable network hostname and mDNS
discovery, plus per-page OLED grids, configurable pages and interface-data
widgets. These features are planned, not enabled by the documentation.
Section 13 is the single implementation checklist: all 50 tasks are ordered
in eight architecture-first stages, with every prerequisite before its
dependent task. Optional features remain subject to the selected profile.

### Implemented first slice

The refactor has started with portable C++17 contracts and the ADC module:

- [Core interfaces](../include/core/peripheral.h) separate peripheral
  lifecycle, typed properties, trigger descriptors and synchronous actions.
- [PeripheralRegistry](../include/core/peripheral_registry.h) uses explicit,
  non-owning bindings without RTTI or dynamic allocation. It validates IDs
  and action argument types, tracks registration revisions and reads
  timestamped snapshots.
- [AnalogInputs](../include/peripherals/analog_inputs.h) owns the four raw
  ADC channels. Its hardware adapter remains in the composition code in
  [src/main.cpp](../src/main.cpp); sampling keeps the existing report interval.
  OLED and serial output use its cached values, and legacy IFTTT reads its
  typed properties through the registry.
- The existing fields of `/api/status` are preserved. The analog array now
  retains the full 0–4095 range instead of truncating values to 8 bits.

The registry is currently single-threaded. Bindings and immutable descriptor
storage must outlive registration; callers handle returned errors. Action
providers validate domain-specific ranges after the registry validates types.
`begin`/`tick` return the first failure while allowing other modules to run;
per-module health retains individual errors. Trigger descriptors do not yet
implement an event queue, and the legacy report timer is not a new scheduler.
The first slice alone did not expose capabilities to HTTP/UI; the later slice
below adds those surfaces. Per-module configuration schemas, asynchronous
actions, EventBus and the remaining drivers are still pending.

### Implemented configuration slice

- [DeviceSettings](../include/config/device_settings.h) no longer belongs to
  WebGUI. One typed schema defines the existing 27 fields, legacy NVS keys,
  ranges and allowed values.
- [ConfigService](../include/config/config_service.h) validates changes,
  skips unchanged writes and publishes new settings only after persistence
  succeeds. WebGUI receives the service through constructor injection;
  hardware application still uses the existing callback.
- [PreferencesSettingsStore](../src/config/preferences_settings_store.cpp)
  imports existing `kc868a6` keys without deleting them. The first changed
  save creates an authoritative `device_cfg` record with `schemaVersion: 1`.
  Each save writes one NVS string and verifies its readback.
- `/api/settings` keeps the existing response fields and supports partial
  updates. Unknown fields, incorrect types, unsupported values and integer
  overflow are now rejected rather than silently ignored or narrowed.
  Temperature-enable aliases remain derived from GPIO sensor selectors.
- Failed initialization or saving is explicit: settings requests return
  HTTP 503 when unavailable; a failed save returns HTTP 500 without applying
  the candidate. `/api/status` adds `configuration.ready` and
  `configuration.error`. Startup load failures are logged and keep the
  profile defaults, not a silently recovered older record.

A failed write/readback marks configuration unavailable until reloaded
(currently by restarting the firmware). Readback failure can occur *after*
the new record was committed: RAM/hardware keep their prior settings, but
restart may load the new record. This is not a hardware rollback or an
application-level candidate/active transaction. Invalid or newer records
are not overwritten and do not fall back to stale legacy keys.
Legacy keys are no longer updated after migration: downgrading to older
firmware restores the older legacy settings. Wi-Fi credentials, web
authentication, IFTTT rules and RF24 storage remain in their existing layer.

### Implemented board-resource slice

- [Board profile](../include/board/board_profile.h) describes the 26 relevant
  GPIOs, ADC/DAC capabilities, input-only pins and boot-strapping warnings.
  Existing [pin constants](../include/kc868_a6_pins.h) are unchanged and now
  usable without Arduino in native tests.
- [ResourceManager](../include/board/resource_manager.h) is a bounded,
  allocation-free reservation table for GPIOs, three hardware UARTs and
  I2C addresses. Bus signals have one controller owner; SPI clients claim
  their separate chip-selects. Owner strings must outlive reservations.
- [Board settings validation](../src/board/settings_resources.cpp) runs on
  configuration load and before saving. Conflicting candidates do not
  reach NVS or hardware apply; settings requests return HTTP 409.
  Current profiles keep their existing routing and behavior.
- `/api/board` uses the existing authentication policy and exposes the
  pinout, planned reservations, validation/configuration status and
  PCF8574 addresses/port masks. The Diagnostics page renders these data
  with PL/EN headings and refreshes them at most once every five seconds.
  These are reservations, **not device detection**.

Tests cover UART exhaustion/P17 routing conflicts, PN532 SPI sharing GPIO13,
PN532 I2C conflicting with the relay expander at `0x24`, and beeper versus
DAC1. P17 host TX12/RX13 are explicit future-routing metadata; the legacy
TTL aliases and actual RS232/RS485 routing have not changed.
P17 routing, PN532 I2C and configurable DAC outputs are **not enabled**
by these validation tests. GPS mode and the coprocessor profile still use
UART1 on GPIO16/17. Strapping flags are warnings, not electrical validation.
LEDC channel allocation, RMT/I2S, dynamic resource release and hardware
reconfiguration recovery remain to be implemented.

## Capability-driven surfaces — 2026-10-31

The registry now exposes read-only binding enumeration. The initial capability slice registered three modules through `/api/capabilities`;
the later extension below increases this to six:

| Module | Properties | Actions |
| --- | --- | --- |
| `adc.main` | Four 12-bit raw ADC channels | None |
| `io.main` | Eight active-low digital inputs and six relay readbacks | `io.main/relay/set`: channel 1–6 and boolean `on` |
| `sensors.main` | Sixteen cached temperature/humidity/air/GPS/compass readings | None |

Properties carry type, unit, quality, update timestamp and error. Invalid,
disabled or unavailable values are JSON null, not success-shaped zeroes.
Sensor drivers remain in the composition root; this is a typed adapter over
their existing readings, **not complete driver extraction**. Failed fresh
BME680/ENS160/compass reads do not publish previous data as a fresh valid sample.
DHT timestamps use the last read; GPS is unavailable when its fix is older
than 30 seconds. Other reads retain the existing report cadence.

`/api/actions` accepts `{"id":"io.main/relay/set","parameters":{"channel":1,"on":true}}`.
It uses the same registry dispatcher as manual legacy relay controls and relay
IFTTT actions. Parameter types and numeric bounds are checked before execution,
including validation-only checks when saving IFTTT rules. The physical adapter
requires read-before-write and readback and preserves reserved P6/P7 high.
A readback failure reports an error even if the write was applied; it is not a
physical rollback. No relay load switching was tested in this slice.

The IO page builds module cards, property values and action forms from
descriptors. It does not maintain its own property/action whitelist. IFTTT
appends registered numeric/boolean sources and supports `action:"capability"`
with a `command` object using the same action shape. Legacy rules and controls
remain compatible. Rules still use the existing four-rule evaluator and storage;
this is not a new event engine, cron scheduler or transactional rules service.

`/api/settings/schema` exposes the shared 27-field settings schema with bounds,
allowed values and derived flags. The Settings page generates a form from it.
It is an **application settings schema**, not separate `IConfigurable` ownership
for each sensor or protocol. The existing Sensors/Interfaces forms remain.

The Screen page reads and writes `/api/screen`, with per-page grid dimensions,
property binding selectors, editable cell positions/ASCII labels and live
previews. A versioned `screen_grid` NVS value stores 1–8 pages, grids 1–4 rows
and columns, and **at most 16 widgets total** to bound memory. Duplicate cells,
unknown properties, out-of-grid positions and overlong labels are rejected
without publishing the candidate. Existing legacy screen modes remain;
new `displayMode:5` renders these bindings on SSD1306 and cycles pages using
the existing display interval. Other drivers and larger grid budgets remain
unimplemented. This parser cannot migrate removed property IDs automatically.

Layout writes use one NVS string and exact readback, with the same uncertainty
after a committed write/readback failure as device settings. A corrupt stored
layout is logged, the layout API returns 503 and interface-grid rendering shows
an error instead of silently displaying defaults. A valid POST can replace it.
NVS/hardware power-loss behavior and physical OLED rendering require board tests.

Validation: 28 native groups with ASan/UBSan, eight JavaScript tests and both
maintained firmware builds. Browser checks on an explicitly labelled synthetic
API preview covered IO/action forms, saving two pages, generating 27 settings
fields and creating a capability IFTTT rule with no page errors. This preview
does not control hardware; it is not a substitute for firmware endpoint tests.

Still pending: full modular WebGUI/page providers, remaining tab separation,
per-module configuration ownership, other peripheral/transport drivers,
relay RAM/NVS restart policy, P17/coprocessor bridge, new display controllers,
NTP/scheduler, network settings and mDNS configuration. See the
[annotated task plan](../REFACTOR_TODO.md).

## Presence, transports and output configuration

The following extension is implemented and deployed in the base A6 profile:

- **DHT11** is sensor type `3` on GPIO32/33; `0/1/2` retain off/DS18B20/DHT22.
  The DHT driver is reconstructed with the selected type, including humidity.
- **BLE presence** adds `ble.presence/in_range`. The shared nonblocking scanner
  supplies the device list and observes one persisted selected MAC.
  `/api/bluetooth/target` accepts `{"address":"AA:BB:CC:DD:EE:FF"}` or an empty
  address to disable tracking. Selection in the scan list fills the ID field;
  Save ID writes NVS and publishes only after readback. Invalid MACs are rejected.
  The property is warming-up until the first sighting, true after a sighting,
  then false after the configured timeout (default 60 seconds). Scanner-off/
  beacon modes disable valid presence. This is advertisement observation,
  not pairing or an authenticated identity; rotating private addresses cannot
  reliably identify a particular phone. Removal from range is necessarily delayed.
- **Transport** adds Wi-Fi connection state, UDP and two UART receive counters
  plus last received text, and UDP/serial send actions. UDP RX is off at port `0`.
  UART1 is unavailable to this module while GPS/coprocessor owns it; UART2
  retains the existing RS485 transceiver. Serial settings still select baud.
  LF/CRLF or a 25 ms idle gap terminates a UART frame. Payloads are bounded to
  256 bytes and validated UTF-8/printable text with TAB/CR/LF; overflow discards
  a frame with an explicit error, never truncates it silently. This is not a
  binary protocol implementation. Empty UDP datagrams cannot reliably be
  observed with this SDK's WiFiUDP API. TX confirmation means local enqueue,
  not peer receipt (`confirmed:false`).
- **IFTTT** now lists canonical module properties without legacy ADC/DI/sensor
  duplicates. Existing stored legacy rules still work. Comparison `changed`
  fires on each observed counter/value change, including successive changes;
  the first valid sample establishes a baseline. Boolean `==1`/`==0` rules
  detect BLE entry/leave and Wi-Fi connection/disconnection. The evaluator runs
  every 100 ms while the main loop is available; blocking legacy sensor/network
  operations can delay it, and bursts between evaluations are coalesced.
  Eight rules are supported, including bounded composite actions. The generic
  event engine and scheduler remain pending. Provider-specific, side-effect-free validation
  is shared by direct commands and saved generic rules, including text/host limits.
- **PWM** adds `fan.main/duty` and action `fan.main/set` with `percent:0..100`.
  Default pin GPIO25 / DAC2 / DA-2, disabled by default; default frequency 1 kHz,
  configurable 100–25,000 Hz, 8-bit LEDC. This is a **3.3 V logic PWM signal**,
  not a 0–10 V analog output, measured RPM, or AC phase/triac control.
  DA terminals have onboard LM258 conditioning. Verify terminal waveform,
  filtering and load limits before using onboard conditioning or an external
  PWM-to-0–10 V converter; neither calibrated DC output nor motor operation
  is verified. Duty starts at zero on enable/reconfiguration/restart.
  Beeper selects GPIO26/DAC1 or GPIO25/DAC2, default GPIO26; enabled overlapping
  pins/DAC reservations are rejected before persistence (HTTP 409).
  Fan and beeper use separate LEDC timers/channels (4 and 6).
- **Grid variables** support `font:0/1/2` (Classic/FreeSans9/FreeMono9),
  `textSize:1..3` and ASCII `unit` up to eight characters per widget. Empty unit
  uses the property descriptor's unit. A bounded per-cell canvas clips large
  text to its own cell, avoiding overwrite of adjacent widgets. Browser font
  rendering is an approximate preview, not pixel-identical to the GFX fonts.
- **RF24** driver, packet history and persisted replay implementation are guarded
  by `USE_RADIO_NRF24`. Both maintained profiles omit that flag and the RF24
  dependency. The lightweight status response reports `compiled:false`; UI hides
  the monitor/radio actions. Stored RF24 rules cannot transmit in this firmware.

The application schema now has 33 fields. `device_cfg` writes schema version 2;
complete version-1 records retain their 27 existing values and receive safe
defaults for new fields. Missing original fields/corrupt records still fail.
Layout schema version 1 accepts older widgets lacking the optional font/unit
fields. Older firmware cannot read version-2 settings/new widget fields; do not
expect a transparent downgrade. BLE target is a separate NVS record.

Validation includes 29 core/config/board/capability test groups, additional
BLE/transport native suites under ASan/UBSan, nine JS tests, and the exact
six-module capabilities response fitting a 24 KiB JSON workspace. Real board
API, UDP and browser checks are recorded in [deployment](./build.md#extension-deployment--2026-10-31).
Sensor hardware, BLE range transitions, UART peer delivery and oscilloscope
waveforms still require their respective attached devices.

## Settings, BLE serialization and rule UDP

Dashboard nested grids now constrain children with `min-width:0` and use two
DI columns within desktop panels; the existing mobile four-column layout
fits its wider single panel. Settings groups are a presentation layer over
the same 33-field record, not new per-module configuration ownership.

The portable BLE serializer owns temporary MAC strings, budgets 24 devices
and validates the measured output length. Its callback returns a typed result;
API failures produce HTTP 503 rather than success-shaped malformed JSON.

`UdpRules` implements lifecycle, property and trigger contracts through an
`IUdpRulesHardware` adapter. The portable codec validates saved `udpPacket`
rules and conflicts before NVS writes; the ESP32 adapter owns four WiFiUDP
sockets. Pending packet matches use the existing action dispatcher, without
changing legacy rules' edge semantics. See [configuration and limits](./peripherals.md#rule-specific-udp-triggers).

There are now **eight modules and 55 properties**. The registry's eight-module
limit is filled; a ninth requires an explicit capacity/JSON/memory review.
Native regression tests cover all eight modules' JSON budget and all 24 BLE
addresses after serialization. Validation/deployment evidence is in
[the latest deployment](./build.md#settings-ble-and-rule-udp-deployment--2026-10-31).
Project workflow context is in [LLM memory](./llm-memory.md).
This slice does not complete the full architectural plan.

## Eight independent BLE targets — v1.1.1

`BlePresence` remains one registry module, with a bounded list of eight MACs
and separate sighting/quality state. Its owned descriptor buffers expose
MAC-specific property IDs; the legacy alias retains its original MAC.
The adapter synchronizes all eight observation mailboxes independently of
the UI cache. List edits are validated before persistence/application;
unchanged MACs retain their state even when reordered.

The provider's bounded descriptor list changes on configuration edits.
Registry lookups enumerate the current provider list; WebGUI compares property
IDs as well as registry revision to refresh IO/IFTTT after edits. No ninth
module or global registry-capacity increase is required. With all eight
devices there are **63 properties**, including the compatibility alias.
The existing capabilities JSON budget passes the full eight-module/eight-MAC
native test; descriptors must not be copied or retained across configuration
changes. IFTTT stores ID strings, not pointers to mutable descriptor buffers.

The portable version-1 list codec rejects duplicates, overflow, invalid types,
unknown fields and embedded NUL addresses. The old single target migrates
when no list record exists. See [API and compatibility](./peripherals.md#saved-ble-devices--v111).
Native tests cover independent expiry, full scan cache, reorder/removal,
legacy identity and codec restore; JS tests cover add/remove/limits and
IFTTT refresh with an unchanged registry revision.
This release has local build/tests and a synthetic browser layout check,
not deployment or multi-device physical range validation.
