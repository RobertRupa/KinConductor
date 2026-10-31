# KinConductor project instructions

Firmware for KC868-A6 ESP32-WROOM: Arduino ESP32 3.3.12 / IDF 5.5.5,
PlatformIO, C++17; dependency-free JavaScript WebGUI served from LittleFS.

- Read [project memory](docs/llm-memory.md) for verified decisions/limits and
  [documentation](docs/README.md) for contracts and hardware precautions.
- Write repository documentation in English by default. Keep code identifiers,
  API fields and hardware labels unchanged; translate user-facing prose and
  update existing Markdown when touching it. Use another language only when
  explicitly requested or required for a quoted source.
- [REFACTOR_TODO.md](REFACTOR_TODO.md) contains remaining scope;
  [TODO_REFACTORED.md](TODO_REFACTORED.md) records verified completed slices.
  Move only implemented and verified work, not an entire partially done task.
- Reuse the portable contracts/registry, shared validators and versioned
  configuration codecs. Keep ESP32 dependencies in hardware adapters.
- Firmware and WebGUI must agree on API types, limits, errors and property IDs.
  Never report unavailable readings as valid zero, or failed actions as success.
- Never silently reset NVS, change partitions, infer wiring from a block diagram,
  enable an absent sensor, or enable mutually exclusive peripheral roles.
- Run `npm test` after building dependencies. Run the smallest affected tests
  first; build both maintained profiles for shared firmware changes.
  Do not run builds/dependency resolution concurrently with native compilation.
- `pio run -e kc868-a6 -e kc868-a6-coprocessor` validates firmware;
  build matching assets separately with the same profiles and `-t buildfs`.
  Native-only coprocessor checkout: `PIO_ENV=kc868-a6-coprocessor npm test`.
- Keep serial credentials, network secrets, hardware logs and generated images
  out of tracked files. Preserve unrelated worktree/editor changes.
- Hardware upload/reset requires explicit approval of board/port and safe loads.
  Use base `kc868-a6` without RF24/coprocessor for the current physical board;
  separate `upload` and `uploadfs`; never erase NVS for a routine update.
- Update directly related wiki chapters, task status and package/build metadata.
  Distinguish local builds, physical tests and actual GitHub Actions execution.
