const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const crypto = require("node:crypto");
const { parsePartitions, packageRelease } = require("../scripts/package-release");
function fixture(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "kinconductor-release-"));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  fs.copyFileSync("partitions.csv", path.join(root, "partitions.csv"));
  fs.writeFileSync(path.join(root, "package.json"), '{"version":"1.1.0"}');
  fs.writeFileSync(path.join(root, "platformio.ini"), '-DKINCONDUCTOR_VERSION=\\"1.1.0\\"');
  const build = path.join(root, ".pio/build/kc868-a6");
  fs.mkdirSync(build, { recursive: true });
  const firmware = Buffer.alloc(2048, 0xe9), bootloader = Buffer.alloc(2048, 0x11), table = Buffer.alloc(0xc00, 0x22);
  const factory = Buffer.alloc(0x10000 + firmware.length, 0xff);
  firmware.copy(factory, 0x10000); bootloader.copy(factory, 0x1000); table.copy(factory, 0x8000);
  for (const [name, bytes] of [["firmware.bin", firmware], ["firmware.factory.bin", factory],
    ["bootloader.bin", bootloader], ["partitions.bin", table], ["littlefs.bin", Buffer.alloc(0xe0000)]]) {
    fs.writeFileSync(path.join(build, name), bytes);
  }
  return { options: { root, profile: "kc868-a6", version: "1.1.0", commit: "a".repeat(40), outputRoot: path.join(root, "dist") }, build };
}
test("release package includes all six images, exact offsets and SHA256 hashes", (t) => {
  const { options } = fixture(t);
  const { manifest, output } = packageRelease(options);
  assert.equal(manifest.files.length, 6);
  assert.equal(manifest.otaSlotBytes, 0x180000);
  assert.equal(manifest.otaRemainingBytes, 0x180000 - 2048);
  for (const file of manifest.files) {
    const data = fs.readFileSync(path.join(output, file.name));
    assert.equal(data.length, file.bytes);
    assert.equal(crypto.createHash("sha256").update(data).digest("hex"), file.sha256);
  }
  assert.equal(manifest.files.find((file) => file.name === "littlefs.bin").offset, 0x310000);
  assert.equal(manifest.files.find((file) => file.name === "firmware.factory.bin").offset, 0);
  assert.match(fs.readFileSync(path.join(output, "README.txt"), "utf8"), /overwrite NVS/);
});
test("release rejects oversized applications, wrong filesystem and mismatched factory inputs", (t) => {
  const { options, build } = fixture(t);
  fs.writeFileSync(path.join(build, "littlefs.bin"), Buffer.alloc(1));
  assert.throws(() => packageRelease(options), /LittleFS/);
  fs.writeFileSync(path.join(build, "littlefs.bin"), Buffer.alloc(0xe0000));
  fs.writeFileSync(path.join(build, "firmware.bin"), Buffer.alloc(0x180001));
  assert.throws(() => packageRelease(options), /OTA slot/);
  fs.writeFileSync(path.join(build, "firmware.bin"), Buffer.alloc(2048, 0x99));
  assert.throws(() => packageRelease(options), /Factory image/);
});
test("release rejects version/profile mismatches and overlapping partitions", (t) => {
  const { options } = fixture(t);
  assert.throws(() => packageRelease({ ...options, version: "1.2.0" }), /metadata/);
  assert.throws(() => packageRelease({ ...options, profile: "../../other" }), /profile/);
  assert.throws(() => parsePartitions("a,data,nvs,0x9000,0x5000\nb,data,ota,0xa000,0x2000"), /Overlapping/);
});
