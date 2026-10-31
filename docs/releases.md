# Release images and GitHub Actions

[Documentation index](./README.md) · [Project README](../README.md)

## Build inputs and local validation

Current version/tag: **1.1.2 / v1.1.2**, with RF24 build-aware visibility,
corrected WebGUI controls and tag-release retry support. Local firmware,
filesystem and tests are verified; hardware upload and remote publication
were not performed for this tag.

Version 1.1.0 pins PlatformIO Core 6.2.0 in CI, the official pioarduino
`55.03.312-1` platform release (Arduino ESP32 3.3.12 / IDF 5.5.5),
and the tested library versions in [platformio.ini](../platformio.ini).
It no longer depends on a machine-local custom platform definition.
Node 18+ runs the dependency-free tests/packager; CI uses Node 22/Python 3.12.

```sh
pio run -e kc868-a6 -e kc868-a6-coprocessor
npm test
pio run -e kc868-a6 -e kc868-a6-coprocessor -t buildfs
npm run package:release -- kc868-a6 v1.1.2
npm run package:release -- kc868-a6-coprocessor v1.1.2
```

Run builds sequentially with tests: dependency-resolution changes may replace
`.pio/libdeps` while the native runner reads headers. For a checkout built
only with the coprocessor profile, use
`PIO_ENV=kc868-a6-coprocessor npm test`.

Outputs are under ignored `dist/releases/v1.1.2/<profile>/`:

| Image | Purpose / serial offset |
| --- | --- |
| `firmware.bin` | Application / app0 `0x10000` |
| `firmware.factory.bin` | Padded merged factory image / `0x0`; excludes LittleFS |
| `littlefs.bin` | Matching WebGUI filesystem / `0x310000` |
| `bootloader.bin` | Bootloader / `0x1000` |
| `partitions.bin` | Partition table / `0x8000` |
| `boot_app0.bin` | Initial OTA-data block extracted from the verified factory image / `0xe000` |

`manifest.json` records project version/tag, full source commit, profile,
partition layout, image lengths, SHA256 hashes and exact remaining OTA bytes.
`SHA256SUMS` and `README.txt` accompany every package. The packager rejects
oversized applications, wrong LittleFS lengths, unsupported/overlapping
partition layouts, mismatched factory components and version/profile mismatches.
Package only a clean, committed build when publishing a release: local checks
before the commit do not establish final source provenance.

**Routine updates should use separate PlatformIO `upload` and `uploadfs` with
the matching environment and unchanged partition table, preserving NVS.**
Disconnect unsafe loads and confirm the target port first.
An OTA application uses the updater's selected inactive partition; the serial
app0 offset is not an instruction to write a running image into an inactive slot.
Factory padding spans NVS and OTA metadata: factory flashing may destroy
credentials/calibration and is reserved for deliberate initial provisioning.
Do not use erase-flash or factory flashing to preserve settings.

## CI triggers and release publication

[release.yml](../.github/workflows/release.yml) runs on:

- A pushed `v*` tag: both matrix builds and all tests must pass before the
  release job publishes two `.tar.gz` archives.
- A branch push or pull request: both profiles build and tests run; no release.
- Manual **Actions → Validate and release firmware → Run workflow** on a branch:
  validates and uploads build artifacts, but does not create a release.
  A manual dispatch explicitly using a matching tag ref can rerun publication.

Tags must be `v<major>.<minor>.<patch>` and match package and firmware versions.
The build job has read-only contents permission; only the successful tagged
release job receives `contents: write`. The workflow fetches tags, rejects
package/tag mismatches before the expensive build, and publishes only after
both profile builds/tests/filesystems/packages succeed. Releases are first
created as drafts, assets uploaded, then published. Repeated runs reuse the
release and replace matching archives (`--clobber`); permission/network failures
are not mistaken for a missing release. Concurrent runs of the same ref are
serialized without cancelling an in-progress release.

For a future authorized publication from your own environment:

```sh
git remote add origin https://github.com/RobertRupa/KinConductor.git
git push --atomic origin HEAD v1.1.2
```

Create the repository and verify its licensing/contents before those commands;
omit `remote add` if the remote is already configured.
Inspect both build jobs and the release job in Actions, then download the
archives and verify `sha256sum -c SHA256SUMS` inside each extracted profile.
The tag-triggered run, not a local compile, is the GitHub build acceptance test.

**Current publication status:** the workflow, local images and packaging have
been validated locally. This session cannot transmit repository code to an
external service; no repository push, published GitHub release or actual
GitHub Actions run is claimed.
