#pragma once

#include <cstdint>

// Abstract CAN send interface
class CanTransport {
  public:
    virtual ~CanTransport() = default;
    CanTransport(const CanTransport&) = delete;
    CanTransport& operator=(const CanTransport&) = delete;
    CanTransport(CanTransport&&) = delete;
    CanTransport& operator=(CanTransport&&) = delete;

    // Send `len` bytes from `data` under CAN ID `id`.
    virtual void send(uint32_t id, const uint8_t* data, uint8_t len) = 0;

  protected:
    CanTransport() = default;
};
