# KinConductor documentation

[Project README](../README.md)

This repository-local Markdown wiki is maintained alongside the firmware.
English is the default language for repository documentation. No
documentation build or external service is required.

## Chapters

**Start here:** [Requirements and setup](./setup.md) →
[Build profiles](./profiles.md) → [Flashing](./build.md) →
[First start](./first-start.md) → [WebGUI](./webgui.md).

1. [Requirements, installation and first build](./setup.md)
2. [Build profiles and supported configurations](./profiles.md)
3. [Build, upload and validation](./build.md)
4. [First start and web access](./first-start.md)
5. [WebGUI: pages, configuration and flashing](./webgui.md)
6. [All WebGUI settings and parameter limits](./settings-reference.md)
7. [HTTP API endpoints and JSON contracts](./api-reference.md)
8. [Troubleshooting](./troubleshooting.md)
9. [Board resources and wiring](./hardware.md)
10. [Peripherals, interfaces and automation](./peripherals.md)
11. [Architecture and refactoring](./refactoring.md)
12. [Implementation plan and acceptance criteria](../REFACTOR_TODO.md)
13. [Calibrated current sensors and loop receivers](./current-measurement.md)
14. [Manufacturer resources and compatibility](./manufacturer-sources.md)
15. [Tagged release images and CI](./releases.md)
16. [LLM project memory and instructions](./llm-memory.md)
17. [Completed refactoring work](../TODO_REFACTORED.md)

The chapters describe current behavior; the implementation plan also describes
future work. A build flag or resource reservation is not evidence that a
hardware driver, P17 link or coprocessor protocol is implemented.

## Maintenance

Finish each implementation slice by updating its chapter, the project README
when navigation or scope changes, the implementation checklist and affected
package metadata. Keep commands consistent with [platformio.ini](../platformio.ini)
and [package.json](../package.json). Preserve historical measurements as
historical evidence, not current build instructions.
