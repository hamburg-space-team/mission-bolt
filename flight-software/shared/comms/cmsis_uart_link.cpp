#include "cmsis_uart_link.hpp"

CmsisUartLink::CmsisUartLink(ARM_DRIVER_USART& usart, uint8_t slot, clock_fn tick_ms, clock_fn tick_us) noexcept
    : usart(usart), slot(slot), tick_ms(tick_ms), tick_us(tick_us) {
}

bool CmsisUartLink::init(uint32_t baud) noexcept {
    if (this->slot >= MAX_LINKS) {
        return false;
    }

    instances_g[this->slot] = this;

    bool ok = this->usart.Initialize(TRAMPOLINES[this->slot]) == ARM_DRIVER_OK;
    ok = this->usart.PowerControl(ARM_POWER_FULL) == ARM_DRIVER_OK && ok;
    ok = this->usart.Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE |
                                 ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE,
                             baud) == ARM_DRIVER_OK &&
         ok;

    ok = this->usart.Control(ARM_USART_CONTROL_TX, 1U) == ARM_DRIVER_OK && ok;
    ok = this->usart.Control(ARM_USART_CONTROL_RX, 1U) == ARM_DRIVER_OK && ok;

    arm_receive();

    return ok;
}

// --- TX ---------------------------------------------------------------------

uint16_t CmsisUartLink::tx_free() const noexcept {
    // one slot stays unused so head == tail always means "empty"
    const uint16_t h = this->tx_head;
    const uint16_t t = this->tx_tail;
    const auto used = static_cast<uint16_t>((h + TX_RING_SIZE - t) % TX_RING_SIZE);
    return static_cast<uint16_t>((TX_RING_SIZE - 1U) - used);
}

bool CmsisUartLink::write(std::span<const uint8_t> data) noexcept {
    if (data.size() > tx_free()) {
        return false;
    }
    uint16_t h = this->tx_head;
    for (const uint8_t b : data) {
        this->tx_ring[h] = b;
        h = static_cast<uint16_t>((h + 1U) % TX_RING_SIZE);
    }
    this->tx_head = h; // publish only after every byte is in place
    kick();
    return true;
}

void CmsisUartLink::kick() noexcept {
    // No interrupt lock, unlike Rs422Downlink: with a single producer the
    // idle-check-and-start cannot race. A SEND_COMPLETE interrupt only
    // fires while a transfer is active, so when the producer sees
    // tx_active == false no interrupt can touch the TX state until it
    // sets tx_active itself; and the producer publishes tx_head before
    // reading tx_active, so an interrupt that clears tx_active in between
    // sees the new bytes and starts them from its own kick()
    if (this->tx_active || this->tx_head == this->tx_tail) {
        return;
    }
    const uint16_t t = this->tx_tail;
    const uint16_t h = this->tx_head;
    const auto chunk = (h > t) ? static_cast<uint16_t>(h - t) : static_cast<uint16_t>(TX_RING_SIZE - t);
    this->in_flight = chunk;
    this->tx_active = true;
    if (this->usart.Send(&this->tx_ring[t], chunk) != ARM_DRIVER_OK) {
        // nothing in flight after all; the next write() retries
        this->tx_active = false;
        this->in_flight = 0U;
    }
}

// --- RX ---------------------------------------------------------------------

void CmsisUartLink::arm_receive() noexcept {
    (void)this->usart.Receive(&this->rx_byte, 1U);
}

void CmsisUartLink::rx_push(uint8_t b) noexcept {
    const auto next = static_cast<uint16_t>((this->rx_head + 1U) % RX_RING_SIZE);
    if (next == this->rx_tail) {
        // full: drop, never block the interrupt
        if (this->rx_drops != 0xFFFFU) {
            this->rx_drops = static_cast<uint16_t>(this->rx_drops + 1U);
        }
        return;
    }
    this->rx_ring[this->rx_head] = b;
    this->rx_head = next;
}

std::size_t CmsisUartLink::read(std::span<uint8_t> out) noexcept {
    std::size_t n = 0U;
    uint16_t t = this->rx_tail;
    while (n < out.size() && t != this->rx_head) {
        out[n] = this->rx_ring[t];
        t = static_cast<uint16_t>((t + 1U) % RX_RING_SIZE);
        n++;
    }
    this->rx_tail = t;
    return n;
}

// --- interrupt side ---------------------------------------------------------

void CmsisUartLink::on_event(uint32_t event) noexcept {
    if ((event & ARM_USART_EVENT_SEND_COMPLETE) != 0U) {
        this->tx_tail = static_cast<uint16_t>((this->tx_tail + this->in_flight) % TX_RING_SIZE);
        this->tx_active = false;
        kick();
    }

    if ((event & ARM_USART_EVENT_RECEIVE_COMPLETE) != 0U) {
        rx_push(this->rx_byte);
        this->rx_ms = this->tick_ms();
        this->rx_us = this->tick_us();
        arm_receive();
    } else if ((event & (ARM_USART_EVENT_RX_OVERFLOW | ARM_USART_EVENT_RX_BREAK | ARM_USART_EVENT_RX_FRAMING_ERROR |
                         ARM_USART_EVENT_RX_PARITY_ERROR)) != 0U) {
        if (this->rx_errors != 0xFFFFU) {
            this->rx_errors = static_cast<uint16_t>(this->rx_errors + 1U);
        }
        arm_receive();
    }
}
