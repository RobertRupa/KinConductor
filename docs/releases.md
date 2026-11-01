# Release images and GitHub Actions

[Documentation index](./README.md) · [Project README](../README.md)

## Build inputs and local validation

Release version/tag: **1.1.4 / v1.1.4**, updating CI to run only on pushed
version tags and to build/package every configured PlatformIO profile in an
independent job. The A4/A8/A8S/A16/A32/A64/A128 aliases remain A6-baseline
builds, not electrically validated profiles for those boards.

The working tree prepares **1.1.5** with a single factory application
partition and a larger LittleFS partition. There is no firmware OTA updater or
rollback slot. The partition migration keeps NVS at offset `0x9000`, size
`0x5000`, but moves LittleFS; it has not been physically tested on a board.

Version 1.1.0 pins PlatformIO Core 6.2.0 in CI, the official pioarduino
`55.03.312-1` platform release (Arduino ESP32 3.3.12 / IDF 5.5.5),
and the tested library versions in [platformio.ini](../platformio.ini).
It no longer depends on a machine-local custom platform definition.
Node 18+ runs the dependency-free tests/packager; CI uses Node 22/Python 3.12.

```sh
npm run build:all
npm test
npm run build:fs:all
for profile in kc868-a6-base kc868-a6 kc868-a4 kc868-a8 kc868-a8s kc868-a16 kc868-a32 kc868-a64 kc868-a128 kc868-a6-coprocessor; do
  npm run package:release -- "$profile" v1.1.5
done
```

Run builds sequentially with tests: dependency-resolution changes may replace
`.pio/libdeps` while the native runner reads headers. For a checkout built
only with the coprocessor profile, use
`PIO_ENV=kc868-a6-coprocessor npm test`.

Outputs are under ignored `dist/releases/v1.1.5/<profile>/`:

| Image | Purpose / serial offset |
| --- | --- |
| `firmware.bin` | Application / factory partition `0x10000` |
| `firmware.factory.bin` | Padded merged factory image / `0x0`; excludes LittleFS |
| `littlefs.bin` | Matching WebGUI filesystem / `0x250000` |
| `bootloader.bin` | Bootloader / `0x1000` |
| `partitions.bin` | Partition table / `0x8000` |

`manifest.json` schema v2 records project version/tag, full source commit,
profile, partition layout, image lengths, SHA256 hashes and free application
partition bytes.
`SHA256SUMS` and `README.txt` accompany every package. The packager rejects
oversized applications, wrong LittleFS lengths, unsupported/overlapping
partition layouts, mismatched factory components and version/profile mismatches.
Package only a clean, committed build when publishing a release: local checks
before the commit do not establish final source provenance.

**Firmware updates are serial-only; there is no OTA update or rollback slot.**
For the one-time migration from the v1.1.4 partition layout, upload firmware
and its partition table, then upload the matching LittleFS image as separate
PlatformIO operations. NVS offset/size remain unchanged. For routine updates
after migration, use separate `upload` and `uploadfs` operations and preserve
the partition table. Disconnect unsafe loads and confirm the target port first.
Factory images span NVS and may destroy credentials/calibration; they are not
routine NVS-preserving updates. Never erase flash to perform this migration.

## CI triggers and release publication

[release.yml](../.github/workflows/release.yml) runs on:

- A pushed `v*` tag only. Branch pushes, pull requests and manual dispatches do
  not run this workflow. The tag must match the package and firmware version.
- Ten independent matrix jobs build the ten configured PlatformIO environments:
  `kc868-a6-base`, `kc868-a6`, `kc868-a4`, `kc868-a8`, `kc868-a8s`,
  `kc868-a16`, `kc868-a32`, `kc868-a64`, `kc868-a128` and
  `kc868-a6-coprocessor`. Each job builds firmware and matching LittleFS,
  runs tests and packages a profile-specific archive.
- After all ten jobs succeed, the tagged GitHub release contains all ten
  `.tar.gz` profile archives.
  The A4/A8/A8S/A16/A32/A64/A128 aliases reuse the A6 baseline pin map and are
  not electrically validated for those boards; `kc868-a6-base` is a generic
  shared build configuration. Read each packaged README before flashing.

Tags must be `v<major>.<minor>.<patch>` and match package and firmware versions.
The build job has read-only contents permission; only the successful tagged
release job receives `contents: write`. The workflow fetches tags, rejects
package/tag mismatches before the expensive build, and publishes only after
all ten profile builds/tests/filesystems/packages succeed. Releases are first
created as drafts, assets uploaded, then published. Repeated runs reuse the
release and replace matching archives (`--clobber`); permission/network failures
are not mistaken for a missing release. Concurrent runs of the same ref are
serialized without cancelling an in-progress release.

For a future authorized publication from your own environment:

```sh
git remote add origin https://github.com/RobertRupa/KinConductor.git
git push --atomic origin HEAD v1.1.5
```

Create the repository and verify its licensing/contents before those commands;
omit `remote add` if the remote is already configured.
Inspect both build jobs and the release job in Actions, then download the
archives and verify `sha256sum -c SHA256SUMS` inside each extracted profile.
The tag-triggered run, not a local compile, is the GitHub build acceptance test.

**Publication status:** `v1.1.4` is the latest published tag. Version `1.1.5`
is being prepared locally; its builds, tests and packages must pass before that
tag is pushed. GitHub Actions publishes the release only after all ten profile
jobs succeed.
