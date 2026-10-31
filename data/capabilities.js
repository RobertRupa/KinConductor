(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const text = (pl, en) => document.documentElement.lang === "en" ? en : pl;
  let capabilities = null;
  let layout = null;
  let loading = false;
  let lastRefresh = 0;
  let actionControls = new Map();
  let settingsControls = new Map();
  let schema = null;
  let currentConfig = null;
  let currentControls = [];
  const parameterLabels = {
    channel: ["Kanał", "Channel"], on: ["Włączony", "On"],
    percent: ["Wysterowanie (%)", "Duty (%)"], host: ["Adres docelowy", "Destination"],
    port: ["Port", "Port"], data: ["Dane", "Data"]
  };
  function propertyLabel(prop) {
    const adc = prop.id.match(/^adc\.main\/channel\/(\d+)\/raw$/);
    const io = prop.id.match(/^io\.main\/(input|relay)\/(\d+)$/);
    if (adc) return `AI ${adc[1]} · ADC`;
    if (io) return `${io[1] === "input" ? "DI" : text("Przekaźnik", "Relay")} ${io[2]}`;
    const sensor = prop.id.match(/^sensors\.main\/(temperature|humidity)\/([12])$/);
    if (sensor) return `${sensor[1] === "temperature" ? text("Temperatura", "Temperature") :
      text("Wilgotność", "Humidity")} · GPIO${sensor[2] === "1" ? "32" : "33"}`;
    const labels = {
      "ble.presence/in_range": ["BLE · w zasięgu (1/0)", "BLE · in_range (1/0)"],
      "transport.main/wifi/connected": ["Wi-Fi · połączone (1/0)", "Wi-Fi · connected (1/0)"],
      "transport.main/udp/rx/count": ["UDP · licznik odbioru", "UDP · receive count"],
      "transport.main/serial/1/rx/count": ["RS232 · licznik odbioru", "RS232 · receive count"],
      "transport.main/serial/2/rx/count": ["RS485 · licznik odbioru", "RS485 · receive count"],
      "fan.main/duty": ["PWM · wysterowanie (%)", "PWM · duty (%)"]
    };
    if (labels[prop.id]) return text(...labels[prop.id]);
    const ble = prop.id.match(/^ble\.presence\/([0-9A-F:]{17})\/in_range$/);
    if (ble) return `BLE · ${ble[1]} · in_range (1/0)`;
    const current = prop.id.match(/^current\.main\/channel\/(\d+)\/(amperes|signal)$/);
    if (current) return `A${current[1]} · ${current[2] === "amperes" ? text("prąd (A)", "current (A)") : text("sygnał", "signal")}`;
    return prop.id;
  }

  function node(tag, content) {
    const item = document.createElement(tag);
    if (content !== undefined) item.textContent = content;
    return item;
  }
  function properties(data) { return data.modules.flatMap((module) => module.properties); }
  function actions(data) { return data.modules.flatMap((module) => module.actions); }
  function validateCapabilities(data) {
    const types = ["boolean", "unsigned", "integer", "number", "text"];
    if (!data || data.schemaVersion !== 1 || !Number.isInteger(data.revision) ||
        !Array.isArray(data.modules) || !data.modules.every((module) =>
          module && typeof module.id === "string" && typeof module.type === "string" &&
          typeof module.ready === "boolean" && Array.isArray(module.properties) &&
          Array.isArray(module.actions) && Array.isArray(module.triggers) &&
          module.properties.every((prop) => prop && typeof prop.id === "string" &&
            types.includes(prop.type) && typeof prop.quality === "string") &&
          module.actions.every((action) => action && typeof action.id === "string" &&
            Array.isArray(action.parameters) && action.parameters.every((param) =>
              param && typeof param.id === "string" && types.includes(param.type) &&
              typeof param.required === "boolean")))) throw new Error("Invalid capabilities response");
    return data;
  }
  async function request(path, body) {
    const response = await fetch(path, body === undefined ? { cache: "no-store" } : {
      method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body)
    });
    const contentType = response.headers.get("content-type") || "";
    if (!contentType.toLowerCase().includes("application/json")) {
      throw new Error(text(
        `Brak API JSON ${path} (HTTP ${response.status}). Podgląd plików nie jest firmware; sprawdź adres płytki i wersję firmware.`,
        `No JSON API at ${path} (HTTP ${response.status}). A file preview is not firmware; check the board URL and firmware version.`
      ));
    }
    const data = await response.json();
    if (!response.ok) throw new Error(`${data.error || response.status}: ${data.field || path}`);
    return data;
  }
  function validateLayout(data, available) {
    if (!data || data.schemaVersion !== 1 || !Array.isArray(data.pages) ||
        data.pages.length < 1 || data.pages.length > 8 ||
        !data.pages.every((page) => page && Number.isInteger(page.rows) &&
          Number.isInteger(page.columns) && page.rows >= 1 && page.rows <= 4 &&
          page.columns >= 1 && page.columns <= 4) ||
        !Array.isArray(data.widgets) || data.widgets.length > 16) throw new Error("Invalid screen layout");
    const cells = new Set();
    for (const widget of data.widgets) {
      const page = data.pages[widget.page];
      const cell = `${widget.page}/${widget.row}/${widget.column}`;
      if (!page || !Number.isInteger(widget.page) || !Number.isInteger(widget.row) ||
          !Number.isInteger(widget.column) || widget.row < 0 || widget.column < 0 ||
          widget.row >= page.rows || widget.column >= page.columns ||
          typeof widget.label !== "string" || !/^[\x20-\x7e]{0,16}$/.test(widget.label) ||
          (widget.font !== undefined && (!Number.isInteger(widget.font) || widget.font < 0 || widget.font > 2)) ||
          (widget.textSize !== undefined && (!Number.isInteger(widget.textSize) || widget.textSize < 1 || widget.textSize > 3)) ||
          (widget.unit !== undefined && (typeof widget.unit !== "string" || !/^[\x20-\x7e]{0,8}$/.test(widget.unit))) ||
          !available.some((prop) => prop.id === widget.property) || cells.has(cell)) {
        throw new Error("Invalid screen widget");
      }
      cells.add(cell);
    }
    return data;
  }
  function parameterInput(param) {
    const input = node("input");
    input.type = param.type === "boolean" ? "checkbox" : param.type === "text" ? "text" : "number";
    input.required = param.required && param.type !== "boolean";
    if (param.type === "unsigned") input.min = "0";
    if (param.minimum !== undefined) input.min = param.minimum;
    if (param.maximum !== undefined) input.max = param.maximum;
    if (param.type === "number") input.step = "any";
    else if (param.type !== "text" && param.type !== "boolean") input.step = "1";
    return input;
  }
  function commandFrom(action, controls) {
    const parameters = {};
    for (const param of action.parameters) {
      const input = controls.get(param.id);
      if (!input) throw new Error(`Missing parameter: ${param.id}`);
      if (!param.required && input.value === "" && param.type !== "boolean") continue;
      if (param.type === "boolean") parameters[param.id] = input.checked;
      else if (param.type === "text") parameters[param.id] = input.value;
      else {
        const value = Number(input.value);
        if (input.value === "" || !Number.isFinite(value) ||
            (param.type !== "number" && !Number.isInteger(value)) ||
            (param.type === "unsigned" && (value < 0 || value > 4294967295)) ||
            (param.type === "integer" && (value < -2147483648 || value > 2147483647))) {
          throw new Error(`Invalid parameter: ${param.id}`);
        }
        if ((param.minimum !== undefined && value < param.minimum) ||
            (param.maximum !== undefined && value > param.maximum)) {
          throw new Error(`Parameter out of range: ${param.id}`);
        }
        parameters[param.id] = value;
      }
    }
    return { id: action.id, parameters };
  }
  function renderParameters(container, action) {
    container.replaceChildren();
    const controls = new Map();
    for (const param of action.parameters) {
      const label = node("label");
      label.className = param.type === "boolean" ? "capability-parameter is-boolean" : "capability-parameter";
      const caption = parameterLabels[param.id];
      label.append(node("span", caption ? text(...caption) : param.id));
      const input = parameterInput(param);
      label.append(input);
      container.append(label);
      controls.set(param.id, input);
    }
    return controls;
  }
  function renderModules(data) {
    const list = $("capabilityModules");
    list.replaceChildren();
    for (const module of data.modules) {
      const card = node("section");
      card.className = "panel";
      card.append(node("h2", module.id), node("p", `${module.type} · ${module.ready ? "OK" : module.error}`));
      const readings = node("div");
      readings.className = "capability-properties";
      for (const prop of module.properties) {
        const row = node("div");
        row.className = "capability-property";
        row.dataset.propertyId = prop.id;
        row.append(node("span", propertyLabel(prop)), node("strong",
          prop.quality === "valid" ? `${prop.value} ${prop.unit || ""}` : `-- (${prop.quality})`));
        readings.append(row);
      }
      card.append(readings);
      for (const action of module.actions) {
        const form = node("form");
        form.className = "capability-action";
        const parameters = node("div");
        const controls = renderParameters(parameters, action);
        const button = node("button", text("Wykonaj", "Execute"));
        button.type = "submit";
        button.className = "secondary-button";
        button.disabled = !module.ready;
        const message = node("p");
        message.setAttribute("role", "status");
        form.append(node("h3", action.id), parameters, button, message);
        form.addEventListener("submit", async (event) => {
          event.preventDefault();
          button.disabled = true;
          try {
            const result = await request("/api/actions", commandFrom(action, controls));
            message.textContent = `applied=${result.applied}, confirmed=${result.confirmed}`;
          } catch (error) {
            message.textContent = error.message;
          } finally { button.disabled = !module.ready; }
        });
        card.append(form);
      }
      list.append(card);
    }
  }
  function renderIftttCapabilities(data) {
    const select = $("iftttSource");
    for (const option of select.querySelectorAll("[data-capability]")) option.remove();
    const previousSource = select.value;
    for (const prop of properties(data).filter((prop) => prop.type !== "text" &&
        prop.id !== "ble.presence/in_range")) {
      const option = node("option", propertyLabel(prop));
      option.value = prop.id;
      option.dataset.capability = "true";
      select.append(option);
    }
    if ([...select.children].some((option) => option.value === previousSource)) select.value = previousSource;
    const commands = $("iftttCapabilityAction");
    const previous = commands.value;
    commands.replaceChildren();
    for (const action of actions(data)) {
      const option = node("option", action.id);
      option.value = action.id;
      commands.append(option);
    }
    if (actions(data).some((action) => action.id === previous)) commands.value = previous;
    renderIftttParameters();
  }
  function renderIftttParameters() {
    const action = capabilities && actions(capabilities).find(
      (item) => item.id === $("iftttCapabilityAction").value);
    actionControls = action ? renderParameters($("iftttCapabilityParameters"), action) : new Map();
  }
  async function refresh() {
    if (loading) return;
    loading = true;
    try {
      const data = validateCapabilities(await request("/api/capabilities"));
      const changed = !capabilities || capabilities.revision !== data.revision ||
        properties(capabilities).map(prop => prop.id).join("|") !== properties(data).map(prop => prop.id).join("|");
      capabilities = data;
      if (changed) {
        renderModules(data);
        renderIftttCapabilities(data);
      } else {
        for (const row of $("capabilityModules").querySelectorAll("[data-property-id]")) {
          const prop = properties(data).find((item) => item.id === row.dataset.propertyId);
          if (prop) row.querySelector("strong").textContent =
            prop.quality === "valid" ? `${prop.value} ${prop.unit || ""}` : `-- (${prop.quality})`;
        }
      }
      $("capabilityMessage").textContent = `revision ${data.revision} · ${data.modules.length} modules`;
      if (layout) updatePreviews();
    } catch (error) {
      $("capabilityMessage").textContent = error.message;
      $("capabilityModules").replaceChildren();
    } finally { loading = false; lastRefresh = Date.now(); }
  }
  function updatePreviews() {
    for (const preview of document.querySelectorAll("[data-screen-preview]")) {
      const pageIndex = Number(preview.dataset.screenPreview);
      const page = layout.pages[pageIndex];
      preview.style.gridTemplateRows = `repeat(${page.rows}, 1fr)`;
      preview.style.gridTemplateColumns = `repeat(${page.columns}, 1fr)`;
      preview.replaceChildren();
      for (let row = 0; row < page.rows; row++) {
        for (let column = 0; column < page.columns; column++) {
          const widget = layout.widgets.find((item) => item.page === pageIndex &&
            item.row === row && item.column === column);
          const prop = widget && capabilities && properties(capabilities).find(
            (item) => item.id === widget.property);
          const unit = widget?.unit || prop?.unit || "";
          const cell = node("div", widget ? `${widget.label}: ${prop?.quality === "valid" ? prop.value : "--"} ${unit}` : "--");
          cell.className = "screen-cell";
          cell.style.fontFamily = widget?.font === 1 ? "sans-serif" : "monospace";
          cell.style.fontSize = `${12 * (widget?.textSize || 1)}px`;
          preview.append(cell);
        }
      }
    }
  }
  function renderScreen() {
    const container = $("screenPages");
    container.replaceChildren();
    layout.pages.forEach((page, index) => {
      const section = node("section");
      section.className = "screen-page";
      section.append(node("h3", `${text("Strona", "Page")} ${index + 1}`));
      for (const dimension of ["rows", "columns"]) {
        const label = node("label", dimension);
        const input = node("input");
        input.type = "number"; input.min = "1"; input.max = "4"; input.value = page[dimension];
        input.addEventListener("change", () => {
          const value = Number(input.value);
          if (!Number.isInteger(value) || value < 1 || value > 4 ||
              layout.widgets.some((widget) => widget.page === index &&
                widget[dimension === "rows" ? "row" : "column"] >= value)) {
            input.value = page[dimension];
            $("screenMessage").textContent = text("Usuń widgety poza gridem przed zmniejszeniem.", "Remove out-of-grid widgets before shrinking.");
            return;
          }
          page[dimension] = value; updatePreviews();
        });
        label.append(input); section.append(label);
      }
      const preview = node("div"); preview.className = "screen-preview";
      preview.dataset.screenPreview = index; section.append(preview);
      layout.widgets.filter((widget) => widget.page === index).forEach((widget) => {
        const editor = node("div"); editor.className = "screen-widget";
        for (const position of ["row", "column", "label", "unit", "textSize"]) {
          const caption = node("label");
          caption.append(node("span", position));
          const input = node("input"); input.setAttribute("aria-label", position);
          input.value = widget[position] ?? (position === "textSize" ? 1 : "");
          const isText = position === "label" || position === "unit";
          input.type = isText ? "text" : "number";
          if (isText) input.maxLength = position === "label" ? 16 : 8;
          else if (position === "textSize") { input.min = 1; input.max = 3; }
          else { input.min = 0; input.max = page[position === "row" ? "rows" : "columns"] - 1; }
          input.addEventListener("change", () => {
            const next = isText ? input.value : Number(input.value);
            if (position === "textSize" && (!Number.isInteger(next) || next < 1 || next > 3)) {
              input.value = widget.textSize || 1;
              $("screenMessage").textContent = text("Rozmiar fonta: 1–3.", "Font size: 1–3.");
              return;
            }
            if (!isText && position !== "textSize" && (!Number.isInteger(next) || next < 0 ||
                next >= page[position === "row" ? "rows" : "columns"] ||
                layout.widgets.some((other) => other !== widget && other.page === index &&
                  other.row === (position === "row" ? next : widget.row) &&
                  other.column === (position === "column" ? next : widget.column)))) {
              input.value = widget[position];
              $("screenMessage").textContent = text("Pozycja poza gridem lub zajęta.", "Position outside grid or occupied.");
              return;
            }
            widget[position] = next; updatePreviews();
          });
          caption.append(input); editor.append(caption);
        }
        const fontLabel = node("label", text("Font", "Font"));
        const font = node("select");
        ["Classic 5x7", "FreeSans 9pt", "FreeMono 9pt"].forEach((name, id) => {
          const option = node("option", name); option.value = id; font.append(option);
        });
        font.value = widget.font || 0;
        font.addEventListener("change", () => { widget.font = Number(font.value); updatePreviews(); });
        fontLabel.append(font); editor.append(fontLabel);
        const binding = node("select"); binding.setAttribute("aria-label", "property");
        for (const prop of properties(capabilities)) {
          const option = node("option", prop.id); option.value = prop.id; binding.append(option);
        }
        binding.value = widget.property;
        binding.addEventListener("change", () => { widget.property = binding.value; updatePreviews(); });
        const remove = node("button", text("Usuń", "Remove")); remove.type = "button";
        remove.addEventListener("click", () => { layout.widgets = layout.widgets.filter((other) => other !== widget); renderScreen(); });
        editor.append(binding, remove); section.append(editor);
      });
      const add = node("button", text("Dodaj widget", "Add widget")); add.type = "button";
      add.disabled = layout.widgets.length >= 16 || layout.widgets.filter((widget) => widget.page === index).length >= page.rows * page.columns;
      add.addEventListener("click", () => {
        for (let row = 0; row < page.rows; row++) for (let column = 0; column < page.columns; column++) {
          if (!layout.widgets.some((widget) => widget.page === index && widget.row === row && widget.column === column)) {
            const prop = properties(capabilities)[0]; if (!prop) return;
            layout.widgets.push({ page: index, row, column, property: prop.id, label: "Value", font: 0, textSize: 1, unit: "" });
            renderScreen(); return;
          }
        }
      });
      const removePage = node("button", text("Usuń stronę", "Remove page")); removePage.type = "button";
      removePage.disabled = layout.pages.length <= 1;
      removePage.addEventListener("click", () => {
        layout.pages.splice(index, 1);
        layout.widgets = layout.widgets.filter((widget) => widget.page !== index);
        layout.widgets.forEach((widget) => { if (widget.page > index) widget.page--; });
        renderScreen();
      });
      section.append(add, removePage); container.append(section);
    });
    $("screenAddPage").disabled = layout.pages.length >= 8;
    updatePreviews();
  }
  async function loadScreen() {
    try {
      if (!capabilities) await refresh();
      if (!capabilities) throw new Error("Capabilities unavailable");
      layout = validateLayout(await request("/api/screen"), properties(capabilities));
      renderScreen();
      $("screenMessage").textContent = "";
    } catch (error) { $("screenMessage").textContent = error.message; }
  }
  function settingsGroup(id) {
    if (/^(sensor|gpio3|temperature|bme|ens|compass|gps|rtc|pn532)/.test(id)) return "sensors";
    if (/^display/.test(id)) return "screen";
    if (/^(beeper|fan)/.test(id)) return "outputs";
    if (/^bluetooth|^ble/.test(id)) return "bluetooth";
    if (/^(udp|uart|rs485|i2c)/.test(id)) return "interfaces";
    return "inputs";
  }
  function groupedSettings(fields) {
    return ["interfaces", "bluetooth", "sensors", "outputs", "inputs", "screen"]
      .map((id) => ({ id, fields: fields.filter((field) => settingsGroup(field.id) === id) }))
      .filter((group) => group.fields.length);
  }
  async function loadSettings() {
    try {
      const [fields, settings] = await Promise.all([request("/api/settings/schema"), request("/api/settings")]);
      if (fields.schemaVersion < 1 || !Array.isArray(fields.fields) ||
          !fields.fields.every((field) => field && typeof field.id === "string" &&
            ["boolean", "unsigned"].includes(field.type) && Array.isArray(field.allowedValues) &&
            typeof field.derived === "boolean" && Number.isFinite(field.minimum) &&
            Number.isFinite(field.maximum))) throw new Error("Invalid settings schema");
      schema = fields;
      settingsControls = new Map();
      $("schemaFields").replaceChildren();
      const titles = {
        interfaces: ["Magistrale i UDP", "Buses and UDP"], bluetooth: ["Bluetooth i obecność BLE", "Bluetooth and BLE presence"],
        sensors: ["Sensory i próbkowanie", "Sensors and sampling"], outputs: ["Wyjścia · PWM i beeper", "Outputs · PWM and beeper"],
        inputs: ["Wejścia", "Inputs"], screen: ["Ekran OLED", "OLED screen"]
      };
      for (const group of groupedSettings(fields.fields)) {
        const section = node("details"); section.className = "settings-accordion";
        section.dataset.settingsGroup = group.id;
        section.append(node("summary", text(...titles[group.id])));
        const container = node("div"); container.className = "schema-fields";
        for (const field of group.fields) {
          const label = node("label", field.id);
          let input;
          if (field.allowedValues.length) {
            input = node("select");
            for (const value of field.allowedValues) {
              const option = node("option", value); option.value = value; input.append(option);
            }
          } else {
            input = node("input"); input.type = field.type === "boolean" ? "checkbox" : "number";
            if (field.type !== "boolean") { input.min = field.minimum; input.max = field.maximum; input.step = "1"; }
          }
          if (field.type === "boolean") input.checked = settings[field.id];
          else input.value = settings[field.id];
          input.disabled = field.derived;
          if (field.type === "boolean") label.className = "is-boolean";
          label.append(input); container.append(label); settingsControls.set(field.id, input);
        }
        section.append(container); $("schemaFields").append(section);
      }
      $("schemaMessage").textContent = "";
    } catch (error) { $("schemaMessage").textContent = error.message; }
  }
  function validateCurrent(data) {
    if (!data || data.schemaVersion !== 1 || !Array.isArray(data.channels) || data.channels.length !== 4) {
      throw new Error("Invalid current configuration");
    }
    for (const channel of data.channels) {
      if (!channel || ![0, 1, 2].includes(channel.signal) || typeof channel.calibrated !== "boolean" ||
          !Number.isInteger(channel.rawLow) || !Number.isInteger(channel.rawHigh) ||
          channel.rawLow < 0 || channel.rawLow >= channel.rawHigh || channel.rawHigh > 4095 ||
          !["signalLow", "signalHigh", "currentLow", "currentHigh"].every(
            (field) => typeof channel[field] === "number" && Number.isFinite(channel[field])) ||
          channel.signalLow < 0 || channel.signalLow >= channel.signalHigh ||
          channel.signalHigh > (channel.signal === 1 ? 5 : 24) ||
          channel.currentLow < -100000 || channel.currentHigh > 100000 ||
          channel.currentLow >= channel.currentHigh) throw new Error("Invalid current calibration");
    }
    return data;
  }
  async function loadCurrent(repair = false) {
    try {
      currentConfig = validateCurrent(repair ? { schemaVersion: 1, channels: Array.from({ length: 4 }, () => ({
        signal: 0, calibrated: false, rawLow: 0, rawHigh: 4095,
        signalLow: 4, signalHigh: 20, currentLow: 0, currentHigh: 20
      })) } : await request("/api/current"));
      $("currentRepair").hidden = true;
      currentControls = [];
      $("currentChannels").replaceChildren();
      currentConfig.channels.forEach((channel, index) => {
        const section = node("section"); section.className = "current-channel";
        section.append(node("h3", `A${index + 1} · GPIO${[36, 39, 34, 35][index]}`));
        const controls = new Map();
        const signalLabel = node("label", text("Sygnał", "Signal"));
        const signal = node("select");
        [text("Wyłączony", "Off"), text("Napięcie 0–5 V", "Voltage 0–5 V"), text("Pętla (np. 4–20 mA)", "Loop (e.g. 4–20 mA)")].forEach((name, value) => {
          const option = node("option", name); option.value = value; signal.append(option);
        });
        signal.value = channel.signal;
        signalLabel.append(signal); section.append(signalLabel); controls.set("signal", signal);
        const fields = {
          rawLow: ["ADC · punkt dolny", "ADC · low point"], rawHigh: ["ADC · punkt górny", "ADC · high point"],
          signalLow: ["Sygnał dolny (V/mA)", "Low signal (V/mA)"], signalHigh: ["Sygnał górny (V/mA)", "High signal (V/mA)"],
          currentLow: ["Prąd dolny (A)", "Low current (A)"], currentHigh: ["Prąd górny (A)", "High current (A)"]
        };
        for (const [field, names] of Object.entries(fields)) {
          const label = node("label", text(...names)); const input = node("input");
          input.type = "number"; input.value = channel[field]; input.required = true;
          input.step = field.startsWith("raw") ? "1" : "any";
          if (field.startsWith("raw")) { input.min = "0"; input.max = "4095"; }
          label.append(input); section.append(label); controls.set(field, input);
          if (field.startsWith("raw")) {
            const capture = node("button", text("Pobierz bieżący ADC", "Capture current ADC")); capture.type = "button";
            capture.className = "secondary-button";
            capture.addEventListener("click", async () => {
              try {
                const data = validateCapabilities(await request("/api/capabilities"));
                const prop = properties(data).find((item) => item.id === `adc.main/channel/${index + 1}/raw`);
                if (!prop || prop.quality !== "valid" || !Number.isInteger(prop.value)) throw new Error("ADC unavailable");
                input.value = prop.value;
                controls.get("calibrated").checked = false;
                $("currentMessage").textContent = text("Punkt pobrany; potwierdź oba punkty i zapisz kalibrację.", "Point captured; verify both endpoints and save calibration.");
              } catch (error) { $("currentMessage").textContent = error.message; }
            });
            section.append(capture);
          }
        }
        const calibrated = node("input"); calibrated.type = "checkbox"; calibrated.checked = channel.calibrated;
        const label = node("label", text("Potwierdzam kalibrację dwóch punktów", "Confirm two-point calibration"));
        label.append(calibrated); section.append(label); controls.set("calibrated", calibrated);
        signal.addEventListener("change", () => {
          controls.get("signalLow").value = Number(signal.value) === 1 ? 0 : 4;
          controls.get("signalHigh").value = Number(signal.value) === 1 ? 5 : 20;
          calibrated.checked = false;
        });
        currentControls.push(controls); $("currentChannels").append(section);
      });
      $("currentMessage").textContent = repair ? text("Nowa konfiguracja nie jest jeszcze zapisana. Zapis zastąpi poprzednią kalibrację.", "Replacement not yet saved. Saving will replace the previous calibration.") : "";
    } catch (error) {
      currentConfig = null; currentControls = [];
      $("currentChannels").replaceChildren();
      $("currentRepair").hidden = false;
      $("currentMessage").textContent = error.message;
    }
  }
  async function loadNetwork() {
    try {
      const data = await request("/api/network");
      if (typeof data.hostname !== "string") throw new Error("Invalid network configuration");
      $("hostname").value = data.hostname;
      $("networkIdentityStatus").textContent = `${data.hostname}.local · mDNS=${data.mdnsReady} · UDP ${data.udpListenAddress}:${data.udpListenPort}`;
      $("networkSettingsMessage").textContent = "";
    } catch (error) { $("networkSettingsMessage").textContent = error.message; }
  }
  window.CapabilityUI = {
    validateCapabilities, validateLayout, validateCurrent, groupedSettings, properties, actions, commandFrom, request, propertyLabel, refresh,
    getIftttCommand() {
      const action = capabilities && actions(capabilities).find((item) => item.id === $("iftttCapabilityAction").value);
      if (!action) throw new Error("No interface action available");
      return commandFrom(action, actionControls);
    }
  };
  $("currentRepair").addEventListener("click", () => loadCurrent(true));
  $("currentSettingsForm").addEventListener("submit", async (event) => {
    event.preventDefault(); const button = event.currentTarget.querySelector("[type=submit]"); button.disabled = true;
    try {
      if (!currentConfig || currentControls.length !== 4) throw new Error("Current configuration unavailable");
      const channels = currentControls.map((controls) => Object.fromEntries([...controls].map(([field, input]) => {
        if (field !== "calibrated" && input.value === "") throw new Error(`Missing field: ${field}`);
        return [field, field === "calibrated" ? input.checked : Number(input.value)];
      })));
      const candidate = validateCurrent({ schemaVersion: 1, channels });
      await request("/api/current", candidate); await loadCurrent(); await refresh();
      $("currentMessage").textContent = text("Kalibracja zapisana.", "Calibration saved.");
    } catch (error) { $("currentMessage").textContent = error.message; }
    finally { button.disabled = false; }
  });
  $("networkSettingsForm").addEventListener("submit", async (event) => {
    event.preventDefault(); const button = event.currentTarget.querySelector("[type=submit]"); button.disabled = true;
    try {
      const hostname = $("hostname").value.trim();
      if (!/^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$/.test(hostname)) throw new Error("Invalid hostname");
      const result = await request("/api/network", { schemaVersion: 1, hostname });
      $("hostname").value = result.hostname;
      $("networkSettingsMessage").textContent = result.reconnectPending ?
        text("Zapisano. Wi-Fi łączy się ponownie; nowy adres mDNS będzie dostępny po połączeniu.", "Saved. Wi-Fi is reconnecting; the new mDNS address will be available after connection.") :
        text("Zapisano.", "Saved.");
    } catch (error) { $("networkSettingsMessage").textContent = error.message; }
    finally { button.disabled = false; }
  });
  $("iftttCapabilityAction").addEventListener("change", renderIftttParameters);
  $("screenAddPage").addEventListener("click", () => {
    if (layout && layout.pages.length < 8) { layout.pages.push({ rows: 2, columns: 2 }); renderScreen(); }
  });
  $("screenLayoutForm").addEventListener("submit", async (event) => {
    event.preventDefault(); const button = event.currentTarget.querySelector("[type=submit]"); button.disabled = true;
    try {
      if (!layout) throw new Error("Screen layout unavailable");
      validateLayout(layout, properties(capabilities));
      layout = validateLayout(await request("/api/screen", layout), properties(capabilities)); renderScreen();
      $("screenMessage").textContent = text("Układ zapisany.", "Layout saved.");
    } catch (error) { $("screenMessage").textContent = error.message; }
    finally { button.disabled = false; }
  });
  $("schemaSettingsForm").addEventListener("submit", async (event) => {
    event.preventDefault(); const button = event.currentTarget.querySelector("[type=submit]"); button.disabled = true;
    try {
      if (!schema) throw new Error("Settings schema unavailable");
      const payload = {};
      for (const field of schema.fields.filter((field) => !field.derived)) {
        const input = settingsControls.get(field.id);
        payload[field.id] = field.type === "boolean" ? input.checked : Number(input.value);
      }
      await request("/api/settings", payload); await loadDeviceSettings();
      $("schemaMessage").textContent = text("Zapisano.", "Saved.");
    } catch (error) { $("schemaMessage").textContent = error.message; }
    finally { button.disabled = false; }
  });
  document.querySelectorAll(".page-tab").forEach((tab) => tab.addEventListener("click", () => {
    if (tab.dataset.page === "io" || tab.dataset.page === "automation") refresh();
    if (tab.dataset.page === "screen") loadScreen();
    if (tab.dataset.page === "settings") { loadSettings(); loadNetwork(); }
    if (tab.dataset.page === "sensors") loadCurrent();
  }));
  setInterval(() => {
    const page = document.querySelector(".page-tab.is-active")?.dataset.page;
    if (page === "io" && !document.querySelector("#capabilityModules :focus") && Date.now() - lastRefresh >= 2000) refresh();
    if (page === "screen" && layout && Date.now() - lastRefresh >= 2000) refresh();
  }, 1000);
  refresh();
})();
