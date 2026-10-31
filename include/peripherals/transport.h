#pragma once

#include "core/peripheral.h"

namespace kc868a6::peripherals {

// All reads are nonblocking. A UDP read consumes one whole datagram, including
// on CapacityExceeded; serialRead returns -1 when no byte is immediately ready.
class ITransportHardware {
public:
    virtual ~ITransportHardware() = default;
    virtual bool wifiConnected() const = 0;
    virtual core::Result listenUdp(uint16_t port) = 0;
    virtual void stopUdp() = 0;
    virtual core::Result receiveUdp(char *data, size_t capacity, size_t &size,
                                    bool &received) = 0;
    virtual core::ActionResult sendUdp(std::string_view host, uint16_t port,
                                       std::string_view data) = 0;
    virtual core::Result serialRead(uint8_t port, int &byte) = 0;
    virtual core::ActionResult serialSend(uint8_t port, std::string_view data) = 0;
};

struct TransportConfig {
    bool enabled = true;
    uint16_t udpListenPort = 0;
    bool serial1Enabled = true;
    bool serial2Enabled = true;
    // Parent must set this for BOTH GPS and coprocessor UART1 modes.
    bool serial1Reserved = false;
    uint32_t serialIdleMs = 25;
};

class Transport final : public core::IPeripheral,
                        public core::IPropertyProvider,
                        public core::ITriggerProvider,
                        public core::IActionProvider {
public:
    static constexpr size_t maxPayloadBytes = 256;
    static constexpr size_t maxHostBytes = 253;
    static constexpr size_t serialReadBudget = 512;

    explicit Transport(ITransportHardware &hardware) : hardware_(hardware) {}
    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;
    const core::PeripheralDescriptor &descriptor() const override;
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::List<core::ActionDescriptor> actions() const override;
    core::Result begin(uint32_t nowMs) override;
    core::Result tick(uint32_t nowMs) override;
    void stop() override;
    core::Health health() const override { return health_; }
    core::Result configure(const TransportConfig &config, uint32_t nowMs);
    const TransportConfig &config() const { return config_; }
    core::Result readProperty(std::string_view id,
                              core::PropertySnapshot &snapshot) const override;
    core::Result validate(std::string_view id,
                          core::List<core::Parameter> parameters) const override;
    core::ActionResult execute(std::string_view id,
                               core::List<core::Parameter> parameters) override;
    // Printable ASCII/UTF-8 plus TAB/CR/LF; rejects NUL, malformed UTF-8 and
    // control characters. Length is bytes, not Unicode code points.
    static core::Result validateText(std::string_view text);
    static core::Result validateHost(std::string_view host);

private:
    struct Receiver {
        std::array<char, maxPayloadBytes> last{};
        size_t lastSize = 0;
        uint32_t sequence = 0;
        std::array<char, maxPayloadBytes + 1> frame{};
        size_t frameSize = 0;
        uint32_t lastByteMs = 0;
        bool overflow = false;
    };
    bool serialEnabled(uint8_t port) const;
    void refreshAvailability(uint32_t nowMs);
    void receiverError(size_t receiver, core::ErrorCode error, uint32_t nowMs);
    core::Result accept(size_t receiver, std::string_view text, uint32_t nowMs);
    core::Result pollSerial(uint8_t port, uint32_t nowMs);
    core::Result finishSerial(uint8_t port, uint32_t nowMs);
    core::Result updateListener();
    ITransportHardware &hardware_;
    TransportConfig config_{};
    std::array<Receiver, 3> receivers_{};
    std::array<core::PropertySnapshot, 7> snapshots_{};
    core::Health health_{};
    bool started_ = false;
    bool listening_ = false;
};

}  // namespace kc868a6::peripherals
