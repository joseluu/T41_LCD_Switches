/*******************************************************************************
 * Front Panel I2C Master Test
 * Target: Heltec WiFi Kit 32
 *
 * Acts as I2C master talking to the ESP32 front panel slave (addr 0x20).
 * Implements the pico_frontpanel protocol (g0orx/pico_frontpanel).
 * Displays button events on the built-in SSD1306 OLED.
 *
 * Polling mode: REG_INT_MASK is read every POLL_INTERVAL_MS.
 * No INT wire required between boards.
 *
 * Wiring (Heltec WiFi Kit 32 -> ESP32 slave):
 *   GPIO 21 (SDA) -> ESP32 GPIO 21 (SDA)
 *   GPIO 22 (SCL) -> ESP32 GPIO 22 (SCL)
 *   GND           -> GND
 *
 * Libraries required:
 *   - Heltec ESP32 Dev-Boards (for OLED via Heltec.display)
 *     https://github.com/HelTecAutomation/Heltec_ESP32
 ******************************************************************************/

#include "heltec.h"
#include <Wire.h>

// ── I2C slave ──────────────────────────────────────────────────────────────
#define SLAVE_ADDR  0x20
#define SLAVE_SDA   21
#define SLAVE_SCL   22

// ── Polling interval ───────────────────────────────────────────────────────
#define POLL_INTERVAL_MS  20   // poll REG_INT_MASK every 20 ms (50 Hz)

// ── Register addresses (pico_frontpanel protocol) ──────────────────────────
#define REG_CONFIG   0x00
#define REG_RESET    0x01
#define REG_INT_MASK 0x02
#define REG_ENCODER  0x03
#define REG_SWITCH   0x04
#define REG_TOUCH    0x05  // repurposed: button_index(1B) + state(1B) + 3B pad
#define REG_LED      0x06

// ── Interrupt mask bits ────────────────────────────────────────────────────
#define INT_TS    0x0100
#define INT_READY 0x8000

// ── Button labels (must match slave) ──────────────────────────────────────
static const char *BTN_LABELS[18] = {
    "Select", "Menu",    "Band+",
    "Zoom",   "Display", "Band-",
    "Mode",   "Demod",   "MainIncr",
    "Noise",  "Notch",   "FTunInc",
    "Filter", "Decode",  "DirFreq",
    "User1",  "User2",   "User3"
};

// ── OLED display helpers ───────────────────────────────────────────────────
static char oled_line1[32] = "Waiting...";
static char oled_line2[32] = "";
static char oled_line3[32] = "";
static uint32_t event_count = 0;

void oled_refresh() {
    Heltec.display->clear();
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 0,  "FP I2C Master Test");
    Heltec.display->drawString(0, 14, oled_line1);
    Heltec.display->drawString(0, 28, oled_line2);
    Heltec.display->drawString(0, 42, oled_line3);
    Heltec.display->display();
}

// ── I2C helpers ────────────────────────────────────────────────────────────
static uint8_t read_byte() {
    while (!Wire.available()) { delayMicroseconds(10); }
    return Wire.read();
}

static uint16_t read_int_mask() {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(REG_INT_MASK);
    if (Wire.endTransmission(true) != 0) return 0;  // bus error
    Wire.requestFrom(SLAVE_ADDR, 2);
    uint16_t mask = read_byte();
    mask |= ((uint16_t)read_byte() << 8);
    return mask;
}

static void read_button_event(uint8_t *btn_index, uint8_t *btn_state) {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(REG_TOUCH);
    Wire.endTransmission(true);
    Wire.requestFrom(SLAVE_ADDR, 5);
    *btn_index = read_byte();
    *btn_state = read_byte();
    read_byte(); read_byte(); read_byte();  // discard padding
}

static void send_config() {
    // Config: int_active_high=0 (active-LOW, not used in polling mode)
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(REG_CONFIG);
    Wire.write(0x00);  // low byte
    Wire.write(0x00);  // high byte
    Wire.endTransmission(true);
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
    Heltec.begin(true /* display */, false /* LoRa */, true /* serial */);
    Heltec.display->setFont(ArialMT_Plain_10);

    Serial.println("Front Panel I2C Master Test (polling)");
    Serial.println("Connecting to slave 0x20...");

    Wire.begin(SLAVE_SDA, SLAVE_SCL);
    Wire.setClock(100000);

    delay(200);
    send_config();
    Serial.printf("Config sent. Polling every %d ms...\n", POLL_INTERVAL_MS);

    snprintf(oled_line1, sizeof(oled_line1), "Slave: 0x%02X", SLAVE_ADDR);
    snprintf(oled_line2, sizeof(oled_line2), "Poll: %dms", POLL_INTERVAL_MS);
    snprintf(oled_line3, sizeof(oled_line3), "Waiting...");
    oled_refresh();
}

// ── Loop ───────────────────────────────────────────────────────────────────
void loop() {
    static uint32_t last_poll = 0;

    if (millis() - last_poll < POLL_INTERVAL_MS) return;
    last_poll = millis();

    uint16_t mask = read_int_mask();
    if (mask == 0) return;  // nothing pending

    if (mask & INT_READY) {
        Serial.println("Slave ready signal received");
        snprintf(oled_line1, sizeof(oled_line1), "Slave READY");
        snprintf(oled_line2, sizeof(oled_line2), "");
        snprintf(oled_line3, sizeof(oled_line3), "");
        oled_refresh();
    }

    if (mask & INT_TS) {
        uint8_t btn_index = 0xFF;
        uint8_t btn_state = 0;
        read_button_event(&btn_index, &btn_state);

        event_count++;
        const char *label = (btn_index < 18) ? BTN_LABELS[btn_index] : "??";
        const char *state_str = btn_state ? "PRESS" : "REL";

        Serial.printf("Button %d (%s): %s  [event #%lu]\n",
                      btn_index, label, state_str, event_count);

        snprintf(oled_line1, sizeof(oled_line1), "#%lu", event_count);
        snprintf(oled_line2, sizeof(oled_line2), "Btn%d: %s", btn_index, label);
        snprintf(oled_line3, sizeof(oled_line3), "%s", state_str);
        oled_refresh();
    }
}
