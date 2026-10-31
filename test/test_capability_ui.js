const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const test = require("node:test");

function element() {
  return {
    children: [], dataset: {}, style: {}, value: "", textContent: "",
    append(...items) { this.children.push(...items); },
    replaceChildren(...items) { this.children = items; },
    addEventListener() {}, setAttribute() {}, querySelectorAll() { return []; }
  };
}
function fixture() {
  return {
    schemaVersion: 1, revision: 1,
    modules: [{
      id: "fake.module", type: "test", ready: true,
      properties: [{ id: "fake.module/value", type: "unsigned", quality: "valid", value: 4095 }],
      actions: [{
        id: "fake.module/set",
        parameters: [{ id: "channel", type: "unsigned", required: true, minimum: 1, maximum: 6 },
          { id: "on", type: "boolean", required: true }]
      }], triggers: []
    }]
  };
}
function harness(fetcher) {
  const elements = new Map();
  const document = {
    documentElement: { lang: "en" },
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, element());
      return elements.get(id);
    },
    querySelectorAll() { return []; },
    createElement: element
  };
  const context = vm.createContext({
    document, window: {}, setInterval() {}, loadDeviceSettings: async () => {},
    fetch: fetcher || (async () => ({ ok: true, headers: { get: () => "application/json" }, json: async () => fixture() }))
  });
  vm.runInContext(fs.readFileSync("data/capabilities.js", "utf8"), context);
  return { api: context.window.CapabilityUI, elements };
}
test("capability-driven modules render from descriptors without hardcoded IDs", async () => {
  const { api, elements } = harness();
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(api.properties(fixture())[0].id, "fake.module/value");
  assert.equal(elements.get("capabilityModules").children.length, 1);
  assert.equal(elements.get("iftttSource").children[0].value, "fake.module/value");
  assert.equal(elements.get("iftttCapabilityAction").children[0].value, "fake.module/set");
  assert.throws(() => api.validateCapabilities({ modules: [] }), /Invalid capabilities/);
});
test("typed action UI validates booleans, numeric bounds and builds API command", () => {
  const { api } = harness();
  const action = fixture().modules[0].actions[0];
  const controls = new Map([["channel", { value: "6" }], ["on", { checked: false }]]);
  const command = api.commandFrom(action, controls);
  assert.equal(command.id, "fake.module/set");
  assert.equal(command.parameters.channel, 6);
  assert.equal(command.parameters.on, false);
  for (const value of ["", "0", "7", "1.5", "-1", "4294967296", "NaN"]) {
    controls.get("channel").value = value;
    assert.throws(() => api.commandFrom(action, controls));
  }
});
test("screen grids validate page counts, references, bounds and collisions", () => {
  const { api } = harness();
  const layout = {
    schemaVersion: 1, pages: [{ rows: 2, columns: 2 }],
    widgets: [{ page: 0, row: 1, column: 1, property: "fake.module/value", label: "A1" }]
  };
  const props = api.properties(fixture());
  assert.equal(api.validateLayout(layout, props), layout);
  assert.throws(() => api.validateLayout({ ...layout, pages: [] }, props));
  assert.throws(() => api.validateLayout({ ...layout, pages: [{ rows: 5, columns: 1 }] }, props));
  assert.throws(() => api.validateLayout({ ...layout, widgets: [layout.widgets[0], layout.widgets[0]] }, props));
  assert.throws(() => api.validateLayout({ ...layout, widgets: [{ ...layout.widgets[0], property: "missing" }] }, props));
  assert.throws(() => api.validateLayout({ ...layout, widgets: [{ ...layout.widgets[0], label: "non-ascii: \u00e9" }] }, props));
  assert.equal(api.validateLayout({ ...layout, widgets: [{ ...layout.widgets[0], font: 2, textSize: 3, unit: "rpm" }] }, props).widgets[0].unit, "rpm");
  for (const fields of [{ font: 3 }, { textSize: 0 }, { textSize: 1.5 }, { unit: "012345678" }]) {
    assert.throws(() => api.validateLayout({ ...layout, widgets: [{ ...layout.widgets[0], ...fields }] }, props));
  }
});
test("canonical IFTTT sources remove legacy duplicate inputs and sensors", () => {
  const { api } = harness();
  const html = fs.readFileSync("data/index.html", "utf8");
  const source = html.match(/<select id="iftttSource">([\s\S]*?)<\/select>/)[1];
  assert.doesNotMatch(source, /temperature1|temperature2|digital\d|analog\d|bmeTemperature|airQuality|humidity/);
  assert.equal(api.propertyLabel({ id: "adc.main/channel/1/raw" }), "AI 1 · ADC");
  assert.equal(api.propertyLabel({ id: "io.main/input/1" }), "DI 1");
  for (const id of ["gpio32SensorType", "gpio33SensorType"]) {
    assert.match(html.match(new RegExp(`<select id="${id}"[^>]*>([\\s\\S]*?)<\\/select>`))[1], /value="3">DHT11/);
  }
});
test("HTML API response reports missing firmware API instead of JSON parser error", async () => {
  let parsed = false;
  const { api } = harness(async () => ({
    ok: false, status: 404, headers: { get: () => "text/html" },
    json: async () => { parsed = true; throw new Error("Unexpected token <"); }
  }));
  await assert.rejects(api.request("/api/settings/schema"), /No JSON API.*firmware/);
  assert.equal(parsed, false);
});
test("current configuration requires real finite, ordered endpoints", () => {
  const { api } = harness();
  const channel = { signal: 2, calibrated: false, rawLow: 1000, rawHigh: 3000,
    signalLow: 4, signalHigh: 20, currentLow: 0, currentHigh: 20 };
  const config = { schemaVersion: 1, channels: Array.from({ length: 4 }, () => ({ ...channel })) };
  assert.equal(api.validateCurrent(config), config);
  for (const invalid of [{ rawLow: 3000 }, { rawHigh: 4096 }, { signalLow: NaN },
    { signalHigh: 25 }, { calibrated: 1 }, { currentHigh: 0 }, { signal: 3 }]) {
    assert.throws(() => api.validateCurrent({ ...config, channels: [{ ...channel, ...invalid }, ...config.channels.slice(1)] }));
  }
  assert.throws(() => api.validateCurrent({ ...config, channels: [] }));
});
test("hostname HTML pattern escapes the hyphen for browser Unicode sets", () => {
  const html = fs.readFileSync("data/index.html", "utf8");
  const pattern = html.match(/id="hostname"[^>]*pattern="([^"]+)"/)[1];
  assert.ok(pattern.includes("\\-"));
  const regex = new RegExp(`^(?:${pattern})$`, "u");
  assert.ok(regex.test("kinconductor") && regex.test("KC868-A6"));
  assert.ok(!regex.test("a.local") && !regex.test("-host") && !regex.test("host-"));
});
