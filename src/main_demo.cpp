#include "four_line_display.h"
#include "gpio_gpiod.h"
#include "ili9488.h"
#include "spi_linux.h"
#include "st7565.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

static const char* argval(int argc, char** argv, const char* key, const char* defv) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == key && i + 1 < argc) return argv[i + 1];
    }
    return defv;
}

static int argint(int argc, char** argv, const char* key, int defv) {
    const char* v = argval(argc, argv, key, nullptr);
    return v ? std::stoi(v) : defv;
}

static bool is_ili9488_model(const std::string& model) {
    return model == "ili9488" || model == "msp3520";
}

int main(int argc, char** argv) {
    std::string dev = argval(argc, argv, "--spidev", "/dev/spidev1.0");
    std::string chip = argval(argc, argv, "--chip", "/dev/gpiochip0");
    std::string model = argval(argc, argv, "--model", "st7565");

    int dc = argint(argc, argv, "--dc", 271);
    int rst = argint(argc, argv, "--rst", 256);

    const bool use_ili9488 = is_ili9488_model(model);
    int spi_hz = argint(argc, argv, "--spi-hz", use_ili9488 ? 32000000 : 8000000);

    // The other suggested option is: "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    std::string font = argval(argc, argv, "--font", "/usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf");

    const int width = use_ili9488 ? 480 : 128;
    const int height = use_ili9488 ? 320 : 64;
    const int small_font = use_ili9488 ? 40 : 12;
    const int large_font = use_ili9488 ? 80 : 28;

    try {
        SpiLinux spi(dev);
        spi.open(static_cast<uint32_t>(spi_hz), 0);

        GpioLine dcLine(dc, true, false, chip, "demo-dc");
        GpioLine rstLine(rst, true, true, chip, "demo-rst");

        if (use_ili9488) {
            Ili9488 lcd(spi, dcLine, rstLine, width, height);
            lcd.reset();
            lcd.init();
            lcd.fill(0xF800);  // Red screen - should appear immediately
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));  // Wait 2 sec to see it
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

            int counter = 0;
            while (true) {
                display.puts(0, "Статус: Выполняется");
                display.puts(1, "Счётчик: " + std::to_string(counter));
                display.puts(2, "FuelFlux ILI9488");
                display.puts(3, "Ver 2.1");

                const auto& fb = display.render();
                lcd.set_mono_framebuffer(fb, 0xFFFF, 0x0000);

                ++counter;
                //std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        if (model != "st7565") {
            std::cerr << "Unknown model: " << model << "\n";
            std::cerr << "Supported models: st7565, ili9488 (alias: msp3520)\n";
            return 1;
        }

        St7565 lcd(spi, dcLine, rstLine);
        lcd.reset();
        lcd.init();

        FourLineDisplay display(width, height, small_font, large_font);

        if (!display.initialize(font)) {
            std::cerr << "Failed to initialize FourLineDisplay library\n";
            std::cerr << "  - Verify font exists: " << font << "\n";
            return 1;
        }

        std::cout << "Four Line Display Demo [ST7565 128x64]\n";
        std::cout << "=======================================\n";
        std::cout << "Line 0 (small): max " << display.length(0) << " chars\n";
        std::cout << "Line 1 (large): max " << display.length(1) << " chars\n";
        std::cout << "Line 2 (small): max " << display.length(2) << " chars\n";
        std::cout << "Line 3 (small): max " << display.length(3) << " chars\n";
        std::cout << "\nPress Ctrl+C to exit...\n\n";

        int counter = 0;
        while (true) {
            display.puts(0, "Status: Running");
            display.puts(1, "Count: " + std::to_string(counter));
            display.puts(2, "FuelFlux NHD");
            display.puts(3, "Ver 2.0");

            const auto& fb = display.render();
            lcd.set_framebuffer(fb);

            ++counter;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Hints:\n";
        std::cerr << "  - Ensure SPI overlay is enabled and " << dev << " exists.\n";
        std::cerr << "  - Ensure font exists: " << font << "\n";
        std::cerr << "  - Verify GPIO line offsets (libgpiod) using gpio readall/gpioinfo.\n";
        std::cerr << "  - For ILI9488 modules, verify SPI wiring and use --model ili9488.\n";
        return 1;
    }

    return 0;
}
