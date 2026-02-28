# T41 SDR Side Button Display

This project replaces the mechanical switch array on the T41 EP-SDT software-defined radio transceiver with a touchscreen display.

## The problem

The T41 EP-SDT has 18 mechanical push buttons on its right side panel for functions like band selection, mode, filter, zoom, and more. These buttons are functional but bulky, require wiring, and offer no visual feedback.

![T41 front panel with mechanical buttons](docs/Front_panel_black_mechanical_T41.png)

## The solution

An ESP32-based CYD (Cheap Yellow Display) 3.2" touchscreen module replaces all 18 mechanical buttons with a touch-sensitive 6x3 button grid. The display is oriented in portrait mode so the button columns align vertically along the side of the transceiver.

![Touchscreen button replacement](docs/Side_buttons_LCD_T41.jpg)

## Button layout (3 columns x 6 rows)

| Col 1   | Col 2   | Col 3      |
|---------|---------|------------|
| Select  | Menu    | Band+      |
| Zoom    | Display | Band-      |
| Mode    | Demod   | Main Incr  |
| Noise   | Notch   | F Tun Inc  |
| Filter  | Decode  | Dir Freq   |
| User 1  | User 2  | User 3     |

Buttons support both momentary and toggle modes. Toggle buttons change the background color to orange when active.

## Hardware

- **Display module**: ESP32-2432S032C (CYD 3.2") with ST7789 SPI display
- **Touch controller**: GT911 capacitive touch (I2C)
- **MCU**: ESP32-D0WD-V3, 240 MHz dual core
- **Resolution**: 240x320 (portrait, rotation 2)
- **Communication**: I2C slave (MCP23017 emulation) to the T41 main board

## I2C communications

The ESP32 acts as an I2C slave on the **pico_frontpanel protocol** (compatible with g0orx/pico_frontpanel), replacing the T41's mechanical front panel.

### Wiring

The board exposes I2C on the P3 side connector:

| P3 pin | GPIO | Connect to              |
|--------|------|-------------------------|
| SDA    | 21   | Teensy pin 18 (SDA)     |
| SCL    | 22   | Teensy pin 19 (SCL)     |
| GND    | GND  | GND                     |

GPIO35 is also on P3 but is input-only and is not used.

> **No INT wire is needed.** GPIO26 (the INT output) is not accessible on P3 — it is routed to the on-board audio amplifier. The master reads the slave by polling.

### Protocol

The slave implements the pico_frontpanel register map at I2C address **0x20**:

| Register | Address | Description                              |
|----------|---------|------------------------------------------|
| CONFIG   | 0x00    | INT polarity (bit 8: 1=active-HIGH)      |
| RESET    | 0x01    | Reset                                    |
| INT_MASK | 0x02    | Pending event flags (2 bytes, read only) |
| ENCODER  | 0x03    | Encoder counts (not used)                |
| SWITCH   | 0x04    | Encoder switch (not used)                |
| TOUCH    | 0x05    | Button event: index (1B) + state (1B)    |
| LED      | 0x06    | LED control (not used)                   |

INT_MASK bits: `0x0100` = button event pending, `0x8000` = device ready.

### Polling mode

Because the INT pin is not wired to the master, the master polls REG_INT_MASK periodically (every 20 ms in the test program). If the mask is non-zero, the master reads the relevant register (REG_TOUCH for button events), which clears the flag on the slave side.

### Test program

`test/front_panel_i2c/` contains a standalone sketch for a **Heltec WiFi Kit 32** that acts as I2C master and displays button events on its built-in OLED. Only SDA, SCL and GND need to be connected.

## Software

- LVGL 8.3.11 for UI rendering
- Arduino_GFX library for display driver
- TAMC GT911 library for touch input
- Built with arduino-cli, ESP32 Arduino Core 3.3.6
