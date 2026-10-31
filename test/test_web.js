const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const test = require("node:test");

const source = fs.readFileSync("data/app.js", "utf8");
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
    error: "none", subject: "", inputAddress: 0x22,
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

test("header reports actual hardware including absent PSRAM and clears unavailable values", () => {
  const elements = new Map(["#headerCpu", "#headerRam", "#headerPsram", "#headerFlash"].map((id) => [id, element()]));
  const context = vm.createContext({ $: (id) => elements.get(id) });
  const start = source.indexOf("function renderHardware(");
  const end = source.indexOf("function renderStatus(", start);
  vm.runInContext(source.slice(start, end), context);
  context.renderHardware({
    cpu: { model: "ESP32", cores: 2, frequencyMHz: 240 },
    ram: { heapBytes: 262144, freeBytes: 65536 },
    psram: { totalBytes: 0, freeBytes: 0 },
    flash: { totalBytes: 4194304, firmwareBytes: 1500000 }
  });
  assert.match(elements.get("#headerCpu").textContent, /ESP32.*2.*240 MHz/);
  assert.equal(elements.get("#headerPsram").textContent, "0");
  assert.match(elements.get("#headerRam").textContent, /64 KiB.*256 KiB heap/);
  assert.match(elements.get("#headerFlash").textContent, /4 MiB/);
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
