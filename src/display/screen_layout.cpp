#include "display/screen_layout.h"
#include <cstdio>
#include <cstring>

namespace kc868a6::display {
Cell widgetCell(const ScreenPage &page, const ScreenWidget &widget,
                uint16_t width, uint16_t height) {
    const auto x = static_cast<uint16_t>(width * widget.column / page.columns);
    const auto y = static_cast<uint16_t>(height * widget.row / page.rows);
    return {x, y, static_cast<uint16_t>(width * (widget.column + 1) / page.columns - x),
            static_cast<uint16_t>(height * (widget.row + 1) / page.rows - y)};
}
ScreenLayout defaultLayout() {
    ScreenLayout layout;
    layout.widgetCount = 4;
    for (size_t index = 0; index < 4; ++index) {
        auto &widget = layout.widgets[index];
        widget.row = index / 2;
        widget.column = index % 2;
        std::snprintf(widget.property.data(), widget.property.size(), "adc.main/channel/%u/raw",
                      static_cast<unsigned>(index + 1));
        std::snprintf(widget.label.data(), widget.label.size(), "A%u", static_cast<unsigned>(index + 1));
    }
    return layout;
}
core::Result readLayout(JsonVariantConst source, const core::PeripheralRegistry &registry,
                        ScreenLayout &destination) {
    const auto invalid = core::Result{core::ErrorCode::InvalidArgument, "screen_layout"};
    if (!source.is<JsonObjectConst>() || source.size() != 3 ||
        !source["schemaVersion"].is<uint32_t>() || source["schemaVersion"].as<uint32_t>() != 1 ||
        !source["pages"].is<JsonArrayConst>() || !source["widgets"].is<JsonArrayConst>()) return invalid;
    const auto pages = source["pages"].as<JsonArrayConst>();
    const auto widgets = source["widgets"].as<JsonArrayConst>();
    if (pages.size() < 1 || pages.size() > maxPages || widgets.size() > maxWidgets) return invalid;
    ScreenLayout next;
    next.pageCount = pages.size();
    next.widgetCount = widgets.size();
    size_t index = 0;
    for (const auto page : pages) {
        if (!page.is<JsonObjectConst>() || page.size() != 2 || !page["rows"].is<uint8_t>() ||
            !page["columns"].is<uint8_t>()) return invalid;
        const auto rows = page["rows"].as<uint8_t>();
        const auto columns = page["columns"].as<uint8_t>();
        // A label plus value needs at least 16 px vertically and 24 px horizontally at 128x64.
        if (rows < 1 || rows > 4 || columns < 1 || columns > 4) return invalid;
        next.pages[index++] = {rows, columns};
    }
    index = 0;
    for (const auto value : widgets) {
        if (!value.is<JsonObjectConst>() || value.size() < 5 || value.size() > 8 || !value["page"].is<uint8_t>() ||
            !value["row"].is<uint8_t>() || !value["column"].is<uint8_t>() ||
            !value["property"].is<const char *>() || !value["label"].is<const char *>()) return invalid;
        auto &widget = next.widgets[index++];
        for (JsonPairConst field : value.as<JsonObjectConst>()) {
            const std::string_view name = field.key().c_str();
            if (name != "page" && name != "row" && name != "column" && name != "property" &&
                name != "label" && name != "font" && name != "textSize" && name != "unit") return invalid;
        }
        if (value.containsKey("font")) {
            if (!value["font"].is<uint8_t>() || value["font"].as<uint8_t>() > 2) return invalid;
            widget.font = value["font"].as<uint8_t>();
        }
        if (value.containsKey("textSize")) {
            if (!value["textSize"].is<uint8_t>() || value["textSize"].as<uint8_t>() < 1 ||
                value["textSize"].as<uint8_t>() > 3) return invalid;
            widget.textSize = value["textSize"].as<uint8_t>();
        }
        if (value.containsKey("unit")) {
            if (!value["unit"].is<const char *>()) return invalid;
            const char *unit = value["unit"].as<const char *>();
            if (std::strlen(unit) != value["unit"].as<JsonString>().size() ||
                std::strlen(unit) >= widget.unit.size()) return invalid;
            for (const char *c = unit; *c; ++c) if (*c < 32 || *c > 126) return invalid;
            std::strcpy(widget.unit.data(), unit);
        }
        widget.page = value["page"].as<uint8_t>();
        widget.row = value["row"].as<uint8_t>();
        widget.column = value["column"].as<uint8_t>();
        if (widget.page >= next.pageCount || widget.row >= next.pages[widget.page].rows ||
            widget.column >= next.pages[widget.page].columns) return invalid;
        const char *property = value["property"].as<const char *>();
        const char *label = value["label"].as<const char *>();
        if (std::strlen(property) != value["property"].as<JsonString>().size() ||
            std::strlen(label) != value["label"].as<JsonString>().size() ||
            std::strlen(property) >= widget.property.size() || !registry.findProperty(property) ||
            std::strlen(label) >= widget.label.size()) return invalid;
        for (const char *character = label; *character; ++character) {
            if (*character < 32 || *character > 126) return invalid;
        }
        std::strcpy(widget.property.data(), property);
        std::strcpy(widget.label.data(), label);
        for (size_t previous = 0; previous + 1 < index; ++previous) {
            const auto &other = next.widgets[previous];
            if (other.page == widget.page && other.row == widget.row &&
                other.column == widget.column) return {core::ErrorCode::ResourceConflict, "screen_cell"};
        }
    }
    destination = next;
    return {};
}
core::Result writeLayout(JsonObject destination, const ScreenLayout &layout) {
    bool ok = destination["schemaVersion"].set(1);
    auto pages = destination.createNestedArray("pages");
    auto widgets = destination.createNestedArray("widgets");
    for (size_t index = 0; index < layout.pageCount && ok; ++index) {
        auto page = pages.createNestedObject();
        ok = page["rows"].set(layout.pages[index].rows) &&
             page["columns"].set(layout.pages[index].columns);
    }
    for (size_t index = 0; index < layout.widgetCount && ok; ++index) {
        const auto &widget = layout.widgets[index];
        auto item = widgets.createNestedObject();
        ok = item["page"].set(widget.page) && item["row"].set(widget.row) &&
             item["column"].set(widget.column) && item["property"].set(widget.property.data()) &&
             item["label"].set(widget.label.data()) && item["font"].set(widget.font) &&
             item["textSize"].set(widget.textSize) && item["unit"].set(widget.unit.data());
    }
    return ok && !pages.isNull() && !widgets.isNull() ? core::Result{}
        : core::Result{core::ErrorCode::CapacityExceeded, "screen_json"};
}
}
