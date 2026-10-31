#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
environment="${PIO_ENV:-kc868-a6}"
arduino_json=".pio/libdeps/$environment/ArduinoJson/src"
if [[ ! -f "$arduino_json/ArduinoJson.h" ]]; then
    echo "ArduinoJson is missing. Run 'pio run -e $environment' first." >&2
    exit 1
fi

binary="$(mktemp .pio/native-core-tests.XXXXXX)"
trap 'rm -f "$binary"' EXIT

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fno-exceptions -fno-rtti -fsanitize=address,undefined -g \
    -DKC868_BLE_PRESENCE_NO_MAIN -DKC868_TRANSPORT_NO_MAIN \
    -Itest/support -Iinclude -I"$arduino_json" \
    test/test_core.cpp src/core/peripheral_registry.cpp \
    src/peripherals/analog_inputs.cpp src/web/analog_status.cpp \
    test/test_config.cpp src/config/device_settings.cpp src/config/config_service.cpp \
    src/config/settings_json.cpp src/config/preferences_settings_store.cpp \
    test/test_board.cpp src/board/board_profile.cpp src/board/resource_manager.cpp \
    src/board/settings_resources.cpp src/web/board_status.cpp \
    test/test_capabilities.cpp src/peripherals/board_io.cpp src/web/capabilities.cpp \
    src/display/screen_layout.cpp \
    src/peripherals/sensor_readings.cpp \
    src/peripherals/fan_control.cpp \
    test/test_ble_presence.cpp src/peripherals/ble_presence.cpp src/peripherals/ble_targets.cpp \
    test/test_transport.cpp src/peripherals/transport.cpp \
    test/test_measurements.cpp src/peripherals/current_inputs.cpp src/config/network_config.cpp \
    test/test_udp_rules.cpp src/peripherals/udp_rules.cpp src/web/ble_scan.cpp \
    -o "$binary"
"$binary"
