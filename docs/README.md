# KinConductor documentation

[Project README](../README.md)

This repository-local Markdown wiki is maintained alongside the firmware.
English is the default language for repository documentation. No
documentation build or external service is required.

## Chapters

1. [Build, upload and validation](./build.md)
2. [First start and web access](./first-start.md)
3. [WebGUI: pages, configuration and flashing](./webgui.md)
4. [Board resources and wiring](./hardware.md)
5. [Peripherals, interfaces and automation](./peripherals.md)
6. [Architecture and refactoring](./refactoring.md)
7. [Implementation plan and acceptance criteria](../REFACTOR_TODO.md)
8. [Calibrated current sensors and loop receivers](./current-measurement.md)
9. [Manufacturer resources and compatibility](./manufacturer-sources.md)
10. [Tagged release images and CI](./releases.md)
11. [LLM project memory and instructions](./llm-memory.md)
12. [Completed refactoring work](../TODO_REFACTORED.md)

The chapters describe current behavior; the implementation plan also describes
future work. A build flag or resource reservation is not evidence that a
hardware driver, P17 link or coprocessor protocol is implemented.

## Maintenance

Finish each implementation slice by updating its chapter, the project README
when navigation or scope changes, the implementation checklist and affected
package metadata. Keep commands consistent with [platformio.ini](../platformio.ini)
and [package.json](../package.json). Preserve historical measurements as
historical evidence, not current build instructions.
