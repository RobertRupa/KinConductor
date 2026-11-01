const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const crypto = require("node:crypto");
const { parsePartitions, packageRelease, RELEASE_PROFILES } = require("../scripts/package-release");
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
    ["bootloader.bin", bootloader], ["partitions.bin", table], ["littlefs.bin", Buffer.alloc(0x1a0000)]]) {
    fs.writeFileSync(path.join(build, name), bytes);
  }
  return { options: { root, profile: "kc868-a6", version: "1.1.0", commit: "a".repeat(40), outputRoot: path.join(root, "dist") }, build };
}
test("release package includes all five images, exact offsets and SHA256 hashes", (t) => {
  const { options } = fixture(t);
  const { manifest, output } = packageRelease(options);
  assert.equal(manifest.schemaVersion, 2);
  assert.equal(manifest.files.length, 5);
  assert.equal(manifest.applicationPartitionBytes, 0x240000);
  assert.equal(manifest.applicationFreeBytes, 0x240000 - 2048);
  assert.deepEqual(
    manifest.partitions.find((partition) => partition.name === "nvs"),
    { name: "nvs", type: "data", subtype: "nvs", offset: 0x9000, size: 0x5000 }
  );
  for (const file of manifest.files) {
    const data = fs.readFileSync(path.join(output, file.name));
    assert.equal(data.length, file.bytes);
    assert.equal(crypto.createHash("sha256").update(data).digest("hex"), file.sha256);
  }
  assert.equal(manifest.files.find((file) => file.name === "littlefs.bin").offset, 0x250000);
  assert.equal(manifest.files.find((file) => file.name === "firmware.factory.bin").offset, 0);
  assert.ok(!manifest.files.some((file) => file.name === "boot_app0.bin"));
  assert.match(fs.readFileSync(path.join(output, "README.txt"), "utf8"), /overwrite NVS/);
});
test("release rejects oversized applications, wrong filesystem and mismatched factory inputs", (t) => {
  const { options, build } = fixture(t);
  fs.writeFileSync(path.join(build, "littlefs.bin"), Buffer.alloc(1));
  assert.throws(() => packageRelease(options), /LittleFS/);
  fs.writeFileSync(path.join(build, "littlefs.bin"), Buffer.alloc(0x1a0000));
  fs.writeFileSync(path.join(build, "firmware.bin"), Buffer.alloc(0x240001));
  assert.throws(() => packageRelease(options), /application partition/);
  fs.writeFileSync(path.join(build, "firmware.bin"), Buffer.alloc(2048, 0x99));
  assert.throws(() => packageRelease(options), /Factory image/);
});
test("release rejects version/profile mismatches and overlapping partitions", (t) => {
  const { options } = fixture(t);
  assert.throws(() => packageRelease({ ...options, version: "1.2.0" }), /metadata/);
  assert.throws(() => packageRelease({ ...options, profile: "../../other" }), /profile/);
  assert.throws(() => parsePartitions("a,data,nvs,0x9000,0x5000\nb,data,ota,0xa000,0x2000"), /Overlapping/);
  const partitionsPath = path.join(options.root, "partitions.csv");
  const partitions = fs.readFileSync(partitionsPath, "utf8").replace("0x9000,   0x5000", "0xa000,   0x4000");
  fs.writeFileSync(partitionsPath, partitions);
  assert.throws(() => packageRelease(options), /NVS region moved/);
});
test("release packages every configured PlatformIO profile", (t) => {
  const { options, build } = fixture(t);
  const { root } = options;
  const existingBuilds = new Set(["kc868-a6"]);
  for (const profile of RELEASE_PROFILES) {
    if (!existingBuilds.has(profile)) {
      fs.cpSync(build, path.join(root, ".pio", "build", profile), { recursive: true });
    }
    const result = packageRelease({ ...options, profile });
    assert.equal(result.manifest.profile, profile);
    assert.ok(fs.existsSync(path.join(result.output, "firmware.factory.bin")));
    const readme = fs.readFileSync(path.join(result.output, "README.txt"), "utf8");
    assert.match(readme, /not electrically validated/);
  }
});
test("tag-only workflow builds and publishes every profile", async () => {
  const yaml = fs.readFileSync(".github/workflows/release.yml", "utf8");
  const triggerBlock = yaml.match(/^on:\n([\s\S]*?)^permissions:/m)?.[1];
  assert.ok(triggerBlock);
  assert.match(triggerBlock, /push:\s+tags:\s+- "v\*"/);
  assert.doesNotMatch(triggerBlock, /branches:|pull_request:|workflow_dispatch:/);
  const configuredProfiles = [...fs.readFileSync("platformio.ini", "utf8").matchAll(/^\[env:([^\]]+)\]/gm)]
    .map((match) => match[1]);
  const matrixProfiles = yaml.match(/^        profile: \[(.*)\]$/m)[1].split(", ");
  assert.deepEqual(matrixProfiles, configuredProfiles);
  assert.deepEqual(RELEASE_PROFILES, configuredProfiles);
  assert.match(yaml, /name: Build \$\{\{ matrix\.profile \}\}/);
  assert.match(yaml, /needs: build/);
  assert.match(yaml, /Tag does not match package version/);
  assert.match(yaml, /gh release upload "\$TAG" release-assets\/\*\.tar\.gz --clobber/);
  assert.match(yaml, /gh release edit "\$TAG" --draft=false/);
  const block = yaml.match(/          script: \|\n([\s\S]*?)      - name: Publish/)[1];
  const script = block.split("\n").map(line => line.replace(/^            /, "")).join("\n");
  const execute = new (Object.getPrototypeOf(async function () {}).constructor)(
    "github", "context", "core", "process", script
  );
  let created = 0;
  let outcome = 404;
  const github = { rest: { repos: {
    getReleaseByTag: async () => {
      if (outcome) throw Object.assign(new Error("GitHub request failed"), { status: outcome });
      return { data: { id: 42 } };
    },
    createRelease: async options => {
      assert.equal(options.tag_name, "v1.1.2");
      assert.equal(options.draft, true);
      ++created;
    }
  } } };
  const context = { repo: { owner: "test", repo: "firmware" } };
  const core = { info() {} }, process = { env: { TAG: "v1.1.2" } };
  await execute(github, context, core, process);
  assert.equal(created, 1);
  outcome = 0;
  await execute(github, context, core, process);
  assert.equal(created, 1);
  for (outcome of [403, 500]) {
    await assert.rejects(execute(github, context, core, process), /GitHub request failed/);
    assert.equal(created, 1);
  }
});
