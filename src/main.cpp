#include <Arduino.h>
#define LV_USE_PERF_MONITOR 0
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#include "esp_chip_info.h"
#include "esp_log.h"

/*******************************************************************************
 * Touch panel config
 ******************************************************************************/
#include "touch.h"
#include "button_4_106x40.h"

/*******************************************************************************
 * Display config - ESP32-2432S032C (CYD 3.2")
 ******************************************************************************/
#define TFT_BL 27

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else
Arduino_DataBus *bus = new Arduino_ESP32SPI(2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */);
#endif

/*******************************************************************************
 * LVGL display setup
 ******************************************************************************/
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    if (touch_has_signal())
    {
        if (touch_touched())
        {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        }
        else if (touch_released())
        {
            data->state = LV_INDEV_STATE_REL;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*******************************************************************************
 * Button grid configuration
 ******************************************************************************/
#define I2C_SLAVE_ADDR 0x20
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define NUM_ROWS 6
#define NUM_COLS 3

/* In rotation 2 (portrait), width=240, height=320.
 * Button grid: 3 columns across 240px, 6 rows across 320px. */
#define BTN_WIDTH  (SCREEN_WIDTH  / NUM_COLS)   // 80
#define BTN_HEIGHT (SCREEN_HEIGHT / NUM_ROWS)   // 53

static const char * button_labels[18] = {
    "Select", "Menu",    "Band+",
    "Zoom",   "Display", "Band-",
    "Mode",   "Demod",   "Main Incr",
    "Noise",  "Notch",   "F Tun Inc",
    "Filter", "Decode",  "Dir Freq",
    "User 1", "User 2",  "User 3"
};

static const bool is_toggle[18] = {
    true,  false, true,
    false, true,  false,
    true,  false, true,
    false, true,  false,
    true,  false, true,
    false, true,  false
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
/* Structure for button user data */
typedef struct {
    lv_obj_t * bg;
    int index;
    bool toggle_state;
} btn_data_t;

static btn_data_t btn_data[18];

static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    btn_data_t * data = (btn_data_t *)lv_event_get_user_data(e);
    int idx = data->index;

    if (is_toggle[idx]) {
        /* Toggle button: 3 colors - warm amber (inactive), orange (active), white (pressed) */
        if (code == LV_EVENT_PRESSED) {
            lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFFF8F0), 0);
        }
        else if (code == LV_EVENT_CLICKED) {
            data->toggle_state = !data->toggle_state;
            if (data->toggle_state) {
                lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFF6600), 0);
                Serial.printf("Button %d (%s) toggled -> CHECKED\n", idx, button_labels[idx]);
            } else {
                lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFFE8D0), 0);
                Serial.printf("Button %d (%s) toggled -> UNCHECKED\n", idx, button_labels[idx]);
            }
        }
        else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (data->toggle_state) {
                lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFF6600), 0);
            } else {
                lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFFE8D0), 0);
            }
        }
    } else {
        /* Momentary button: white when pressed, warm amber when released */
        if (code == LV_EVENT_PRESSED) {
            lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFFF8F0), 0);
            Serial.printf("Button %d (%s) pressed\n", idx, button_labels[idx]);
        }
        else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            lv_obj_set_style_bg_color(data->bg, lv_color_hex(0xFFE8D0), 0);
            Serial.printf("Button %d (%s) released\n", idx, button_labels[idx]);
        }
    }
}

// ────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    Serial.println("6x3 Button Grid - LVGL 8.3.11");

    detect_chip();

    // Init Display
    gfx->begin();
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    analogWrite(TFT_BL, 100);
#endif

    lv_init();
    delay(20);
    touch_init();

    screenWidth = gfx->width();
    screenHeight = gfx->height();

    Serial.printf("Screen: %dx%d\n", screenWidth, screenHeight);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

#ifdef ESP32
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 2);
#endif

    if (!disp_draw_buf)
    {
        Serial.println("LVGL disp_draw_buf allocate failed!");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 2);

    /* Initialize the display driver */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the touch input driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // I2C slave setup (commented out - enable when needed)
    // Wire.begin(I2C_SLAVE_ADDR);
    // Wire.onReceive(receiveEvent);
    // Wire.onRequest(requestEvent);

    // ── Create all 18 buttons in a 6x3 grid ──
    // Following the working project pattern: background obj + imgbtn on top
    for (int i = 0; i < 18; i++) {
        int row = i / NUM_COLS;
        int col = i % NUM_COLS;

        // Background object for color state
        lv_obj_t * bg = lv_obj_create(lv_scr_act());
        lv_obj_set_pos(bg, col * BTN_WIDTH, row * BTN_HEIGHT);
        lv_obj_set_size(bg, BTN_WIDTH, BTN_HEIGHT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0xFFE8D0), 0);  // warm amber inactive
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bg, 1, 0);
        lv_obj_set_style_border_color(bg, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_opa(bg, LV_OPA_70, 0);
        lv_obj_set_style_radius(bg, 0, 0);
        lv_obj_set_style_pad_all(bg, 0, 0);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

        // Image button on top of background
        lv_obj_t * img_btn = lv_imgbtn_create(bg);
        lv_imgbtn_set_src(img_btn, LV_IMGBTN_STATE_RELEASED, &button_4_106x40, NULL, NULL);
        lv_imgbtn_set_src(img_btn, LV_IMGBTN_STATE_PRESSED, &button_4_106x40, NULL, NULL);
        lv_obj_align(img_btn, LV_ALIGN_CENTER, 0, 0);

        // Transparent imgbtn background so bg color shows through
        lv_obj_set_style_bg_opa(img_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(img_btn, 0, 0);

        // Text label
        lv_obj_t * label = lv_label_create(img_btn);
        lv_label_set_text(label, button_labels[i]);
        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
        lv_obj_center(label);

        // Setup button data
        btn_data[i].bg = bg;
        btn_data[i].index = i;
        btn_data[i].toggle_state = false;
        buttons[i] = img_btn;

        // Event callback
        lv_obj_add_event_cb(img_btn, btn_event_cb, LV_EVENT_ALL, &btn_data[i]);
    }

    Serial.println("Setup complete - 18 buttons created");
}

void loop()
{
    lv_timer_handler();
    delay(5);
}
