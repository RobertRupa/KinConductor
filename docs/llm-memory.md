# Project memory for future LLM work

[Index](./README.md) · [Instructions](../AGENTS.md) ·
[Completed work](../TODO_REFACTORED.md) · [Remaining specification](../REFACTOR_TODO.md)

This is version-controlled engineering context, not an automatic runtime memory
store. Update facts when verified; do not record passwords, tokens, private
network names, serial output or unverified assumptions.

Repository documentation defaults to English. Keep code/API identifiers,
commands and source quotations unchanged; write documentation updates in
English unless another language is explicitly requested.

## Stable decisions

- Base and coprocessor are the only maintained profiles; explicit base for a
  board without a coprocessor. The coprocessor build is a UART1 diagnostic
  default, **not implemented P17/NCP/RCP/custom-bridge support**.
- Official pioarduino platform and tested libraries are pinned in PlatformIO.
  No npm dependencies; `npm test` uses native ASan/UBSan and Node's test runner.
  Fresh checkout needs a firmware build to acquire ArduinoJson first.
- GPIO/P17 roles are not extra hardware UART controllers. MCU UART1/2 routing
  replaces another role. GPIO12 straps boot; GPIO13 conflicts with PN532 CS.
  Manufacturer RS232 naming conflicts with the preserved runtime convention.
- Supplied schematic shows all four conditioned analog inputs as 0–5 V.
  Do not assume A3/A4 have current-loop shunts. DA terminals include LM258
  conditioning, distinct from GPIO voltages. Verify actual revision/waveform.
- Current scaling is two-point linear, disabled and uncalibrated by default;
  not raw-waveform AC RMS, automatic calibration or overcurrent protection.
- Default hostname is **kinconductor**, without `.local`; mDNS adds the suffix.
- Routine firmware and LittleFS updates are separate operations, preserve NVS,
  and require safe-load/port approval. Factory images contain NVS padding and
  initial OTA metadata and are not NVS-preserving routine updates.

## Implementation map and limits

- Portable contracts/registry: `include/core`, `src/core`.
  Composition/hardware scheduling still partly in `src/main.cpp`.
- Current providers: ADC, board IO, sensor cache, fan PWM, BLE presence,
  Wi-Fi/UDP/serial transport, calibrated current and rule-specific UDP receivers.
  Registry limit is eight modules: adding a ninth requires an explicit
  capacity/memory review, not an unchecked registration.
- Main settings: `ConfigService`, complete record migration, schema API.
  Separate NVS records: OLED layout, current calibration, hostname, BLE target,
  IFTTT rules. IFTTT remains limited to four rules.
- Rule UDP: distinct local ports, share one socket for duplicate ports,
  optional sender IP/port and exact/contains text. Maximum four listeners,
  256 UTF-8 bytes, bounded queued matches. Global UDP port must not collide.
- BLE has one shared nonblocking scanner, bounded 24-device UI cache and up
  to eight independent saved MACs. IDs are `ble.presence/<MAC>/in_range`.
  The legacy alias retains the original migrated MAC, not list position;
  removal disables it, never retargets it. A healthy empty scan is not failure.
  `ble_targets` is the version-1 list record; old `ble_target` imports only
  when the list is absent. New IFTTT selectors omit the generic alias.
- Dynamic IO/actions/Settings/OLED use descriptors. Settings groups are UI
  presentation; the API remains a flat record. Do not duplicate configuration.
- Firmware-only upload leaves stale WebGUI; asset-only upload leaves stale APIs.
  API HTML must be reported as a mismatched endpoint, not a JSON parser crash.

## Failure lessons and validation

- ArduinoJson with a `const char *` can borrow storage. A MAC buffer created
  inside a loop must be copied before its scope ends. Native tests must check
  every address after the serializer returns, not just device count.
- Native and ESP32 JSON slots have different sizes. Use `JSON_OBJECT_SIZE`
  with an explicit copied-string budget and test the actual maximum modules.
- Growing Arduino `String` while a large JSON pool is alive can truncate a
  response under fragmentation. Reserve `measureJson` once and check the exact
  serialized length. A locally successful build does not prove hardware heap.
- In dashboard nested grids, `min-width: 0` and `minmax(0, 1fr)` prevent
  content-derived overflow; choose DI column count for the actual inner panel.
  Assert new controls belong to their intended form, not just that IDs exist.
- Version 1.1.1 base/coprocessor binaries: 1,543,376 / 1,543,552 bytes
  in a 1,572,864-byte OTA slot; static RAM 78,448 bytes. Earlier hardware tests
  reached about 15 KiB minimum heap; v1.1.1 was not uploaded. These are not worst-case budgets;
  added drivers require a new flash/heap/capabilities-response review.
- Hostname HTML patterns use Unicode-sets `v` in modern browsers; escape hyphens.
- Builds modify `.pio/libdeps`; native compilation reading those headers must
  not run concurrently with dependency resolution.
- [release workflow](../.github/workflows/release.yml) exists: branch/PR/manual
  validation and tag-only releases. Tags must match package/firmware versions;
  successful matrix builds precede draft/assets/publication, reruns reuse the
  release and clobber its matching assets. Local workflow checks do not establish an
  actual successful GitHub run. Publication requires a permitted environment.
- Release v1.1.1 follows the immutable v1.1.0 tag. Do not move or overwrite
  existing release tags without an explicit ask.
- v1.1.2 changes only WebGUI/release behavior: RF24 sections fail closed until
  explicit build support, incomplete numeric presets replaced by backend ranges,
  mode settings use named choices, transport data textarea uses LF, grid bounds
  refresh after resize. Both maintained builds still omit RF24. Firmware size
  is unchanged from v1.1.1; this version also has no physical deployment.
- [WebGUI wiki chapter](./webgui.md) has screenshots re-captured in English
  (EN selected) from the reachable KC868-A6 running firmware 1.1.0 on
  2026-10-31; device API endpoints returned HTTP 200. Screenshot-only event
  translations and redactions were local to the browser; no device settings
  were changed. Network identifiers, BLE MAC and UDP port are redacted. These
  live screenshots are historical and may not match the repository's newer
  1.1.2 assets; flash firmware and matching LittleFS before using them as
  current UI acceptance references.

## Documentation map for new maintainers

- [Setup](./setup.md) is the reproducible host prerequisites / PlatformIO
  installation / initial build / two-step upload guide.
- [Profiles](./profiles.md) separates maintained A6 builds from unvalidated
  KC868-Ax aliases and conditional compilation.
- [Settings](./settings-reference.md) records the 33 field names/defaults/ranges
  from `src/config/device_settings.cpp`; compare against runtime schema.
- [API](./api-reference.md) enumerates routes from `src/wifi_web_ui.cpp` and
  JSON contracts; [Troubleshooting](./troubleshooting.md) maps common failures.
- These Markdown pages are a **repository-local wiki** under `docs/`.
  The separate GitHub-hosted Wiki is not automatically synchronized.
