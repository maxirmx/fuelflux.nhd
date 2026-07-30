#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "spi_linux.h"
#include "gpio_gpiod.h"

class St7565 {
public:
    St7565(SpiLinux& spi, GpioLine& dc, GpioLine& rst, int width=128, int height=64);

    void reset();
    void init(uint8_t contrast = 0x11,
              bool reverse_segments = false,
              bool reverse_common = false);
    void set_contrast(uint8_t v);
    void display_on(bool on);

    void set_framebuffer(const std::vector<uint8_t>& fb);
    void clear();

    static std::array<uint8_t, 9> nhd_c12864a1z_init_sequence(
        uint8_t contrast = 0x11,
        bool reverse_segments = false,
        bool reverse_common = false);

private:
    void cmd(uint8_t b);
    void data(const uint8_t* p, size_t n);

    SpiLinux& spi_;
    GpioLine& dc_;
    GpioLine& rst_;
    int w_;
    int h_;
};
