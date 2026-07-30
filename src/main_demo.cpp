#include "four_line_display.h"
#include "gpio_gpiod.h"
#include "ili9488.h"
#include "spi_linux.h"
#include "st7565.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

constexpr const char* kDefaultSpiDevice = "/dev/spidev1.0";
constexpr const char* kDefaultGpioChip = "/dev/gpiochip0";
constexpr const char* kDefaultFont = "fonts/unifont-17.0.03.otf";
constexpr int kDefaultDcLine = 271;   // PI15, physical pin 31
constexpr int kDefaultResetLine = 256; // PI0, physical pin 29
constexpr int kDefaultNhdSpiHz = 8000000;
constexpr int kMaxNhdSpiHz = 20000000;
constexpr int kDefaultNhdContrast = 0x11;
constexpr int kDefaultIntervalMs = 500;

const char* argval(int argc, char** argv, const char* key, const char* defv) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == key && i + 1 < argc) return argv[i + 1];
    }
    return defv;
}

int argint(int argc, char** argv, const char* key, int defv) {
    const char* v = argval(argc, argv, key, nullptr);
    return v ? std::stoi(v) : defv;
}

bool hasarg(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) return true;
    }
    return false;
}

bool is_ili9488_model(const std::string& model) {
    return model == "ili9488" || model == "msp3520";
}

bool is_nhd_model(const std::string& model) {
    return model == "nhd" || model == "nhd-c12864a1z" || model == "st7565";
}

void print_usage(const char* executable) {
    std::cout
        << "FuelFlux LCD hardware test/demo\n\n"
        << "Usage: " << executable << " [options]\n\n"
        << "  --model <nhd|ili9488>  Display under test (default: nhd)\n"
        << "  --spidev <path>        SPI device (default: /dev/spidev1.0)\n"
        << "  --spi-hz <hz>          SPI clock (default: 8000000 for NHD,\n"
        << "                         32000000 for ILI9488)\n"
        << "  --chip <path>          GPIO chip (default: /dev/gpiochip0)\n"
        << "  --dc <offset>          D/C (A0) line (default: 271 / pin 31)\n"
        << "  --rst <offset>         RESET line (default: 256 / pin 29)\n"
        << "  --contrast <0..63>     NHD electronic volume (default: 17)\n"
        << "  --flip-x               Reverse NHD segment direction\n"
        << "  --flip-y               Reverse NHD common direction\n"
        << "  --font <path>          TTF/OTF font (default: "
        << kDefaultFont << ")\n"
        << "  --interval-ms <ms>     Delay between frames (default: 500)\n"
        << "  --iterations <count>   Stop after count frames (default: 0,\n"
        << "                         run until interrupted)\n"
        << "  --help                  Show this help\n\n"
        << "Aliases: nhd-c12864a1z and st7565 select the NHD display;\n"
        << "msp3520 selects the ILI9488 display.\n";
}

void validate_options(bool use_nhd, int spi_hz, int contrast,
                      int interval_ms, int iterations) {
    if (spi_hz <= 0) {
        throw std::invalid_argument("--spi-hz must be greater than zero");
    }
    if (use_nhd && spi_hz > kMaxNhdSpiHz) {
        throw std::invalid_argument(
            "--spi-hz exceeds the NHD serial-interface limit of 20000000 Hz");
    }
    if (contrast < 0 || contrast > 0x3F) {
        throw std::invalid_argument("--contrast must be between 0 and 63");
    }
    if (interval_ms < 0) {
        throw std::invalid_argument("--interval-ms must not be negative");
    }
    if (iterations < 0) {
        throw std::invalid_argument("--iterations must not be negative");
    }
}

void wait_for_next_frame(int interval_ms) {
    if (interval_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (hasarg(argc, argv, "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    try {
        const std::string dev = argval(argc, argv, "--spidev", kDefaultSpiDevice);
        const std::string chip = argval(argc, argv, "--chip", kDefaultGpioChip);
        const std::string model = argval(argc, argv, "--model", "nhd");
        const std::string font = argval(argc, argv, "--font", kDefaultFont);

        const bool use_ili9488 = is_ili9488_model(model);
        const bool use_nhd = is_nhd_model(model);
        if (!use_ili9488 && !use_nhd) {
            throw std::invalid_argument(
                "unknown --model '" + model +
                "' (expected nhd or ili9488; see --help for aliases)");
        }

        const int dc = argint(argc, argv, "--dc", kDefaultDcLine);
        const int rst = argint(argc, argv, "--rst", kDefaultResetLine);
        const int spi_hz = argint(
            argc, argv, "--spi-hz", use_ili9488 ? 32000000 : kDefaultNhdSpiHz);
        const int contrast = argint(
            argc, argv, "--contrast", kDefaultNhdContrast);
        const int interval_ms = argint(
            argc, argv, "--interval-ms", kDefaultIntervalMs);
        const int iterations = argint(argc, argv, "--iterations", 0);
        const bool flip_x = hasarg(argc, argv, "--flip-x");
        const bool flip_y = hasarg(argc, argv, "--flip-y");

        validate_options(use_nhd, spi_hz, contrast, interval_ms, iterations);

        const int width = use_ili9488 ? 480 : 128;
        const int height = use_ili9488 ? 320 : 64;
        const int small_font = use_ili9488 ? 40 : 12;
        const int large_font = use_ili9488 ? 80 : 28;

        std::cout << "Opening " << dev << " at " << spi_hz << " Hz\n"
                  << "GPIO: " << chip << ", D/C=" << dc << ", RESET=" << rst
                  << "\n";

        SpiLinux spi(dev);
        spi.open(static_cast<uint32_t>(spi_hz), 0);

        GpioLine dcLine(dc, true, false, chip, "lcd-dc");
        GpioLine rstLine(rst, true, true, chip, "lcd-rst");

        if (use_ili9488) {
            Ili9488 lcd(spi, dcLine, rstLine, width, height);
            lcd.reset();
            lcd.init();
            lcd.fill(0xF800);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            lcd.fill(0x0000);

            FourLineDisplay display(width, height, small_font, large_font);
            if (!display.initialize(font)) {
                std::cerr << "Failed to initialize FourLineDisplay library\n";
                std::cerr << "  - Verify font exists: " << font << "\n";
                return 1;
            }

            std::cout << "Four Line Display Demo [ILI9488 480x320]\n";
            std::cout << "==========================================\n";
            std::cout << "Line 0 (small): max " << display.length(0) << " chars\n";
            std::cout << "Line 1 (large): max " << display.length(1) << " chars\n";
            std::cout << "Line 2 (small): max " << display.length(2) << " chars\n";
            std::cout << "Line 3 (small): max " << display.length(3) << " chars\n";
            std::cout << "\nPress Ctrl+C to exit...\n\n";

            for (int counter = 0;
                 iterations == 0 || counter < iterations;
                 ++counter) {
                display.puts(0, "Статус: Выполняется");
                display.puts(1, "Счётчик: " + std::to_string(counter));
                display.puts(2, "FuelFlux ILI9488");
                display.puts(3, "LCD test/demo");

                const auto& fb = display.render();
                lcd.set_mono_framebuffer(fb, 0xFFFF, 0x0000);

                if (iterations == 0 || counter + 1 < iterations) {
                    wait_for_next_frame(interval_ms);
                }
            }
            return 0;
        }

        St7565 lcd(spi, dcLine, rstLine);
        lcd.reset();
        lcd.init(
            static_cast<uint8_t>(contrast),
            flip_x,
            flip_y);
        lcd.clear();

        FourLineDisplay display(width, height, small_font, large_font);

        if (!display.initialize(font)) {
            std::cerr << "Failed to initialize FourLineDisplay library\n";
            std::cerr << "  - Verify font exists: " << font << "\n";
            return 1;
        }

        std::cout << "Four Line Display Demo [NHD-C12864A1Z / ST7565P]\n";
        std::cout << "================================================\n";
        std::cout << "Line 0 (small): max " << display.length(0) << " chars\n";
        std::cout << "Line 1 (large): max " << display.length(1) << " chars\n";
        std::cout << "Line 2 (small): max " << display.length(2) << " chars\n";
        std::cout << "Line 3 (small): max " << display.length(3) << " chars\n";
        std::cout << "Contrast: " << contrast
                  << ", flip-x: " << (flip_x ? "yes" : "no")
                  << ", flip-y: " << (flip_y ? "yes" : "no") << "\n";
        std::cout << "\nPress Ctrl+C to exit...\n\n";

        for (int counter = 0;
             iterations == 0 || counter < iterations;
             ++counter) {
            display.puts(0, "NHD TEST: OK");
            display.puts(1, "Count: " + std::to_string(counter));
            display.puts(2, "SPI1 / ST7565P");
            display.puts(3, "LCD test/demo");

            const auto& fb = display.render();
            lcd.set_framebuffer(fb);

            if (iterations == 0 || counter + 1 < iterations) {
                wait_for_next_frame(interval_ms);
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Hints:\n";
        std::cerr << "  - Run with --help to review options and compiled defaults.\n";
        std::cerr << "  - Enable the SPI1 CS0 overlay and verify /dev/spidev1.0.\n";
        std::cerr << "  - Verify the font path and GPIO ownership with gpioinfo.\n";
        std::cerr << "  - Check the consolidated wiring table in README.md.\n";
        return 1;
    }

    return 0;
}
