#include "st7565.h"
#include <thread>
#include <chrono>
#include <stdexcept>

St7565::St7565(SpiLinux& spi, GpioLine& dc, GpioLine& rst, int width, int height)
    : spi_(spi), dc_(dc), rst_(rst), w_(width), h_(height) {}

void St7565::cmd(uint8_t b) { dc_.set(false); spi_.write(&b, 1); }
void St7565::data(const uint8_t* p, size_t n) { dc_.set(true); spi_.write(p, n); }

void St7565::reset() {
    rst_.set(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    rst_.set(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

std::array<uint8_t, 9> St7565::nhd_c12864a1z_init_sequence(
    uint8_t contrast, bool reverse_segments, bool reverse_common) {
    return {
        static_cast<uint8_t>(reverse_segments ? 0xA1 : 0xA0),
        0xAE,
        static_cast<uint8_t>(reverse_common ? 0xC8 : 0xC0),
        0xA2,
        0x2F,
        0x26,
        0x81,
        static_cast<uint8_t>(contrast & 0x3F),
        0xAF,
    };
}

void St7565::init(uint8_t contrast, bool reverse_segments, bool reverse_common) {
    // NHD-C12864A1Z-FSW-FBW-HTT / ST7565P initialization.
    // Defaults follow the module datasheet; the two direction bits allow the
    // test fixture to compensate for a physically rotated display.
    const auto sequence = nhd_c12864a1z_init_sequence(
        contrast, reverse_segments, reverse_common);
    for (const uint8_t value : sequence) {
        cmd(value);
    }
}

void St7565::set_contrast(uint8_t v) { cmd(0x81); cmd(v & 0x3F); }
void St7565::display_on(bool on) { cmd(on ? 0xAF : 0xAE); }

void St7565::clear() {
    std::vector<uint8_t> zeros(static_cast<size_t>(w_ * (h_/8)), 0x00);
    set_framebuffer(zeros);
}

void St7565::set_framebuffer(const std::vector<uint8_t>& fb) {
    if ((int)fb.size() != w_ * (h_/8)) throw std::runtime_error("Framebuffer size mismatch");
    for (int page = 0; page < (h_/8); ++page) {
        cmd(0xB0 | page);
        cmd(0x10);
        cmd(0x00);
        const uint8_t* row = fb.data() + (page * w_);
        data(row, (size_t)w_);
    }
}
