const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const test = require("node:test");

function element() {
  return {
    children: [], dataset: {}, style: {}, value: "", textContent: "",
    append(...items) { for (const item of items) { item.parent = this; this.children.push(item); } },
    replaceChildren(...items) { this.children = items; },
    remove() { this.parent.children = this.parent.children.filter(item => item !== this); },
    addEventListener() {}, setAttribute() {},
    querySelectorAll(selector) { return selector === "[data-capability]" ?
      this.children.filter(item => item.dataset.capability) : []; }
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
    document, window: {}, TextEncoder, setInterval() {}, loadDeviceSettings: async () => {},
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
test("settings fields are categorized once, in stable functional order", () => {
  const { api } = harness();
  const fields = ["udpListenPort", "uart1Mode", "serial1Protocol", "serial2Route",
    "ebusWritesEnabled", "i2cClockHz", "blePresenceTimeoutSeconds",
    "bluetoothMode", "gpio32SensorType", "sensorIntervalMs", "temperature1Enabled",
    "fanPin", "beeperFrequencyHz", "s2InputEnabled", "displayMode"].map((id) => ({ id }));
  const groups = api.groupedSettings(fields);
  assert.equal(groups.map((group) => group.id).join(","), "interfaces,bluetooth,sensors,outputs,inputs,screen");
  assert.equal(groups.flatMap((group) => group.fields).length, fields.length);
  assert.equal(new Set(groups.flatMap((group) => group.fields.map((field) => field.id))).size, fields.length);
  assert.ok(groups.find((group) => group.id === "interfaces").fields.some((field) => field.id === "ebusWritesEnabled"));
});
test("UDP trigger configuration belongs to the IFTTT form, not the header", () => {
  const html = fs.readFileSync("data/index.html", "utf8");
  const form = html.match(/<form[^>]*id="iftttBuilderForm"[\s\S]*?<\/form>/)[0];
  assert.ok(form.includes('id="iftttUdpFields"'));
  assert.ok(form.includes('id="iftttUdpPort"'));
  assert.ok(!html.slice(0, html.indexOf("</header>")).includes('id="iftttUdpFields"'));
});
test("IFTTT allows eight rules and exposes bounded sequential/continue action groups", () => {
  const html = fs.readFileSync("data/index.html", "utf8");
  const script = fs.readFileSync("data/app.js", "utf8");
  const form = html.match(/<form[^>]*id="iftttBuilderForm"[\s\S]*?<\/form>/)[0];
  assert.match(form, /option value="group"/);
  assert.match(form, /id="iftttGroupAction"/);
  assert.match(form, /id="iftttGroupMode"[\s\S]*?value="sequential"[\s\S]*?value="all"/);
  assert.match(form, /id="addIftttGroupAction"/);
  assert.match(html, /id="iftttRuleCount">0 \/ 8/);
  assert.match(script, /iftttRules\.length >= 8/);
  assert.match(script, /iftttGroupActions\.length >= 8/);
});
test("IFTTT exposes each saved BLE MAC once and refreshes after descriptor changes with unchanged revision", async () => {
  const data = fixture();
  data.modules[0].properties = [
    { id: "ble.presence/in_range", type: "boolean", quality: "disabled", value: false },
    { id: "ble.presence/AA:BB:CC:DD:EE:01/in_range", type: "boolean", quality: "warming_up", value: null }
  ];
  const { api, elements } = harness(async () => ({
    ok: true, headers: { get: () => "application/json" }, json: async () => data
  }));
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(elements.get("iftttSource").children.length, 1);
  assert.equal(elements.get("iftttSource").children[0].value, data.modules[0].properties[1].id);
  assert.match(api.propertyLabel(data.modules[0].properties[1]), /AA:BB:CC:DD:EE:01.*in_range/);
  // Real HTTP responses are separate objects, not a mutation of the previous response.
  const second = structuredClone(data);
  second.modules[0].properties.push(
    { id: "ble.presence/AA:BB:CC:DD:EE:02/in_range", type: "boolean", quality: "valid", value: true }
  );
  let changingData = data;
  const changing = harness(async () => ({
    ok: true, headers: { get: () => "application/json" },
    json: async () => structuredClone(changingData)
  }));
  await new Promise(resolve => setImmediate(resolve));
  changingData = second;
  await changing.api.refresh();
  const options = changing.elements.get("iftttSource").children;
  assert.equal(options.length, 2);
  assert.deepEqual(options.map(option => option.value), [
    "ble.presence/AA:BB:CC:DD:EE:01/in_range", "ble.presence/AA:BB:CC:DD:EE:02/in_range"
  ]);
});
test("settings controls preserve full numeric ranges and render named mode choices", () => {
  const { api, elements } = harness();
  for (const [id, minimum, maximum] of [
    ["sensorIntervalMs", 500, 60000], ["displayPageSeconds", 2, 60],
    ["beeperFrequencyHz", 250, 5000], ["beeperDurationMs", 50, 3000],
    ["udpListenPort", 0, 65535]
  ]) {
    const input = api.settingsInput({ id, type: "unsigned", minimum, maximum, allowedValues: [] });
    assert.equal(input.type, "number");
    assert.equal(input.min, minimum); assert.equal(input.max, maximum);
    assert.equal(input.step, "1"); assert.equal(input.required, true);
  }
  elements.set("gpio32SensorType", { children: [
    { value: "0", textContent: "Off" }, { value: "1", textContent: "DS18B20" },
    { value: "2", textContent: "DHT22" }, { value: "3", textContent: "DHT11" }
  ] });
  const select = api.settingsInput({ id: "gpio32SensorType", type: "unsigned", minimum: 0, maximum: 3, allowedValues: [] });
  assert.deepEqual(select.children.map(option => option.textContent), ["Off", "DS18B20", "DHT22", "DHT11"]);
  const protocol = api.settingsInput({ id: "serial2Protocol", type: "unsigned", minimum: 0,
    maximum: 1, allowedValues: [0, 1] });
  assert.deepEqual(protocol.children.map(option => option.textContent), ["Raw serial", "Modbus RTU"]);
  const route = api.settingsInput({ id: "serial2Route", type: "unsigned", minimum: 0,
    maximum: 1, allowedValues: [0, 1] });
  assert.deepEqual(route.children.map(option => option.textContent), [
    "RS-485 · GPIO14/27", "P17 TTL · GPIO12/13"
  ]);
});
test("eBUS capability actions use named fields and a bounded target", () => {
  const { api } = harness();
  const action = { id: "transport.main/ebus/read", parameters: [
    { id: "host", type: "text", required: true },
    { id: "port", type: "unsigned", required: true, minimum: 1, maximum: 65535 },
    { id: "circuit", type: "text", required: true },
    { id: "name", type: "text", required: true },
    { id: "field", type: "text", required: true }
  ] };
  const controls = new Map([
    ["host", { value: "192.168.1.50" }], ["port", { value: "8888" }],
    ["circuit", { value: "bai" }], ["name", { value: "FlowTemp" }],
    ["field", { value: "value" }]
  ]);
  const command = api.commandFrom(action, controls);
  assert.equal(command.id, "transport.main/ebus/read");
  assert.equal(command.parameters.port, 8888);
  assert.equal(command.parameters.name, "FlowTemp");
  controls.get("host").value = "bad host";
  assert.throws(() => api.commandFrom(action, controls), /Invalid parameter: host/);
});
test("UDP and serial data fields support newlines with exact UTF-8 byte limits", () => {
  const { api } = harness();
  const parameter = { id: "data", type: "text", required: true };
  const control = api.parameterInput(parameter);
  assert.equal(control.rows, 3); assert.equal(control.maxLength, 256);
  const action = { id: "transport.main/serial/send", parameters: [parameter] };
  const controls = new Map([["data", { value: "hello\nworld\r\n" }]]);
  assert.equal(api.commandFrom(action, controls).parameters.data, "hello\nworld\r\n");
  controls.get("data").value = "é".repeat(128);
  assert.equal(api.commandFrom(action, controls).parameters.data.length, 128);
  controls.get("data").value = "é".repeat(129);
  assert.throws(() => api.commandFrom(action, controls), /out of range/);
  controls.get("data").value = "";
  assert.equal(api.commandFrom(action, controls).parameters.data, "");
});
