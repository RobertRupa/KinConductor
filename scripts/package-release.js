"use strict";
const fs = require("node:fs");
const path = require("node:path");
const crypto = require("node:crypto");
const { execFileSync } = require("node:child_process");
const RELEASE_PROFILES = [
  "kc868-a6-base", "kc868-a6", "kc868-a4", "kc868-a8", "kc868-a8s",
  "kc868-a16", "kc868-a32", "kc868-a64", "kc868-a128", "kc868-a6-coprocessor"
];

function parsePartitions(csv) {
  const partitions = csv.split(/\r?\n/).filter((line) => line.trim() && !line.trim().startsWith("#")).map((line) => {
    const [name, type, subtype, offset, size] = line.split(",").map((value) => value.trim());
    if (!name || !type || !subtype || !/^(0x[0-9a-f]+|\d+)$/i.test(offset) ||
        !/^(0x[0-9a-f]+|\d+)$/i.test(size)) throw new Error("Invalid explicit partition layout");
    return { name, type, subtype, offset: Number(offset), size: Number(size) };
  });
  const ordered = [...partitions].sort((a, b) => a.offset - b.offset);
  for (let i = 0; i < ordered.length; i++) {
    if (ordered[i].size <= 0 || ordered[i].offset + ordered[i].size > 4 * 1048576 ||
        (i && ordered[i - 1].offset + ordered[i - 1].size > ordered[i].offset)) {
      throw new Error("Overlapping or out-of-bounds partitions");
    }
  }
  if (new Set(partitions.map((part) => part.name)).size !== partitions.length) throw new Error("Duplicate partitions");
  return partitions;
}

function packageRelease({ root, profile, version, commit, outputRoot }) {
  if (!RELEASE_PROFILES.includes(profile)) throw new Error("Unsupported release profile");
  if (!/^\d+\.\d+\.\d+$/.test(version)) throw new Error("Version must be major.minor.patch");
  if (!/^[0-9a-f]{40}$/.test(commit)) throw new Error("Commit must be a full SHA-1");
  const metadata = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf8"));
  if (metadata.version !== version ||
      !fs.readFileSync(path.join(root, "platformio.ini"), "utf8").includes(`KINCONDUCTOR_VERSION=\\"${version}\\"`)) {
    throw new Error("Release version differs from package or firmware metadata");
  }
  const partitions = parsePartitions(fs.readFileSync(path.join(root, "partitions.csv"), "utf8"));
  const appPartitions = partitions.filter((part) => part.type === "app");
  const app = appPartitions.find((part) => part.subtype === "factory");
  const nvs = partitions.find((part) => part.name === "nvs" && part.type === "data" && part.subtype === "nvs");
  const filesystem = partitions.find((part) => part.type === "data" && part.subtype === "spiffs");
  if (appPartitions.length !== 1 || !app || !nvs || nvs.offset !== 0x9000 || nvs.size !== 0x5000 ||
      !filesystem || app.offset !== 0x10000 ||
      filesystem.offset !== app.offset + app.size || filesystem.offset + filesystem.size > 4 * 1048576 ||
      partitions.some((part) => part.type === "data" && part.subtype === "ota")) {
    throw new Error("Unsupported single-app layout or NVS region moved");
  }
  const build = path.join(root, ".pio", "build", profile);
  const firmware = fs.readFileSync(path.join(build, "firmware.bin"));
  const factory = fs.readFileSync(path.join(build, "firmware.factory.bin"));
  const littlefs = fs.readFileSync(path.join(build, "littlefs.bin"));
  const bootloader = fs.readFileSync(path.join(build, "bootloader.bin"));
  const table = fs.readFileSync(path.join(build, "partitions.bin"));
  if (!firmware.length || firmware.length > app.size) throw new Error("Firmware exceeds application partition");
  if (littlefs.length !== filesystem.size) throw new Error("LittleFS image differs from partition size");
  if (!bootloader.length || bootloader.length > 0x7000 || table.length !== 0xc00 ||
      factory.length !== app.offset + firmware.length ||
      !factory.subarray(app.offset).equals(firmware) ||
      !factory.subarray(0x1000, 0x1000 + bootloader.length).equals(bootloader) ||
      !factory.subarray(0x8000, 0x8000 + table.length).equals(table)) {
    throw new Error("Factory image does not match firmware/bootloader/partition artifacts");
  }
  const images = [
    ["firmware.bin", firmware, "application", app.offset],
    ["firmware.factory.bin", factory, "factory-without-filesystem", 0],
    ["littlefs.bin", littlefs, "filesystem", filesystem.offset],
    ["bootloader.bin", bootloader, "bootloader", 0x1000],
    ["partitions.bin", table, "partition-table", 0x8000]
  ];
  const manifest = {
    schemaVersion: 2, project: "KinConductor", version, tag: `v${version}`, commit, profile,
    flashBytes: 4 * 1048576, applicationPartitionBytes: app.size,
    firmwareBytes: firmware.length, applicationFreeBytes: app.size - firmware.length,
    rf24: false, partitions,
    files: images.map(([name, data, role, offset]) => ({
      name, role, offset, bytes: data.length,
      sha256: crypto.createHash("sha256").update(data).digest("hex")
    }))
  };
  const output = path.join(outputRoot, profile);
  fs.mkdirSync(output, { recursive: true });
  for (const [name, data] of images) fs.writeFileSync(path.join(output, name), data);
  fs.writeFileSync(path.join(output, "manifest.json"), JSON.stringify(manifest, null, 2) + "\n");
  fs.writeFileSync(path.join(output, "SHA256SUMS"), manifest.files.map((file) => `${file.sha256}  ${file.name}`).join("\n") + "\n");
  fs.writeFileSync(path.join(output, "README.txt"), [
    `KinConductor v${version} / ${profile} / ${commit}`,
    "Both matching firmware and LittleFS are required. Verify SHA256SUMS before flashing.",
    "All profiles omit RF24. Select a profile matching the intended firmware configuration.",
    "kc868-a6-base is a generic shared build configuration, not a board-specific validation profile.",
    "KC868-Ax aliases other than A6 reuse the A6 baseline pin map and are not electrically validated for those boards.",
    "The coprocessor profile configures a diagnostic UART; Zigbee/Thread stacks and the P17 protocol are not implemented.",
    "Firmware updates are serial-only; this image uses one factory app partition and has no OTA rollback slot.",
    "For one-time migration from the dual-slot layout, write-flash partitions.bin at 0x8000, firmware.bin at 0x10000, and littlefs.bin at 0x250000 in one esptool invocation. This preserves NVS at 0x9000..0xdfff; do not flash firmware.factory.bin for migration.",
    "Disconnect unsafe loads and confirm the board/serial port before a reset.",
    "Routine update: use PlatformIO upload then uploadfs, preserving NVS and partition layout.",
    "firmware.bin is the application image at the offset recorded in manifest.json; it is not an OTA update.",
    "firmware.factory.bin is merged from offset 0 and DOES NOT include LittleFS.",
    "Factory flashing can overwrite NVS padding. Use only for intentional provisioning, not routine updates.",
    "Never erase flash/NVS or flash a factory image to preserve existing credentials/calibration.",
    "littlefs.bin replaces the web-assets filesystem. Offsets and SHA256 hashes are in manifest.json.",
    "P17/custom coprocessor protocols and automatic electrical calibration are not implemented.",
    ""
  ].join("\n"));
  return { output, manifest };
}
if (require.main === module) {
  const [profile, tag] = process.argv.slice(2);
  const root = process.cwd();
  const version = (tag || JSON.parse(fs.readFileSync("package.json", "utf8")).version).replace(/^v/, "");
  const commit = execFileSync("git", ["rev-parse", "HEAD"], { encoding: "utf8" }).trim();
  const result = packageRelease({ root, profile, version, commit, outputRoot: path.join(root, "dist", "releases", `v${version}`) });
  console.log(`${result.output}: ${result.manifest.firmwareBytes} application bytes, ${result.manifest.applicationFreeBytes} application bytes free`);
}
module.exports = { parsePartitions, packageRelease, RELEASE_PROFILES };
