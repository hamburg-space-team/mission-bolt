#pragma once

#include <cstdint>

/// Send-only CAN interface. Concrete implementation (BxcanTransport)
/// handles fragmentation and mailbox arbitration; the EXP pipeline only
/// sees this interface, which keeps host unit tests buildable without HAL.
///
/// @ingroup comms
class CanTransport {
  public:
    virtual ~CanTransport() = default;
    CanTransport(const CanTransport&) = delete;
    CanTransport& operator=(const CanTransport&) = delete;
    CanTransport(CanTransport&&) = delete;
    CanTransport& operator=(CanTransport&&) = delete;

    /// Send len bytes from data under CAN ID id.
    /// len <= CanProtocol::EXP_DATA_LEN.
    virtual void send(uint32_t id, const uint8_t* data, uint8_t len) = 0;

    /// True once the transport latched a persistent TX failure (e.g. the
    /// TX ring never drains). Default: never fails (host test stubs).
    [[nodiscard]] virtual bool is_failed() const {
        return false;
    }

  protected:
    CanTransport() = default;
};
