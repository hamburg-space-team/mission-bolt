#pragma once

#include "Driver_USART.h"
#include "uart_link.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// UartLink over one CMSIS-Driver USART instance: a TX byte ring drained
/// by the SEND_COMPLETE interrupt in contiguous chunks and an RX byte ring
/// filled one byte per RECEIVE_COMPLETE interrupt. Same scheme as the
/// BTC's Rs422Downlink, generalised to several instances per binary: the
/// CMSIS SignalEvent callback carries no user context, so every instance
/// owns one of MAX_LINKS static trampolines, selected by `slot`.
///
/// write() and read() must come from one context only (the tick loop);
/// that is what makes the rings lock-free against the interrupt side.
///
/// The board enables the peripheral's NVIC line and provides the IRQ
/// handler that calls HAL_UART_IRQHandler (CubeMX does that once the
/// interrupt is enabled in the .ioc).
///
/// @ingroup comms
class CmsisUartLink final : public UartLink {
  public:
    using clock_fn = uint32_t (*)();
    static constexpr uint8_t MAX_LINKS = 4U;

    /// slot: 0..MAX_LINKS-1, unique per instance in the binary. tick_ms /
    /// tick_us stamp received bytes from interrupt context, so they must
    /// be interrupt-safe (HAL_GetTick, Timing::us_now are)
    CmsisUartLink(ARM_DRIVER_USART& usart, uint8_t slot, clock_fn tick_ms, clock_fn tick_us) noexcept;

    /// One-time bring-up: 8N1, no flow control, TX and RX on, first
    /// receive armed. False = the CMSIS driver rejected a step; the link
    /// then never delivers and its user sees timeouts
    [[nodiscard]] bool init(uint32_t baud) noexcept;

    [[nodiscard]] bool write(std::span<const uint8_t> data) noexcept override;
    [[nodiscard]] std::size_t read(std::span<uint8_t> out) noexcept override;

    [[nodiscard]] uint32_t last_rx_ms() const noexcept override {
        return this->rx_ms;
    }
    [[nodiscard]] uint32_t last_rx_us() const noexcept override {
        return this->rx_us;
    }
    [[nodiscard]] uint16_t rx_dropped() const noexcept override {
        return this->rx_drops;
    }

    /// Framing / overrun / parity events reported by the peripheral
    /// (saturating). Bytes around such an event are suspect
    [[nodiscard]] uint16_t rx_line_errors() const noexcept {
        return this->rx_errors;
    }

  private:
    // One load script is 259 B ('\s' + 255 + '\S'); two of them plus a
    // configure script fit with room to spare. Drained at 11.5 kB/s
    static constexpr uint16_t TX_RING_SIZE = 768U;
    // One 40 ms tick carries at most 460 B at 115200; drained every tick
    static constexpr uint16_t RX_RING_SIZE = 512U;

    template <uint8_t Slot> static void trampoline(uint32_t event) noexcept {
        if (instances_g[Slot] != nullptr) {
            instances_g[Slot]->on_event(event);
        }
    }
    static constexpr std::array<ARM_USART_SignalEvent_t, MAX_LINKS> TRAMPOLINES = {&trampoline<0U>, &trampoline<1U>,
                                                                                   &trampoline<2U>, &trampoline<3U>};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static inline std::array<CmsisUartLink*, MAX_LINKS> instances_g{};

    void on_event(uint32_t event) noexcept;
    void arm_receive() noexcept;
    void rx_push(uint8_t b) noexcept;
    /// Start the next contiguous TX chunk if the line is idle. Runs from
    /// both the producer and the SEND_COMPLETE interrupt
    void kick() noexcept;
    [[nodiscard]] uint16_t tx_free() const noexcept;

    ARM_DRIVER_USART& usart; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    uint8_t slot;
    clock_fn tick_ms;
    clock_fn tick_us;

    std::array<uint8_t, TX_RING_SIZE> tx_ring{};
    volatile uint16_t tx_head = 0U; // producer (tick loop)
    volatile uint16_t tx_tail = 0U; // consumer (SEND_COMPLETE interrupt)
    volatile uint16_t in_flight = 0U;
    volatile bool tx_active = false;

    uint8_t rx_byte = 0U;
    std::array<uint8_t, RX_RING_SIZE> rx_ring{};
    volatile uint16_t rx_head = 0U; // producer (RECEIVE_COMPLETE interrupt)
    volatile uint16_t rx_tail = 0U; // consumer (tick loop)
    volatile uint32_t rx_ms = 0U;
    volatile uint32_t rx_us = 0U;
    volatile uint16_t rx_drops = 0U;
    volatile uint16_t rx_errors = 0U;
};
