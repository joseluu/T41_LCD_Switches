## 0. context — DONE 2026-01-29

Rewrote src/main.cpp so the 6x3 button grid works, using
~/hobby_w/ESP32/lvgl_button_test5 as the working reference.

### Changes made

1. Removed all `#if TEST` blocks (TEST 1-6) that prevented the 18-button
   grid from ever executing — the code was stuck on `TEST==4` (2 buttons).

2. Enabled the inactive background color (`0x2A2A2A`) — was inside `#if 0`.

3. Added `LV_STATE_CHECKED` color for toggle buttons — was never set.

4. Increased render buffer from 5 to 20 lines — 5 lines is too small for
   image-backed buttons and can cause visual artifacts.

5. Replaced multiple conflicting event handlers (`btn_cb`, `debug_btn_cb`,
   `debug_btn_cb_state`, `button_event_handler`) with a single clean
   `btn_event_cb` that relies on LVGL's built-in `LV_OBJ_FLAG_CHECKABLE`
   for toggle state management.

6. Removed manual `lv_indev_read()` polling in `loop()` — it interfered
   with LVGL's own input handling. Loop now just calls `lv_timer_handler()`.

7. Set screen background to black for proper contrast.

8. Added grid styling: zero radius, zero padding, thin borders for tight
   button layout filling the 240x320 screen.

9. Used inline per-button styles instead of shared style objects to avoid
   lifecycle issues.

### Migrated to LVGL 8.3.11 — DONE 2026-01-29 08:10

Switched from LVGL v9 (`esp32_smartdisplay`) to LVGL 8.3.11
(`Arduino_GFX` + `GT911` touch), matching the working reference project.

#### Changes made

1. Replaced `esp32_smartdisplay` library with:
   - `lvgl/lvgl@^8.3.11`
   - `moononournation/GFX Library for Arduino@^1.4.9`
   - `https://github.com/TAMCTec/gt911-arduino.git`

2. Rewrote `src/main.cpp` to use LVGL v8 API:
   - `lv_disp_drv_t` / `lv_disp_draw_buf_t` instead of `lv_display_*`
   - `lv_imgbtn_create()` with background `lv_obj_t` for color state
     (matching the working project's proven pattern)
   - `my_disp_flush()` and `my_touchpad_read()` callbacks
   - Dynamic display buffer allocation via `heap_caps_malloc`
   - Display init via `Arduino_GFX` (ST7789, rotation 3, SPI)

3. Added new source files copied from the working project:
   - `src/touch.h` — touch controller selection (GT911 active)
   - `src/touch.cpp` — touch init, read, coordinate mapping
   - `src/button_3_wip_2.h` — LVGL v8 image header (`lv_img_dsc_t`)

4. Replaced `src/button_3_wip_2.c` with the LVGL v8 version
   (`lv_img_dsc_t` / `LV_IMG_CF_RGB565A8` instead of v9's
   `lv_image_dsc_t` / `LV_COLOR_FORMAT_RGB565A8`).

5. Updated `platformio.ini`:
   - New `lib_deps` for LVGL 8, Arduino_GFX, GT911
   - `build_flags` for LV_COLOR_DEPTH=16, LV_COLOR_16_SWAP=1,
     LV_TICK_CUSTOM=1

6. Button event handler uses `btn_data_t` struct per button (matching
   the working project's `toggle_btn_data_t` pattern) for explicit
   toggle state tracking and background color management.

#### Preserved

- All button labels, toggle flags, grid layout (6x3).
- MCP23017 I2C slave emulation (still commented out, ready to enable).
- Chip detection.
- Same color scheme (dark gray inactive, green active, bright green pressed).

### Switched to arduino-cli with ESP32 Core 3.3.6 — DONE 2026-01-29 08:20

Switched build system from PlatformIO to arduino-cli. Compiled and
uploaded successfully to COM9.

#### Changes made

1. Board changed to `esp32:esp32:esp32` (ESP32 Dev Module) with
   ESP32 Arduino Core 3.3.6 (which includes `esp32-hal-periman.h`
   needed by Arduino_GFX 1.6.4).

2. Removed duplicate `button_3_wip_2.c` at project root (was also in
   `src/`) — caused multiple definition linker errors.

3. Emptied `Sdr_side_display.ino` — it was `#include "src/main.cpp"`
   which caused every symbol to be defined twice (Arduino compiles both
   the `.ino` and files in `src/` independently).

4. Simplified `src/button_3_wip_2.c` include to `#include <lvgl.h>`
   (the `__has_include` / `lvgl/lvgl.h` fallback didn't work with
   Arduino's build system for C files in `src/`).

5. Installed `lv_conf.h` from
   https://github.com/joseluu/Demo-CYD-3.2inch-ESP32-2432S032
   into `Arduino/libraries/lv_conf.h`.

#### Build result

- Sketch: 566743 bytes (43% of 1310720)
- Global variables: 73688 bytes (22% of 327680)
- Upload: COM9, ESP32-D0WD-V3 rev 3.1

## 1. remove extras — DONE 2026-01-29 18:48

1. Removed the 3 startup color test screens (red, green, blue + delays)
   from `setup()` in `src/main.cpp`.

2. Disabled FPS/CPU stats by setting `LV_USE_PERF_MONITOR` to 0 in
   `Arduino/libraries/lv_conf.h`.

## 2. improve button background colors — DONE 2026-01-29 18:50

Changed button background colors in `src/main.cpp`:

- Inactive: `0xFFE8D0` — pale warm amber (undervoltaged incandescent bulb)
- Pressed: `0xFFF8F0` — warm white (normally lit incandescent bulb)
- Active/checked: `0xFF6600` — orange (neon indicator)
