---
name: kinconductor-peripheral
description: "Use when adding or refactoring KinConductor KC868-A6 peripherals, typed properties, IFTTT triggers/actions, NVS configuration, ESP32 adapters, WebGUI forms or OLED bindings."
---

# Implement a KinConductor capability

1. Read [project memory](../../../docs/llm-memory.md) and the relevant chapter in
   [documentation](../../../docs/README.md). Find remaining work in
   [the active specification](../../../REFACTOR_TODO.md).
2. Trace the current composition in `src/main.cpp`, portable interfaces in
   `include/core`, provider/codec implementations in `src/peripherals` and
   `src/config`, API wiring in `src/wifi_web_ui.cpp`, and descriptor-driven UI in
   `data/capabilities.js`. Reuse a validator rather than adding a second rule.
3. Define bounds, status/quality, startup state, ownership and pin/bus conflicts
   before editing. Clarify unsafe hardware and ambiguous behavior with the user.
4. Implement lifecycle and applicable property/trigger/action contracts without
   Arduino dependencies in the portable model. Use an ESP32 hardware adapter.
   Register the binding once. Check registry limits and JSON pool size on the
   target architecture, not just native host sizes.
5. Version persistent records and validate a candidate before writing/applying.
   Check NVS write/readback; surface errors and provide explicit repair instead
   of a silent reset or fabricated defaults.
6. Wire configuration/API, IO, IFTTT, OLED and WebGUI consistently. Preserve
   existing IDs/record migration. Use PL/EN labels, bounded inputs and error text.
   Avoid form resets while users edit. Do not retain pointers to temporary
   strings in ArduinoJson; copy short-lived addresses/labels.
7. Add focused native tests for bounds/types/errors/recovery and Node tests for
   output shape. Run `npm test`, both firmware builds and matching `buildfs`
   sequentially. Test response allocation/length and runtime heap on hardware
   only after explicit upload approval.
8. Verify browser layout at 390, 860 and 1200 px: child cells stay inside panels,
   controls are accessible, content does not overlap, all fields are reachable.
   A DOM click is not proof of pointer/accessibility behavior.
9. Restore hardware test configuration, stop local helper processes, and remove
   credential-bearing scratch files. Update user docs and project memory.
   Record only verified completed work in `TODO_REFACTORED.md`; keep unresolved
   acceptance criteria and hardware/remote-CI blockers in `REFACTOR_TODO.md`.
