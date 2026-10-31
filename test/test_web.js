const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const test = require("node:test");

const source = fs.readFileSync("data/app.js", "utf8");
function bleTargetsFixture(addresses) {
  return { schemaVersion: 1, capacity: 8, addresses,
    devices: addresses.map(address => ({
      address, property: `ble.presence/${address}/in_range`, in_range: false, quality: "warming_up"
    })) };
}
test("saved BLE list adds eight independent MACs, rejects duplicates/full lists and removes only selected ID", async () => {
  const elements = new Map();
  const makeElement = () => ({
    ...element(), value: "", disabled: false, listeners: {},
    addEventListener(type, callback) { this.listeners[type] = callback; }
  });
  const get = id => { if (!elements.has(id)) elements.set(id, makeElement()); return elements.get(id); };
  let addresses = [];
  let refreshes = 0;
  const posts = [];
  const context = vm.createContext({
    $: get, document: { createElement: makeElement }, t: key => key,
    window: { CapabilityUI: {
      refresh: async () => { ++refreshes; },
      request: async (url, body) => {
        assert.equal(url, "/api/bluetooth/targets");
        if (body) { posts.push(body); addresses = Array.from(body.addresses); }
        return bleTargetsFixture(addresses);
      }
    } }
  });
  const start = source.indexOf("async function loadBleTarget()");
  const finish = source.indexOf('$("#language").addEventListener', start);
  vm.runInContext(source.slice(start, finish), context);
  const add = get("#bleTargetSave").listeners.click;
  for (let i = 1; i <= 8; ++i) {
    get("#bleTarget").value = `aa:bb:cc:dd:ee:0${i}`;
    await add();
    assert.equal(addresses.length, i);
    assert.equal(get("#bleSavedCount").textContent, `${i} / 8`);
    assert.equal(get("#bleTarget").value, "");
  }
  assert.equal(refreshes, 8);
  assert.equal(get("#bleSavedTargets").children.length, 8);
  get("#bleTarget").value = "AA:BB:CC:DD:EE:01";
  await add();
  assert.equal(get("#bleTargetMessage").textContent, "bleDuplicateId");
  get("#bleTarget").value = "AA:BB:CC:DD:EE:09";
  await add();
  assert.equal(get("#bleTargetMessage").textContent, "bleTargetLimit");
  get("#bleTarget").value = "";
  await add();
  assert.equal(get("#bleTargetMessage").textContent, "bleInvalidId");
  assert.equal(posts.length, 8);
  const first = get("#bleSavedTargets").children[0];
  await first.children[1].listeners.click();
  assert.equal(addresses.length, 7);
  assert.equal(addresses[0], "AA:BB:CC:DD:EE:02");
  assert.equal(get("#bleSavedCount").textContent, "7 / 8");
  assert.equal(refreshes, 9);
  const remaining = get("#bleSavedTargets").children[0];
  assert.equal(remaining.children[0].children[0].textContent, addresses[0]);
  assert.equal(remaining.children[0].children[1].textContent, "in_range: warming_up");
  assert.throws(() => context.validateBleTargetsResponse({ ...bleTargetsFixture(addresses), devices: [] }));
  context.window.CapabilityUI.request = async () => { throw Error("storage unavailable"); };
  await context.loadBleTarget();
  assert.equal(get("#bleSavedTargets").children.length, 0);
  assert.equal(get("#bleTargetMessage").textContent, "storage unavailable");
});
const begin = source.indexOf("async function refreshBoardStatus()");
const end = source.indexOf("function renderStatus(", begin);
assert(begin !== -1 && end > begin);

function element() {
  return {
    textContent: "",
    children: [],
    append(...children) { this.children.push(...children); },
    replaceChildren(...children) { this.children = children; }
  };
}

function harness(fetch) {
  const elements = new Map(
    ["#boardMessage", "#boardPins", "#boardExpanders"].map((id) => [id, element()])
  );
  const context = vm.createContext({
    fetch,
    document: { createElement: element },
    $: (id) => elements.get(id),
    t: (key) => `translated:${key}`
  });
  vm.runInContext(
    `let lastBoardStatus = null; let boardLoading = false; let lastBoardRefreshAt = 0;\n` +
    source.slice(begin, end),
    context
  );
  return { context, elements };
}

function fixture() {
  return {
    valid: true, configurationReady: true, configurationError: "none",
    error: "none", subject: "", pinoutValidated: true, inputAddress: 0x22,
    relays: { address: 0x24, portMask: 0x3F, reservedMask: 0xC0 },
    pins: [
      { gpio: 12, role: "P17 host TX", inputOnly: false, strapping: true, adc: false, dac: false },
      { gpio: 34, role: "A3", inputOnly: true, strapping: false, adc: true, dac: false }
    ],
    claims: [{ kind: "gpio", number: 12, owner: "p17" }]
  };
}

test("board rendering uses server roles, reservations and safe text cells", async () => {
  const data = fixture();
  data.pins[0].role = "<img src=x onerror=alert(1)>";
  const { context, elements } = harness(async (url, options) => {
    assert.equal(url, "/api/board");
    assert.equal(options.cache, "no-store");
    return { ok: true, json: async () => data };
  });
  await context.refreshBoardStatus();
  const rows = elements.get("#boardPins").children;
  assert.equal(rows.length, 2);
  assert.equal(rows[0].children[1].textContent, data.pins[0].role);
  assert.equal(rows[0].children[2].textContent, "p17");
  assert.equal(rows[1].children[2].textContent, "--");
  assert.match(rows[1].children[3].textContent, /boardInputOnly.*ADC1/);
  assert.match(elements.get("#boardExpanders").textContent, /0x22.*0x24.*0x3f.*0xc0/);
  assert.equal(elements.get("#boardMessage").textContent, "translated:boardValid");
});

test("unvalidated Ax pin maps are explicitly marked as an A6 baseline", async () => {
  const data = fixture();
  data.board = "KC868-A8";
  data.pinoutValidated = false;
  const { context, elements } = harness(async () => ({ ok: true, json: async () => data }));
  await context.refreshBoardStatus();
  assert.match(elements.get("#boardMessage").textContent, /boardPinoutUnvalidated/);
});

test("header reports actual hardware including absent PSRAM and clears unavailable values", () => {
  const elements = new Map([
    "#headerCpu", "#headerRam", "#headerPsram", "#headerFlash",
    "#headerModel", "#diagnosticModel", "#brandMark", "#favicon"
  ].map((id) => [id, element()]));
  const document = { title: "", createElement: element };
  const context = vm.createContext({
    $: (id) => elements.get(id), document,
    t: (key) => key,
    encodeURIComponent
  });
  const start = source.indexOf("function renderHardware(");
  const end = source.indexOf("function renderStatus(", start);
  vm.runInContext(source.slice(start, end), context);
  context.renderHardware({
    cpu: {
      model: "ESP32", cores: 2, frequencyMHz: 240, boardModel: "KC868-A8",
      loadAvailable: true, coreLoadPercent: [24, 57]
    },
    ram: { heapBytes: 262144, freeBytes: 65536 },
    psram: { totalBytes: 0, freeBytes: 0 },
    flash: {
      totalBytes: 4194304, firmwareBytes: 1500000,
      appPartitionBytes: 1572864, freeBytes: 72288
    }
  });
  assert.match(elements.get("#headerCpu").textContent, /ESP32.*2.*240 MHz.*C0 24%.*C1 57%/);
  assert.equal(elements.get("#headerPsram").textContent, "0");
  assert.match(elements.get("#headerRam").textContent, /64 KiB.*256 KiB heap/);
  assert.match(elements.get("#headerFlash").textContent, /4 MiB.*71 KiB.*flashFreeOta.*FW/);
  assert.equal(elements.get("#headerModel").textContent, "KC868-A8");
  assert.equal(elements.get("#brandMark").textContent, "A8");
  assert.equal(document.title, "KinConductor | KC868-A8");
  assert.match(elements.get("#favicon").href, /model=KC868-A8/);
  context.renderHardware(null);
  assert.equal(elements.get("#headerCpu").textContent, "--");
});

test("unavailable config is distinguished from a valid resource plan", async () => {
  const data = fixture();
  data.configurationReady = false;
  data.configurationError = "unsupported_version";
  const { context, elements } = harness(async () => ({ ok: true, json: async () => data }));
  await context.refreshBoardStatus();
  assert.match(elements.get("#boardMessage").textContent, /boardConfigUnavailable.*unsupported_version/);
  assert.equal(elements.get("#boardPins").children.length, 2);
});

test("malformed board data and failed HTTP clear stale reservations", async () => {
  const data = fixture();
  let response = { ok: true, json: async () => data };
  const { context, elements } = harness(async () => response);
  await context.refreshBoardStatus();
  response = { ok: false };
  await context.refreshBoardStatus();
  assert.equal(elements.get("#boardPins").children.length, 0);
  assert.equal(elements.get("#boardExpanders").textContent, "--");
  assert.equal(elements.get("#boardMessage").textContent, "translated:boardError");
  response = { ok: true, json: async () => ({ ...data, pins: [{}] }) };
  await context.refreshBoardStatus();
  assert.equal(elements.get("#boardMessage").textContent, "translated:boardError");
  response = { ok: true, json: async () => fixture() };
  await context.refreshBoardStatus();
  assert.equal(elements.get("#boardPins").children.length, 2);
});

test("board requests do not overlap", async () => {
  let release;
  let requests = 0;
  const { context } = harness(() => {
    ++requests;
    return new Promise((resolve) => { release = resolve; });
  });
  const pending = context.refreshBoardStatus();
  await context.refreshBoardStatus();
  assert.equal(requests, 1);
  release({ ok: true, json: async () => fixture() });
  await pending;
  const retry = context.refreshBoardStatus();
  assert.equal(requests, 2);
  release({ ok: true, json: async () => fixture() });
  await retry;
});

test("BLE scan renders cached devices and surfaces backend failures instead of a false empty result", async () => {
  const elements = new Map(["#bluetoothScanButton", "#bluetoothScanMessage", "#bluetoothDeviceList"].map((id) => [id, element()]));
  elements.set("#interfaceSettingsForm", { elements: { bluetoothMode: { value: "1" } } });
  let response = { devices: [{ name: "Test BLE", address: "AA:BB:CC:DD:EE:FF", rssi: -45 }] };
  const context = vm.createContext({
    $: (id) => elements.get(id), t: (key) => key, document: { createElement: () => ({ ...element(), addEventListener() {} }) },
    window: { CapabilityUI: { request: async () => response } }
  });
  const start = source.indexOf("async function scanNearbyBluetooth()");
  const end = source.indexOf("async function loadBleTarget()", start);
  vm.runInContext("let nearbyBluetoothDevices=[];\n" + source.slice(start, end), context);
  await context.scanNearbyBluetooth();
  assert.equal(elements.get("#bluetoothDeviceList").children.length, 1);
  assert.equal(elements.get("#bluetoothScanMessage").textContent, "1 BLE");
  response = { error: "unavailable", field: "ble_scanner", devices: [] };
  await context.scanNearbyBluetooth();
  assert.match(elements.get("#bluetoothScanMessage").textContent, /ble_scanner/);
  assert.equal(elements.get("#bluetoothDeviceList").children.length, 0);
  assert.equal(elements.get("#bluetoothScanButton").disabled, false);
});
test("UDP rule form validates per-rule port, IPv4 and UTF-8 payload byte limits", () => {
  const values = {
    "#iftttUdpPort": "5000", "#iftttUdpRemotePort": "0", "#iftttUdpRemoteIp": "",
    "#iftttUdpText": "hello", "#iftttUdpMatch": "contains"
  };
  const context = vm.createContext({ $: (id) => ({ value: values[id] }), deviceSettings: { udpListenPort: 6000 }, TextEncoder });
  const start = source.indexOf("function udpRuleFromForm()");
  const end = source.indexOf('$("#addIftttRule")', start);
  vm.runInContext(source.slice(start, end), context);
  assert.equal(context.udpRuleFromForm().port, 5000);
  for (const [key, value] of [["#iftttUdpPort", "0"], ["#iftttUdpPort", "5353"], ["#iftttUdpPort", "6000"],
    ["#iftttUdpRemoteIp", "256.1.1.1"], ["#iftttUdpRemotePort", "-1"], ["#iftttUdpText", "é".repeat(129)]]) {
    const original = values[key]; values[key] = value;
    assert.throws(() => context.udpRuleFromForm());
    values[key] = original;
  }
});

test("RF24 monitor, diagnostics and rule options fail closed without explicit build support", async () => {
  const panels = [{ hidden: false }, { hidden: false }, { hidden: false }];
  const options = [{ value: "relay" }, { value: "rf24Send" }, { value: "rf24Saved" }];
  const sourceSelect = {
    children: [], value: "buttonS2",
    append(option) {
      option.remove = () => { this.children = this.children.filter(item => item !== option); this.value = ""; };
      this.children.push(option);
    },
    querySelector() { return this.children.find(option => option.value === "rf24Packet"); }
  };
  const messages = new Map([["#rf24Message", {}], ["#iftttMessage", {}]]);
  const context = vm.createContext({
    $: id => id === "#iftttSource" ? sourceSelect : messages.get(id),
    t: key => key, updateIftttActionFields() {}, updateIftttSourceFields() {},
    renderRf24Monitor() {}, document: {
      createElement() { return {}; },
      querySelectorAll(selector) { return selector === "[data-rf24-only]" ? panels : options; }
    }
  });
  const start = source.indexOf("function setRf24Support(");
  const end = source.indexOf("function renderRf24Monitor(", start);
  const load = source.indexOf("async function loadRf24Packets(");
  const finish = source.indexOf("async function replaySavedRf24Packet(", load);
  vm.runInContext("let rf24Compiled=false,rf24Available=false,rf24History=[],savedRf24Packets=[];\n" +
    source.slice(start, end) + source.slice(load, finish), context);
  for (const compiled of [false, undefined, null, "true"]) {
    context.setRf24Support(compiled);
    assert.ok(panels.every(panel => panel.hidden));
    assert.ok(options.slice(1).every(option => option.hidden && option.disabled));
    assert.equal(sourceSelect.children.length, 0);
  }
  context.setRf24Support(true);
  assert.ok(panels.every(panel => !panel.hidden));
  assert.equal(sourceSelect.children.length, 1);
  context.setRf24Support(true);
  assert.equal(sourceSelect.children.length, 1);
  context.setRf24Support(false);
  assert.equal(sourceSelect.children.length, 0);
  const response = { compiled: true, available: true, history: [], saved: [] };
  context.fetch = async () => ({ ok: true, json: async () => response });
  await context.loadRf24Packets();
  assert.ok(panels.every(panel => !panel.hidden));
  delete response.compiled;
  await context.loadRf24Packets();
  assert.ok(panels.every(panel => panel.hidden));
  assert.match(messages.get("#iftttMessage").textContent, /Invalid RF24 response/);
  context.fetch = async () => { throw Error("network offline"); };
  await context.loadRf24Packets();
  assert.ok(panels.every(panel => panel.hidden));
  assert.match(messages.get("#iftttMessage").textContent, /network offline/);
});
test("static settings inputs match complete backend ranges and RF24 starts hidden", () => {
  const html = fs.readFileSync("data/index.html", "utf8");
  for (const [name, min, max] of [
    ["sensorIntervalMs", 500, 60000], ["displayPageSeconds", 2, 60],
    ["beeperFrequencyHz", 250, 5000], ["beeperDurationMs", 50, 3000],
    ["udpListenPort", 0, 65535], ["fanFrequencyHz", 100, 25000],
    ["blePresenceTimeoutSeconds", 5, 3600]
  ]) {
    const tag = html.match(new RegExp(`<input[^>]*name="${name}"[^>]*>`))[0];
    assert.ok(tag.includes('type="number"') && tag.includes(`min="${min}"`) &&
      tag.includes(`max="${max}"`) && tag.includes('step="1"') && tag.includes("required"));
  }
  const radioPanels = [...html.matchAll(/<[^>]*data-rf24-only[^>]*>/g)].map(match => match[0]);
  assert.equal(radioPanels.length, 3);
  assert.ok(radioPanels.every(tag => tag.includes("hidden")));
  for (const action of ["rf24Send", "rf24Saved"]) {
    assert.match(html.match(new RegExp(`<option value="${action}"[^>]*>`))[0], /hidden disabled/);
  }
});
