#pragma once

#include "Driver_USART.h"
#include "platform.hpp"

#include <cstdint>

/// Thin wrapper around the CMSIS-Driver USART used for the BTC's
/// RS-422 downlink to the RXSM (ICD-001)
///
/// @ingroup apps
class Rs422Downlink {
  public:
    /// usart:    the CMSIS-Driver USART instance (e.g. Driver_USART2).
    /// platform: borrowed for tick_ms; used to implement the TX-ready
    ///           timeout. Both references must outlive this object.
    Rs422Downlink(ARM_DRIVER_USART& usart, const Platform& platform) noexcept : usart(usart), platform(platform) {
    }

    Rs422Downlink(const Rs422Downlink&) = delete;
    Rs422Downlink& operator=(const Rs422Downlink&) = delete;
    Rs422Downlink(Rs422Downlink&&) = delete;
    Rs422Downlink& operator=(Rs422Downlink&&) = delete;

    /// One-time bring-up: power on, configure framing (8N1, no flow
    /// control), enable TX. Call from on_init().
    void init(uint32_t baud) noexcept {
        usart.Initialize(nullptr);
        usart.PowerControl(ARM_POWER_FULL);
        usart.Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE |
                          ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE,
                      baud);
        usart.Control(ARM_USART_CONTROL_TX, 1U);
    }

    /// Wait until the previous TX has cleared, then push `len` bytes
    /// onto the line. Drops the new frame silently if the previous
    /// transmission has not completed within TX_TIMEOUT_MS - that
    /// keeps the tick loop bounded when the cable is unplugged or
    /// the RXSM stops draining (I-2).
    void send(const uint8_t* data, uint8_t len) noexcept {
        wait_tx_ready();
        usart.Send(data, len);
    }

  private:
    static constexpr uint32_t TX_TIMEOUT_MS = 10U;

    void wait_tx_ready() noexcept {
        const uint32_t deadline = platform.tick_ms() + TX_TIMEOUT_MS;
        while (usart.GetStatus().tx_busy != 0U) {
            if (platform.tick_ms() > deadline) {
                break;
            }
        }
    }

    ARM_DRIVER_USART& usart;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const Platform& platform; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};
