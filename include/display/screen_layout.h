#pragma once

#include <ArduinoJson.h>
#include "core/peripheral_registry.h"

namespace kc868a6::display {
constexpr size_t maxPages = 8;
constexpr size_t maxWidgets = 16;
constexpr size_t layoutJsonCapacity = 8192;

struct ScreenPage { uint8_t rows = 2; uint8_t columns = 2; };
struct ScreenWidget {
    uint8_t page = 0;
    uint8_t row = 0;
    uint8_t column = 0;
    std::array<char, 64> property{};
    std::array<char, 17> label{};
    uint8_t font = 0;
    uint8_t textSize = 1;
    std::array<char, 9> unit{};
};
struct ScreenLayout {
    uint8_t pageCount = 1;
    uint8_t widgetCount = 0;
    std::array<ScreenPage, maxPages> pages{};
    std::array<ScreenWidget, maxWidgets> widgets{};
};
struct Cell { uint16_t x; uint16_t y; uint16_t width; uint16_t height; };
Cell widgetCell(const ScreenPage &page, const ScreenWidget &widget,
                uint16_t width, uint16_t height);
ScreenLayout defaultLayout();
core::Result readLayout(JsonVariantConst source, const core::PeripheralRegistry &registry,
                        ScreenLayout &destination);
core::Result writeLayout(JsonObject destination, const ScreenLayout &layout);
}
