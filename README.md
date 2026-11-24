# Arduino Nano Game Center

A custom handheld arcade console built around an **8-bit Arduino Nano (ATmega328P @ 16 MHz, 2 KB SRAM)** and a **320×240 ST7789 SPI TFT display**.

This repository hosts a growing collection of classic arcade games engineered from scratch in C++ with a focus on **zero-flicker rendering**, **sub-grid smooth interpolation**, and **deterministic fixed-point math** under strict hardware memory constraints.

## Demo Video

*(Trage fișierul tău .mp4 aici, apoi șterge această linie)*

## Games Included

### 1. Snake (`games/Snake`)
A modern, Google Snake-inspired implementation featuring:
- **Zero-Flicker Atomic Tile Blitting**: Dynamic elements (head, curved 90° corners, and multi-segment tail) are composed inside a reusable **800-byte (`20×20` RGB565) atomic sprite buffer** and transferred in a single `8 MHz` SPI burst—eliminating screen tearing without exceeding the 2 KB SRAM limit.
- **Pure Integer (`int16_t`) Graphics Pipeline**: Floating-point math (`float`, `sinf`, `sqrtf`) and per-pixel divisions are completely eliminated from the render loop using `PROGMEM` lookup tables, achieving smooth ~60 FPS frame pacing on an 8-bit MCU.
- **Dynamic Multi-Segment Tail & Curved Body**: Rounded 90-degree body corners with consistent top-left directional lighting and a tapering tail that scales dynamically up to 6 segments as the snake grows.
- **Responsive Input Buffering**: Independent falling-edge button debouncing with a 3-step directional command queue so rapid consecutive turns are never dropped.
- **Non-Blocking Audio & EEPROM Persistence**: Asynchronous piezo sound effects and persistent high-score storage.

## Hardware & Pinout

All games in this repository share the same standardized hardware layout:

| Signal / Component | Arduino Nano Pin | Function |
| :--- | :---: | :--- |
| `TFT_CS` | `D10` | ST7789 SPI Chip Select |
| `TFT_DC` | `D8` | Data / Command |
| `TFT_RST` | `D9` | Hardware Reset |
| `TFT_MOSI` / `TFT_SCK` | `D11` / `D13` | Hardware SPI (`8 MHz`) |
| `B_LEFT` | `D2` | Left D-Pad button (`INPUT_PULLUP`) |
| `B_DOWN` | `D3` | Down D-Pad button (`INPUT_PULLUP`) |
| `B_RIGHT` | `D4` | Right D-Pad button (`INPUT_PULLUP`) |
| `B_UP` | `D7` | Up / Restart button (`INPUT_PULLUP`) |
| `BUZ` | `D6` | Passive/Active Piezo Buzzer |

## How to Build & Upload

1. Install **Arduino IDE** and add the following libraries via **Library Manager**:
   - `Adafruit GFX Library`
   - `Adafruit ST7789 Library`
2. Open any game folder inside `games/` (for example `games/Smooth_Snake/Smooth_Snake.ino`).
3. Select **Board: Arduino Nano** (Processor: `ATmega328P` or `ATmega328P (Old Bootloader)` depending on your board) and click **Upload**.

## Controls

- **D-Pad (`D2`, `D3`, `D4`, `D7`)**: Steer (supports queued rapid turns).
- **Left + Right simultaneously**: Pause / Resume game.
- **Up (`D7`) on Game Over**: Instant restart.

https://github.com/user-attachments/assets/a2f6cdf9-5c86-480a-8633-6d99f89bbc9a

<img width="3024" height="4032" alt="IMG_9815" src="https://github.com/user-attachments/assets/167fbce6-e121-49dd-8e3f-6800272f141a" />
<img width="3024" height="4032" alt="IMG_9816" src="https://github.com/user-attachments/assets/51e5356c-5d67-4001-9676-330b64793f44" />






