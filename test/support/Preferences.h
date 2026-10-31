#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>

using String = std::string;

enum PreferenceType { PT_U8, PT_U16, PT_U32, PT_STR, PT_INVALID };

// Native fault injection for the actual PreferencesSettingsStore adapter.
class Preferences {
public:
    using StoredValue = std::variant<uint8_t, uint16_t, uint32_t, String>;
    inline static std::map<std::string, StoredValue> values;
    inline static bool failOpen = false;
    inline static bool failWrite = false;
    inline static bool failRead = false;
    inline static bool corruptAfterWrite = false;
    inline static bool failReadAfterWrite = false;
    inline static size_t writes = 0;

    static void reset() {
        values.clear();
        failOpen = failWrite = failRead = corruptAfterWrite = failReadAfterWrite = false;
        writes = 0;
    }

    bool begin(const char *name, bool = false) {
        return std::string(name) == "kc868a6" && !failOpen;
    }
    bool isKey(const char *key) { return values.count(key) != 0; }
    PreferenceType getType(const char *key) {
        const auto it = values.find(key);
        if (it == values.end()) return PT_INVALID;
        if (std::holds_alternative<uint8_t>(it->second)) return PT_U8;
        if (std::holds_alternative<uint16_t>(it->second)) return PT_U16;
        if (std::holds_alternative<uint32_t>(it->second)) return PT_U32;
        return PT_STR;
    }

    uint8_t getUChar(const char *key, uint8_t fallback = 0) {
        return get<uint8_t>(key, fallback);
    }
    uint16_t getUShort(const char *key, uint16_t fallback = 0) {
        return get<uint16_t>(key, fallback);
    }
    uint32_t getUInt(const char *key, uint32_t fallback = 0) {
        return get<uint32_t>(key, fallback);
    }
    String getString(const char *key, String fallback = {}) {
        return get<String>(key, fallback);
    }
    size_t getStringLength(const char *key) {
        if (failRead || getType(key) != PT_STR) return 0;
        return getString(key).length() + 1;
    }
    size_t putString(const char *key, String value) {
        ++writes;
        if (failWrite) return 0;
        const size_t length = value.length();
        values[key] = corruptAfterWrite ? String{"corrupted"} : value;
        if (failReadAfterWrite) failRead = true;
        return length;
    }

private:
    template <typename T>
    T get(const char *key, T fallback) const {
        if (failRead) return fallback;
        const auto it = values.find(key);
        if (it == values.end()) return fallback;
        const auto *value = std::get_if<T>(&it->second);
        return value ? *value : fallback;
    }
};
