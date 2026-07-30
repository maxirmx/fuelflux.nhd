# FuelFlux LCD hardware test/demo

This repository is a bench test and demonstration project for the FuelFlux LCD
hardware. Its primary target is the Newhaven
`NHD-C12864A1Z-FSW-FBW-HTT` (128x64, ST7565P, SPI) connected to an Orange Pi
Zero 2W. The ILI9488 path is retained as a secondary display experiment.

This is not a production display service, a complete Orange Pi board-support
package, or the owner of UART, I2C, and relay control. It opens the display
interfaces, renders a four-line test screen, and makes hardware validation
repeatable.

## Verified default hardware allocation

The following allocation keeps the display clear of the interfaces already
used by the test rig. Header pin numbers are physical 40-pin connector
positions. GPIO values are the H618 numbers used as libgpiod line offsets by
the target image; confirm them with `gpioinfo` on the actual image.

| Function | Header pins | H618 signals | GPIO offsets | Notes |
| --- | --- | --- | --- | --- |
| I2C-1 / TWI1 | 3, 5 | PI8 SDA, PI7 SCL | 264, 263 | Device-node number is image-dependent |
| UART5 | 11, 13 | PH2 TX, PH3 RX | 226, 227 | Do not use these as display GPIO |
| UART2 | 15, 22 | PI5 TX, PI6 RX | 261, 262 | Shares the TWI0 pinmux; do not enable TWI0 |
| NHD SPI1 | 19, 23, 24 | PH7 MOSI, PH6 CLK, PH5 CS0 | 231, 230, 229 | `/dev/spidev1.0`; MISO is not used |
| I2C-2 / TWI2 | 27, 28 | PI10 SDA, PI9 SCL | 266, 265 | Device-node number is image-dependent |
| NHD RESET | 29 | PI0 | **256** | Default `--rst` |
| NHD A0 (D/C) | 31 | PI15 | **271** | Default `--dc` |
| Relay CH1 | 37 | PI16 | 272 | Waveshare RPi Relay Board, active low |
| Relay CH2 | 38 | PI4 | 260 | Waveshare RPi Relay Board, active low |
| Relay CH3 | 40 | PI3 | 259 | Waveshare RPi Relay Board, active low |

The former reference values `DC_LINE=262` and `RST_LINE=226` were unsafe:
they are UART2 RX and UART5 TX in this rig. The source, configuration, and this
table now consistently use offsets 271 and 256.

The relay mapping is for the three-channel **Waveshare RPi Relay Board**, not
the eight-channel Relay Board (B). The regular board's default channels use
physical pins 37, 38, and 40 and are low-active.

## NHD wiring

Connect the NHD module to SPI1 CS0 as follows:

| NHD pin | Signal | Orange Pi connection |
| ---: | --- | --- |
| 1 | SCL | Header pin 23, SPI1 CLK |
| 2 | SI | Header pin 19, SPI1 MOSI |
| 3 | VDD | Regulated 3.3 V, header pin 17 |
| 4 | A0 | Header pin 31, PI15 / GPIO 271 |
| 5 | `/RESET` | Header pin 29, PI0 / GPIO 256 |
| 6 | `/CS` | Header pin 24, SPI1 CS0 |
| 7 | VSS | Ground, for example header pin 20 |
| 8 | Heater + | External 12 V supply only when required |
| 9 | Heater - | External heater-supply ground |
| 10 | LED- | Ground |
| 11 | LED+ | 3.3 V |
| 12 | NC | Leave disconnected |

Electrical limits from the module specification:

- LCD/logic VDD: 2.8-3.3 V (3.0 V typical).
- Backlight: 3.2-3.4 V, 20-60 mA.
- Heater: 12 V nominal, 15 V maximum; required below -20 C.
- Serial clock period: at least 50 ns, so the NHD SPI clock must not exceed
  20 MHz. This demo defaults to the conservative 8 MHz.

Never connect the heater to an Orange Pi header supply. Use a suitable external
12 V source and a common ground only if low-temperature testing requires it.
Check the combined 3.3 V/5 V load when the relay board and backlight are powered
from the same header.

## Orange Pi setup

On the Orange Pi Linux images described by the board manual, open
`orangepi-config`, then select **System > Hardware** and enable:

- `spi1-cs0-spidev`
- `ph-uart5`
- `pi-uart2`
- `pi-i2c1`
- `pi-i2c2`

Reboot after saving. Do not enable `pi-i2c0`: TWI0 and UART2 use the same pins
15 and 22.

Verify the live system instead of assuming device-node numbering:

```bash
ls -l /dev/spidev1.0 /dev/gpiochip*
gpioinfo /dev/gpiochip0
i2cdetect -l
ls -l /dev/ttyS* /dev/ttyAS* 2>/dev/null
```

The manual notes that a logical I2C controller can enumerate under a different
`/dev/i2c-N` number on Linux. Use `i2cdetect -l` and the device tree on the
actual image to identify I2C-1 and I2C-2.

## Build and test

Requirements:

- CMake 3.16+
- a C++17 compiler
- libgpiod development headers
- FreeType2 development headers

Debian/Ubuntu/Armbian:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libgpiod-dev \
  libfreetype6-dev gpiod i2c-tools
```

Configure, build, and run the software tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The demo executable is `./build/lcd`.

## Run the NHD demo

Run from the repository root so the bundled default font resolves correctly:

```bash
sudo ./build/lcd
```

That command selects the NHD display, `/dev/spidev1.0`, GPIO chip 0, D/C line
271, RESET line 256, 8 MHz SPI, contrast 17, and a 500 ms update interval.
Use a finite run for a repeatable smoke test:

```bash
sudo ./build/lcd --iterations 20
```

The NHD initialization now follows the module datasheet:
`A0 AE C0 A2 2F 26 81 11 AF`. If the fixture mounts the display in the opposite
orientation, use runtime switches instead of editing the driver:

```bash
sudo ./build/lcd --flip-x --flip-y --contrast 20
```

`--contrast` accepts 0-63. The default is the datasheet value 17.

All options:

```bash
./build/lcd --help
```

## Reference configuration

[`scripts/nhd12864.conf`](scripts/nhd12864.conf) mirrors the compiled NHD
defaults for shell scripts and bench notes. The demo does not load it
automatically. To use it explicitly:

```bash
. scripts/nhd12864.conf
sudo ./build/lcd \
  --model "$MODEL" \
  --spidev "$SPIDEV" \
  --chip "$GPIOCHIP" \
  --dc "$DC_LINE" \
  --rst "$RST_LINE" \
  --spi-hz "$SPI_HZ" \
  --contrast "$CONTRAST" \
  --font "$FONT" \
  --interval-ms "$INTERVAL_MS" \
  --iterations "$ITERATIONS"
```

## ILI9488 comparison demo

The retained ILI9488/MSP3520 experiment uses the same D/C and RESET defaults:

```bash
sudo ./build/lcd --model ili9488 --spi-hz 32000000 --iterations 20
```

This path is a comparison test only; NHD is the documented hardware target.

## Code map

- `tools`: Linux spidev and libgpiod helpers.
- `lcd_display`: ST7565P/NHD and ILI9488 drivers, framebuffer drawing,
  FreeType text, and the four-line view.
- `lcd`: the hardware test/demo executable.
- `tests`: host-side framebuffer, text, and NHD initialization tests.

The display framebuffer is page-packed 1 bpp: each group of eight vertical
pixels occupies one byte per X coordinate.

## Troubleshooting

- `Failed to open spidev`: enable SPI1 CS0 and verify `/dev/spidev1.0`.
- `Failed to request output line`: check `gpioinfo`, confirm offsets 271 and
  256 are free, and make sure no other process owns them.
- Blank NHD display: verify A0, RESET, CS0, common ground, and the 3.3 V
  backlight separately; then try `--contrast` within 0-63.
- Mirrored or inverted NHD image: try `--flip-x`, `--flip-y`, or both.
- Font initialization failure: run from the repository root or pass an
  absolute path with `--font`.
- Relay switches unexpectedly: the Waveshare board is active low. The LCD demo
  never requests relay lines 272, 260, or 259.

## Source material

The hardware review used:

- `NHD-C12864A1Z-FSW-FBW-HTT.pdf`, revision 21 (Newhaven Display).
- `OrangePi_Zero2w_H618_User Manual_v1.3.pdf` (40-pin table and SPI/I2C/UART
  setup).
- [Waveshare RPi Relay Board wiki](https://www.waveshare.com/wiki/RPi_Relay_Board)
  (three-channel default pins and active-low behavior).

MIT License. See [LICENSE](LICENSE).
