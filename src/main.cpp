#include <Arduino.h>
#define GPIO_BCKL 27
#include <esp32_smartdisplay.h>
#include <lvgl.h>
#include <Wire.h>

#include "esp_chip_info.h"
#include "esp_log.h"

/*how to use the smart-display library
in the platformio project directory do:
git clone https://github.com/rzeldent/platformio-espressif32-sunton.git boards

platforio.ini should containt:
[env:esp32-2432S032C]
platform = espressif32
board = esp32-2432S032C
framework = arduino
monitor_speed = 115200
lib_deps =
    rzeldent/esp32_smartdisplay
    lvgl/lvgl

adjust configuration file
the file lv_conf.h should be taken from the include subdirectory
it originally came from: https://github.com/nu1504ta0609sa0902/CYD35_practice_project/blob/main/_Templates/lv_conf.h
and copied into .pio/libdeps/esp32-2432S032C/lvgl

// */
#define I2C_SLAVE_ADDR 0x20  // Base address for emulated MCP23017; handles 0x20 and 0x21
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define NUM_ROWS 6
#define NUM_COLS 3
#define BTN_WIDTH  (SCREEN_WIDTH  / NUM_COLS)   // 80
#define BTN_HEIGHT (SCREEN_HEIGHT / NUM_ROWS)   // ~53

// ────────────────────────────────────────────────
//  ONE SINGLE SHARED ICON / SYMBOL IMAGE
// ────────────────────────────────────────────────
#define BUTTON_IMAGE button_3_wip_2
LV_IMAGE_DECLARE(BUTTON_IMAGE);

// Array of labels for the 18 buttons
static const char * button_labels[18] = {
    "Heat",   "Fan",    "Light",
    "Mode",   "Timer",  "Auto",
    "Power",  "Up",     "Down",
    "Menu",   "Set",    "Reset",
    "A",      "B",      "C",
    "Night",  "Day",    "Eco"
};

// Which buttons are toggle (true) vs momentary (false)
static const bool is_toggle[18] = {
    true,  false, true,
    false, true,  false,
    true,  false, true,
    false, true,  false,
    true,  false, true,
    false, true,  false
};

// Per-button active/checked color
static const lv_color_t active_colors[18] = {
    lv_color_hex(0x006600), lv_color_hex(0x006600), lv_color_hex(0x006600),
    lv_color_hex(0x006600), lv_color_hex(0x006600), lv_color_hex(0x006600),
    lv_color_hex(0x006600), lv_color_hex(0x006600), lv_color_hex(0x006600),
    lv_color_hex(0x006600), lv_color_hex(0x006600), lv_color_hex(0x006600),
    lv_color_hex(0x006600), lv_color_hex(0x006600), lv_color_hex(0x006600),
    lv_color_hex(0x006600), lv_color_hex(0x006600), lv_color_hex(0x006600)
};

// Per-button pressed color
static const lv_color_t pressed_colors[18] = {
    lv_color_hex(0x009900), lv_color_hex(0x009900), lv_color_hex(0x009900),
    lv_color_hex(0x009900), lv_color_hex(0x009900), lv_color_hex(0x009900),
    lv_color_hex(0x009900), lv_color_hex(0x009900), lv_color_hex(0x009900),
    lv_color_hex(0x009900), lv_color_hex(0x009900), lv_color_hex(0x009900),
    lv_color_hex(0x009900), lv_color_hex(0x009900), lv_color_hex(0x009900),
    lv_color_hex(0x009900), lv_color_hex(0x009900), lv_color_hex(0x009900)
};

lv_obj_t * buttons[18];
uint8_t reg = 0;

static const char *TAG = "CHIP_DETECT";

void detect_chip(void) {
    esp_chip_info_t info;
    esp_chip_info(&info);

    ESP_LOGI(TAG, "Chip model:    %d", info.model);
    ESP_LOGI(TAG, "Cores:         %d", info.cores);
    ESP_LOGI(TAG, "Revision:      v%d.%d", info.revision / 100, info.revision % 100);

    if (info.features & CHIP_FEATURE_WIFI_BGN) {
        ESP_LOGI(TAG, "Has Wi-Fi b/g/n");
    }
    if (info.features & CHIP_FEATURE_BLE) {
        ESP_LOGI(TAG, "Has BLE");
    }
    if (info.features & CHIP_FEATURE_IEEE802154) {
        ESP_LOGI(TAG, "Has 802.15.4 (Thread/Zigbee)");
    }
}

// ────────────────────────────────────────────────
// MCP23017 Emulation
// ────────────────────────────────────────────────
// We emulate two MCP23017 at 0x20 and 0x21.
// Only GPIOA (0x12) and GPIOB (0x13) reads are supported.
// Button states mapped to bits: active = 0 (low), inactive = 1 (high).
// Mapping:
// 0x20 GPIOA: buttons 0-7
// 0x20 GPIOB: buttons 8-15
// 0x21 GPIOA: buttons 16-17 (bits 0-1), rest 1
// 0x21 GPIOB: all 1 (unused)

void receiveEvent(int numBytes) {
    if (numBytes == 0) return;
    reg = Wire.read();
    while (Wire.available()) Wire.read();
}

void requestEvent() {
    uint8_t value = 0xFF;

    uint8_t states[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    for (int i = 0; i < 18; i++) {
        bool active = false;
        if (is_toggle[i]) {
            active = lv_obj_has_state(buttons[i], LV_STATE_CHECKED);
        } else {
            active = lv_obj_has_state(buttons[i], LV_STATE_PRESSED);
        }
        if (active) {
            int byte_idx = i / 8;
            int bit_idx = i % 8;
            states[byte_idx] &= ~(1 << bit_idx);
        }
    }

    if (reg == 0x12) value = states[0];
    else if (reg == 0x13) value = states[1];
    else if (reg == 0x22) value = states[2];
    else if (reg == 0x23) value = states[3];

    Wire.write(value);
}

// ────────────────────────────────────────────────
// Button event callback
// ────────────────────────────────────────────────
// For toggle buttons: LVGL handles checked state via LV_OBJ_FLAG_CHECKABLE.
// For momentary buttons: just log the click.
static void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    int idx = (intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        if (is_toggle[idx]) {
            bool checked = lv_obj_has_state(buttons[idx], LV_STATE_CHECKED);
            Serial.printf("Button %d (%s) toggled -> %s\n", idx, button_labels[idx],
                          checked ? "CHECKED" : "UNCHECKED");
        } else {
            Serial.printf("Button %d (%s) clicked\n", idx, button_labels[idx]);
        }
    }
}

// ────────────────────────────────────────────────
void setup() {
    detect_chip();
    smartdisplay_init();

    Serial.begin(115200);
    delay(1000);

    auto disp = lv_display_get_default();
    Serial.printf("LVGL resolution = %dx%d\n",
                  lv_display_get_horizontal_resolution(disp),
                  lv_display_get_vertical_resolution(disp));

    Serial.printf("PsRam size: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Partial rendering buffer - 20 lines for better image rendering
    const uint32_t lines = 20;
    static lv_color_t buf1[240 * 20];

    lv_display_set_buffers(disp,
                           buf1,
                           NULL,
                           240 * lines,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    // I2C slave setup (commented out - enable when needed)
    // Wire.begin(I2C_SLAVE_ADDR);
    // Wire.onReceive(receiveEvent);
    // Wire.onRequest(requestEvent);

    lv_obj_t * scr = lv_scr_act();

    // Dark screen background
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // ── Create all 18 buttons in a 6x3 grid
    for (int i = 0; i < 18; i++) {
        int row = i / NUM_COLS;
        int col = i % NUM_COLS;

        lv_obj_t * btn = lv_button_create(scr);
        buttons[i] = btn;

        lv_obj_set_pos(btn, col * BTN_WIDTH, row * BTN_HEIGHT);
        lv_obj_set_size(btn, BTN_WIDTH, BTN_HEIGHT);

        // Background image overlay
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_src(btn, &BUTTON_IMAGE, 0);
        lv_obj_set_style_bg_image_opa(btn, LV_OPA_COVER, 0);

        // Default (inactive) background color - dark gray
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);

        // Pressed state color
        lv_obj_set_style_bg_color(btn, pressed_colors[i], LV_STATE_PRESSED);

        if (is_toggle[i]) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
            // Checked (active) state color
            lv_obj_set_style_bg_color(btn, active_colors[i], LV_STATE_CHECKED);
        }

        // Remove default padding/radius for tight grid
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_opa(btn, LV_OPA_70, 0);

        // Text label on top
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, button_labels[i]);
        lv_obj_set_style_text_color(label, lv_color_hex(0xE8E8E8), 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
        lv_obj_center(label);

        // Event callback
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    Serial.println("Setup complete - 18 buttons created");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
