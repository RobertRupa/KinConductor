#include "web/board_status.h"
#include "board/target_model.h"

namespace kc868a6::web {

core::Result writeBoardStatus(JsonObject destination, const DeviceSettings &settings,
                             board::BoardOptions options, core::Result configStatus) {
    if (destination.isNull()) return {core::ErrorCode::CapacityExceeded, "board_json"};
    board::ResourceManager resources;
    const auto planned = board::planResources(settings, options, resources);
    bool written = destination["board"].set(board::targetModel.data()) &&
                   destination["pinoutValidated"].set(board::targetPinoutValidated) &&
                   destination["uartCount"].set(board::uartCount) &&
                   destination["uart1Mode"].set(settings.uart1Mode) &&
                   destination["configurationReady"].set(configStatus.ok()) &&
                   destination["configurationError"].set(core::errorCodeName(configStatus.code)) &&
                   destination["valid"].set(planned.ok()) &&
                   destination["error"].set(core::errorCodeName(planned.code)) &&
                   (planned.subject.empty() ? destination["subject"].set("")
                                            : destination["subject"].set(planned.subject));
    JsonObject relays = destination.createNestedObject("relays");
    written = written && relays["address"].set(i2c::relayExpanderAddress) &&
              relays["portMask"].set(board::relayPortMask) &&
              relays["reservedMask"].set(board::relayReservedPortMask) &&
              destination["inputAddress"].set(i2c::inputExpanderAddress);
    JsonArray pins = destination.createNestedArray("pins");
    const auto profile = board::pins();
    for (size_t index = 0; index < profile.size && written; ++index) {
        const auto &pin = profile.data[index];
        JsonObject item = pins.createNestedObject();
        written = item["gpio"].set(pin.number) && item["role"].set(pin.role) &&
                  item["inputOnly"].set(pin.inputOnly) &&
                  item["strapping"].set(pin.strapping) &&
                  item["adc"].set(pin.adc) && item["dac"].set(pin.dac);
    }
    JsonArray claims = destination.createNestedArray("claims");
    const auto assigned = resources.claims();
    for (size_t index = 0; index < assigned.size && written; ++index) {
        const auto &claim = assigned.data[index];
        JsonObject item = claims.createNestedObject();
        written = item["kind"].set(board::resourceKindName(claim.kind)) &&
                  item["number"].set(claim.number) && item["owner"].set(claim.owner);
    }
    if (!written || pins.isNull() || claims.isNull()) {
        return {core::ErrorCode::CapacityExceeded, "board_json"};
    }
    return {};
}

}  // namespace kc868a6::web
