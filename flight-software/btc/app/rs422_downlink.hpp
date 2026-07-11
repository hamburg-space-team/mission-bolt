#pragma once

#include "Driver_USART.h"
#include "main.h" // IWYU pragma: keep (__disable_irq)

#include <array>
#include <cstdint>

/// Thin wrapper around the CMSIS-Driver USART used for the BTC's RS-422
/// downlink to the RXSM
///
/// @ingroup apps
class Rs422Downlink {
  private:
    /// Arm a one-byte receive into rx_byte. Re-armed after every completed
    /// byte from the RECEIVE_COMPLETE event so RX runs continuously.
    void arm_receive() noexcept {
        (void)this->usart.Receive(&this->rx_byte, 1U);
    }

    /// Producer side (RX-complete interrupt): push one byte, drop if the ring
    /// is full so the ISR never blocks.
    void rx_push(uint8_t b) noexcept {
        const auto next = static_cast<uint16_t>((this->rx_head + 1U) % RX_RING_SIZE);
        if (next != this->rx_tail) {
            this->rx_ring[this->rx_head] = b;
            this->rx_head = next;
        }
    }

    static constexpr uint16_t TX_RING_SIZE = 4096U;
    static constexpr uint8_t MAX_CONSECUTIVE_DROPS = 10U;

    // Uplink RX ring
    static constexpr uint16_t RX_RING_SIZE = 256U;

    /// Free ring capacity. One slot is kept unused so head == tail always
    /// means "empty".
    [[nodiscard]] uint16_t free_bytes() const noexcept {
        const uint16_t h = this->tx_head;
        const uint16_t t = this->tx_tail;
        const auto used = static_cast<uint16_t>((h + TX_RING_SIZE - t) % TX_RING_SIZE);
        return static_cast<uint16_t>((TX_RING_SIZE - 1U) - used);
    }

    /// Start the next contiguous chunk if the line is idle. Called from
    /// both the producer (send) and the SEND_COMPLETE interrupt; the short
    /// global IRQ lock makes the idle-check-and-start atomic between them.
    void kick() noexcept {
        __disable_irq();
        if (!this->tx_active && this->tx_head != this->tx_tail) {
            const uint16_t t = this->tx_tail;
            const uint16_t h = this->tx_head;
            const auto chunk = (h > t) ? static_cast<uint16_t>(h - t) : static_cast<uint16_t>(TX_RING_SIZE - t);
            this->in_flight = chunk;
            this->tx_active = true;
            (void)this->usart.Send(&this->ring[t], chunk);
        }
        __enable_irq();
    }

    /// CMSIS SignalEvent callback (no user context -> singleton pointer,
    /// same pattern as CmsisI2CBus). Runs in interrupt context: release the
    /// transmitted chunk and chain the next one.
    static void signal_event(uint32_t event) {
        if (instance_g == nullptr) {
            return;
        }
        if ((event & ARM_USART_EVENT_SEND_COMPLETE) != 0U) {
            instance_g->tx_tail = static_cast<uint16_t>((instance_g->tx_tail + instance_g->in_flight) % TX_RING_SIZE);
            instance_g->tx_active = false;
            instance_g->kick();
        }

        if ((event & ARM_USART_EVENT_RECEIVE_COMPLETE) != 0U) {
            instance_g->rx_push(instance_g->rx_byte);
            instance_g->arm_receive();
        } else if ((event & (ARM_USART_EVENT_RX_OVERFLOW | ARM_USART_EVENT_RX_BREAK | ARM_USART_EVENT_RX_FRAMING_ERROR |
                             ARM_USART_EVENT_RX_PARITY_ERROR)) != 0U) {
            instance_g->arm_receive();
        }
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static inline Rs422Downlink* instance_g = nullptr;

    ARM_DRIVER_USART& usart; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    // Donlink TX
    std::array<uint8_t, TX_RING_SIZE> ring{};
    volatile uint16_t tx_head = 0U;
    volatile uint16_t tx_tail = 0U;
    volatile uint16_t in_flight = 0U;
    volatile bool tx_active = false;
    uint8_t consecutive_drops = 0U;

    // Uplink RX
    uint8_t rx_byte = 0U;
    std::array<uint8_t, RX_RING_SIZE> rx_ring{};
    volatile uint16_t rx_head = 0U;
    volatile uint16_t rx_tail = 0U;

  public:
    /// usart: the CMSIS-Driver USART instance (Driver_USART21 = LPUART1).
    /// Must outlive this object. Exactly one downlink exists per BTC; the
    /// constructor registers it as the SignalEvent target.
    explicit Rs422Downlink(ARM_DRIVER_USART& usart) noexcept : usart(usart) {
        instance_g = this;
    }

    Rs422Downlink(const Rs422Downlink&) = delete;
    Rs422Downlink& operator=(const Rs422Downlink&) = delete;
    Rs422Downlink(Rs422Downlink&&) = delete;
    Rs422Downlink& operator=(Rs422Downlink&&) = delete;

    /// One-time bring-up: power on, configure framing (8N1, no flow
    /// control), enable TX. Call from on_init().
    void init(uint32_t baud) noexcept {
        this->usart.Initialize(&Rs422Downlink::signal_event);
        this->usart.PowerControl(ARM_POWER_FULL);
        this->usart.Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE |
                                ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE,
                            baud);
        this->usart.Control(ARM_USART_CONTROL_TX, 1U);
        this->usart.Control(ARM_USART_CONTROL_RX, 1U);
        arm_receive();
    }

    /// Pop one received uplink byte
    [[nodiscard]] bool rx_pop(uint8_t& out) noexcept {
        if (this->rx_head == this->rx_tail) {
            return false;
        }

        out = this->rx_ring[this->rx_tail];
        this->rx_tail = static_cast<uint16_t>((this->rx_tail + 1U) % RX_RING_SIZE);
        return true;
    }

    /// Enqueue one packet for transmission and return immediately (never
    /// blocks). Drops the WHOLE packet if the ring cannot hold it and
    /// counts the drop; a later successful enqueue resets the streak.
    void send(const uint8_t* data, uint8_t len) noexcept {
        if (free_bytes() < len) {
            if (this->consecutive_drops < MAX_CONSECUTIVE_DROPS) {
                this->consecutive_drops++;
            }
            return;
        }
        this->consecutive_drops = 0U;
        uint16_t h = this->tx_head;
        for (uint8_t i = 0U; i < len; i++) {
            this->ring[h] = data[i];
            h = static_cast<uint16_t>((h + 1U) % TX_RING_SIZE);
        }
        this->tx_head = h; // publish only after the bytes are in place
        kick();
    }

    /// Latched true once MAX_CONSECUTIVE_DROPS packets in a row were
    /// dropped - the ring never drains, so the line is dead or permanently
    /// overloaded. Polled once per tick by BtcComputer to raise
    /// StatusLeds::Fault::UART.
    [[nodiscard]] bool is_failed() const noexcept {
        return this->consecutive_drops >= MAX_CONSECUTIVE_DROPS;
    }
};
