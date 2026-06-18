/* ============================================================================
 * test_oled.cc â€” Standalone test firmware cho OLED SSD1306 128x64 I2C
 * ----------------------------------------------------------------------------
 * Board:    ESP32-S3-WROOM-1 (N16R8)
 * OLED:     SSD1306 128x64 I2C, addr 0x3C
 * Wiring:   SDA = GPIO8, SCL = GPIO9 (I2C_NUM_0, 100 kHz)
 * Display:  Mirror X = true, Mirror Y = true (xoay 180Â°)
 * Framework: ESP-IDF v5.x (new I2C master API)
 * ----------------------------------------------------------------------------
 * CÃ¡ch dÃ¹ng:
 *   1. Copy file nÃ y vÃ o project ESP-IDF (vd main/test_oled.cc)
 *      hoáº·c táº¡o project má»›i vÃ  thay main/app_main.cc báº±ng file nÃ y.
 *   2. idf.py set-target esp32s3
 *   3. idf.py -j8 build
 *   4. idf.py -p COMx -b 921600 flash monitor
 *   5. Má»Ÿ Serial Monitor (115200 baud), gÃµ sá»‘ 0..11 + Enter Ä‘á»ƒ cháº¡y test.
 *
 * LÆ°u Ã½:
 *   - File nÃ y KHÃ”NG phá»¥ thuá»™c component OLED cá»§a xiaozhi-esp32.
 *     NÃ³ lÃ  driver SSD1306 inline + test logic, build Ä‘á»™c láº­p Ä‘Æ°á»£c.
 *   - Äá»ƒ dÃ¹ng trong project tháº­t, nÃªn #include board config Ä‘á»ƒ láº¥y SDA/SCL.
 * ========================================================================== */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"

/* ============================================================== CONFIG ===== */

#define TAG                      "TestOled"

// Match `config.h` cá»§a board xiaozhi-esp32
#define DISPLAY_SDA_PIN          GPIO_NUM_8
#define DISPLAY_SCL_PIN          GPIO_NUM_9
#define DISPLAY_I2C_NUM          I2C_NUM_0
#define DISPLAY_I2C_ADDR         0x3C
#define DISPLAY_I2C_SPEED_HZ     100000   // 100 kHz
#define DISPLAY_WIDTH            128
#define DISPLAY_HEIGHT           64
#define DISPLAY_MIRROR_X         true
#define DISPLAY_MIRROR_Y         true

// Page buffer: 1 byte = 8 vertical pixels. 128x64 â†’ 8 pages Ã— 128 bytes.
#define OLED_PAGES               (DISPLAY_HEIGHT / 8)
#define OLED_BUF_SIZE            (DISPLAY_WIDTH * OLED_PAGES)

/* ============================================================ I2C LAYER === */

static i2c_master_bus_handle_t g_i2c_bus = nullptr;
static i2c_master_dev_handle_t g_oled_dev = nullptr;

// Ghi 1 byte command
static esp_err_t oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };   // 0x00 = control byte "command stream"
    return i2c_master_transmit(g_oled_dev, buf, sizeof(buf), -1);
}

// Ghi 1 byte data
static esp_err_t oled_data(uint8_t data)
{
    uint8_t buf[2] = { 0x40, data };  // 0x40 = control byte "data stream"
    return i2c_master_transmit(g_oled_dev, buf, sizeof(buf), -1);
}

/* ============================================================ SSD1306 ===== */

// Font 5x7 ASCII (kÃ½ tá»± in Ä‘Æ°á»£c 0x20..0x7F). Má»—i glyph = 5 bytes cá»™t.
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 0x20 ' '
    {0x00,0x00,0x5F,0x00,0x00}, // 0x21 '!'
    {0x00,0x07,0x00,0x07,0x00}, // 0x22 '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // 0x23 '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // 0x24 '$'
    {0x23,0x13,0x08,0x64,0x62}, // 0x25 '%'
    {0x36,0x49,0x55,0x22,0x50}, // 0x26 '&'
    {0x00,0x05,0x03,0x00,0x00}, // 0x27 '''
    {0x00,0x1C,0x22,0x41,0x00}, // 0x28 '('
    {0x00,0x41,0x22,0x1C,0x00}, // 0x29 ')'
    {0x08,0x2A,0x1C,0x2A,0x08}, // 0x2A '*'
    {0x08,0x08,0x3E,0x08,0x08}, // 0x2B '+'
    {0x00,0x50,0x30,0x00,0x00}, // 0x2C ','
    {0x08,0x08,0x08,0x08,0x08}, // 0x2D '-'
    {0x00,0x60,0x60,0x00,0x00}, // 0x2E '.'
    {0x20,0x10,0x08,0x04,0x02}, // 0x2F '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // 0x30 '0'
    {0x00,0x42,0x7F,0x40,0x00}, // 0x31 '1'
    {0x42,0x61,0x51,0x49,0x46}, // 0x32 '2'
    {0x21,0x41,0x45,0x4B,0x31}, // 0x33 '3'
    {0x18,0x14,0x12,0x7F,0x10}, // 0x34 '4'
    {0x27,0x45,0x45,0x45,0x39}, // 0x35 '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // 0x36 '6'
    {0x01,0x71,0x09,0x05,0x03}, // 0x37 '7'
    {0x36,0x49,0x49,0x49,0x36}, // 0x38 '8'
    {0x06,0x49,0x49,0x29,0x1E}, // 0x39 '9'
    {0x00,0x36,0x36,0x00,0x00}, // 0x3A ':'
    {0x00,0x56,0x36,0x00,0x00}, // 0x3B ';'
    {0x00,0x08,0x14,0x22,0x41}, // 0x3C '<'
    {0x14,0x14,0x14,0x14,0x14}, // 0x3D '='
    {0x41,0x22,0x14,0x08,0x00}, // 0x3E '>'
    {0x02,0x01,0x51,0x09,0x06}, // 0x3F '?'
    {0x32,0x49,0x79,0x41,0x3E}, // 0x40 '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 0x41 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 0x42 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 0x43 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 0x44 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 0x45 'E'
    {0x7F,0x09,0x09,0x01,0x01}, // 0x46 'F'
    {0x3E,0x41,0x41,0x51,0x32}, // 0x47 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 0x48 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 0x49 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 0x4A 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 0x4B 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 0x4C 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 0x4D 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 0x4E 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 0x4F 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 0x50 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 0x51 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 0x52 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 0x53 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 0x54 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 0x55 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 0x56 'V'
    {0x7F,0x20,0x18,0x20,0x7F}, // 0x57 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 0x58 'X'
    {0x03,0x04,0x78,0x04,0x03}, // 0x59 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 0x5A 'Z'
};

// Framebuffer (1 = pixel ON). Index = page * 128 + column.
static uint8_t g_framebuf[OLED_BUF_SIZE];

static inline int fb_idx(int x, int y)
{
    return (y / 8) * DISPLAY_WIDTH + x;
}

static void fb_clear(void)             { memset(g_framebuf, 0, sizeof(g_framebuf)); }
static void fb_fill(void)              { memset(g_framebuf, 0xFF, sizeof(g_framebuf)); }

static void fb_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    if (on) g_framebuf[fb_idx(x, y)] |=  (1u << (y & 7));
    else    g_framebuf[fb_idx(x, y)] &= ~(1u << (y & 7));
}

static void fb_line(int x0, int y0, int x1, int y1, bool on)
{
    // Bresenham
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        fb_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

static void fb_rect(int x, int y, int w, int h, bool on, bool fill)
{
    if (fill) {
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++)
                fb_pixel(xx, yy, on);
    } else {
        fb_line(x,         y,         x + w - 1, y,         on);
        fb_line(x + w - 1, y,         x + w - 1, y + h - 1, on);
        fb_line(x + w - 1, y + h - 1, x,         y + h - 1, on);
        fb_line(x,         y + h - 1, x,         y,         on);
    }
}

static void fb_circle(int cx, int cy, int r, bool on)
{
    // Midpoint circle
    int x = r, y = 0, err = 0;
    while (x >= y) {
        fb_pixel(cx + x, cy + y, on);
        fb_pixel(cx + y, cy + x, on);
        fb_pixel(cx - y, cy + x, on);
        fb_pixel(cx - x, cy + y, on);
        fb_pixel(cx - x, cy - y, on);
        fb_pixel(cx - y, cy - x, on);
        fb_pixel(cx + y, cy - x, on);
        fb_pixel(cx + x, cy - y, on);
        if (err <= 0) { y++; err += 2 * y + 1; }
        if (err >  0) { x--; err -= 2 * x + 1; }
    }
}

static void fb_char(int x, int y, char c)
{
    if (c < 0x20 || c > 0x5A) return;   // font nÃ y chá»‰ cover 0x20..0x5A
    const uint8_t *g = font5x7[c - 0x20];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (g[col] & (1 << row))
                fb_pixel(x + col, y + row, true);
}

static void fb_text(int x, int y, const char *s)
{
    while (*s) {
        fb_char(x, y, *s++);
        x += 6;   // 5 + 1 space
        if (x > DISPLAY_WIDTH - 6) { x = 0; y += 8; }
    }
}

/* ============================================================ INIT ======== */

static esp_err_t ssd1306_init(void)
{
    // Init sequence theo datasheet SSD1306
    ESP_ERROR_CHECK(oled_cmd(0xAE));            // Display OFF
    ESP_ERROR_CHECK(oled_cmd(0xD5));            // Set display clock
    ESP_ERROR_CHECK(oled_cmd(0x80));            // default ratio
    ESP_ERROR_CHECK(oled_cmd(0xA8));            // Set multiplex
    ESP_ERROR_CHECK(oled_cmd(DISPLAY_HEIGHT - 1));
    ESP_ERROR_CHECK(oled_cmd(0xD3));            // Set display offset
    ESP_ERROR_CHECK(oled_cmd(0x00));            // offset 0
    ESP_ERROR_CHECK(oled_cmd(0x40));            // Set start line = 0
    ESP_ERROR_CHECK(oled_cmd(0x8D));            // Enable charge pump
    ESP_ERROR_CHECK(oled_cmd(0x14));
    ESP_ERROR_CHECK(oled_cmd(0x20));            // Memory addressing mode
    ESP_ERROR_CHECK(oled_cmd(0x00));            // Horizontal mode
    ESP_ERROR_CHECK(oled_cmd(0xA1 | (DISPLAY_MIRROR_X ? 0x00 : 0x01)));
    ESP_ERROR_CHECK(oled_cmd(0xC8 | (DISPLAY_MIRROR_Y ? 0x00 : 0x08)));
    ESP_ERROR_CHECK(oled_cmd(0xDA));            // COM pins
    ESP_ERROR_CHECK(oled_cmd(0x12));            // alt COM config (cho 128x64)
    ESP_ERROR_CHECK(oled_cmd(0x81));            // Contrast
    ESP_ERROR_CHECK(oled_cmd(0xCF));
    ESP_ERROR_CHECK(oled_cmd(0xD9));            // Pre-charge
    ESP_ERROR_CHECK(oled_cmd(0xF1));
    ESP_ERROR_CHECK(oled_cmd(0xDB));            // VCOMH deselect level
    ESP_ERROR_CHECK(oled_cmd(0x40));
    ESP_ERROR_CHECK(oled_cmd(0xA4));            // Resume to RAM content
    ESP_ERROR_CHECK(oled_cmd(0xA6));            // Normal (not inverted)
    ESP_ERROR_CHECK(oled_cmd(0x2E));            // Deactivate scroll
    ESP_ERROR_CHECK(oled_cmd(0xAF));            // Display ON

    fb_clear();
    return ESP_OK;
}

static esp_err_t ssd1306_flush(void)
{
    // Set column range 0..127, page range 0..7
    ESP_ERROR_CHECK(oled_cmd(0x21));
    ESP_ERROR_CHECK(oled_cmd(0x00));
    ESP_ERROR_CHECK(oled_cmd(DISPLAY_WIDTH - 1));
    ESP_ERROR_CHECK(oled_cmd(0x22));
    ESP_ERROR_CHECK(oled_cmd(0x00));
    ESP_ERROR_CHECK(oled_cmd(OLED_PAGES - 1));

    // Burst write: control byte 0x40 rá»“i 1024 byte data
    uint8_t tx[1 + OLED_BUF_SIZE];
    tx[0] = 0x40;
    memcpy(&tx[1], g_framebuf, OLED_BUF_SIZE);
    return i2c_master_transmit(g_oled_dev, tx, sizeof(tx), -1);
}

static esp_err_t ssd1306_contrast(uint8_t v)
{
    ESP_ERROR_CHECK(oled_cmd(0x81));
    return oled_cmd(v);
}

static esp_err_t ssd1306_invert(bool inv)
{
    return oled_cmd(inv ? 0xA7 : 0xA6);
}

static esp_err_t ssd1306_scroll(bool right, uint8_t start_page, uint8_t end_page)
{
    ESP_ERROR_CHECK(oled_cmd(0x2E));                         // stop scroll
    ESP_ERROR_CHECK(oled_cmd(right ? 0x26 : 0x27));          // direction
    ESP_ERROR_CHECK(oled_cmd(0x00));                         // dummy byte
    ESP_ERROR_CHECK(oled_cmd(start_page));
    ESP_ERROR_CHECK(oled_cmd(0x00));                         // speed (frames)
    ESP_ERROR_CHECK(oled_cmd(end_page));
    ESP_ERROR_CHECK(oled_cmd(0x00));
    ESP_ERROR_CHECK(oled_cmd(0xFF));
    ESP_ERROR_CHECK(oled_cmd(0x2F));                         // start
    return ESP_OK;
}

/* ========================================================== I2C INIT ===== */

static esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port      = DISPLAY_I2C_NUM;
    bus_cfg.sda_io_num    = DISPLAY_SDA_PIN;
    bus_cfg.scl_io_num    = DISPLAY_SCL_PIN;
    bus_cfg.clk_source    = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &g_i2c_bus));

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = DISPLAY_I2C_ADDR;
    dev_cfg.scl_speed_hz    = DISPLAY_I2C_SPEED_HZ;
    return i2c_master_bus_add_device(g_i2c_bus, &dev_cfg, &g_oled_dev);
}

static void i2c_scan(void)
{
    ESP_LOGI(TAG, "I2C scan on SDA=%d SCL=%d:", DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
    int found = 0;
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        if (i2c_master_probe(g_i2c_bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "  0x%02X  <-- device", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "Scan done. %d device(s) found.", found);
}

/* ============================================================== TESTS ==== */

static void test_banner(const char *title)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "TEST: %s", title);
    ESP_LOGI(TAG, "==============================================");
}

static void test_0_scan(void)
{
    test_banner("0. I2C bus scan");
    i2c_scan();
}

static void test_1_clear_fill(void)
{
    test_banner("1. Clear & fill (alternating)");
    fb_clear();  ssd1306_flush(); vTaskDelay(pdMS_TO_TICKS(600));
    fb_fill();   ssd1306_flush(); vTaskDelay(pdMS_TO_TICKS(600));
    fb_clear();  ssd1306_flush();
}

static void test_2_text(void)
{
    test_banner("2. Text rendering");
    fb_clear();
    fb_text(0,  0, "HELLO ESP32 S3");
    fb_text(0, 10, "SSD1306 128X64");
    fb_text(0, 20, "I2C 100KHZ");
    fb_text(0, 30, "GPIO8 SDA");
    fb_text(0, 40, "GPIO9 SCL");
    fb_text(0, 50, "ADDR 0X3C");
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(2000));
}

static void test_3_pixel_line_rect_circle(void)
{
    test_banner("3. Pixel / Line / Rect / Circle");
    fb_clear();

    // Pixel grid
    for (int x = 0; x < DISPLAY_WIDTH; x += 8)
        for (int y = 0; y < DISPLAY_HEIGHT; y += 8)
            fb_pixel(x, y, true);
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(800));

    // Lines from corners
    fb_clear();
    fb_line(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, true);
    fb_line(DISPLAY_WIDTH - 1, 0, 0, DISPLAY_HEIGHT - 1, true);
    fb_line(DISPLAY_WIDTH / 2, 0, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT - 1, true);
    fb_line(0, DISPLAY_HEIGHT / 2, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT / 2, true);
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(800));

    // Nested rectangles
    fb_clear();
    for (int i = 0; i < 8; i++)
        fb_rect(i * 4, i * 2, DISPLAY_WIDTH - i * 8, DISPLAY_HEIGHT - i * 4,
                true, false);
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(800));

    // Circles
    fb_clear();
    fb_circle(32, 32, 18, true);
    fb_circle(96, 32, 22, true);
    fb_circle(64, 32, 28, true);
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(1500));
}

static void test_4_mirror(void)
{
    test_banner("4. Mirror X/Y verification");
    // Váº½ chá»¯ "L" á»Ÿ gÃ³c (0,0) vÃ  "R" á»Ÿ gÃ³c (W-6,0) â€” náº¿u mirror Ä‘Ãºng
    // thÃ¬ L náº±m pháº£i-trÃªn vÃ  R náº±m trÃ¡i-trÃªn (vÃ¬ xoay 180Â°).
    fb_clear();
    fb_text(0,             0, "L");
    fb_text(DISPLAY_WIDTH - 6, 0, "R");
    fb_text(DISPLAY_WIDTH - 30, DISPLAY_HEIGHT - 8, "TOP");
    fb_text(0,              DISPLAY_HEIGHT - 8, "BOT");
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

static void test_5_contrast_sweep(void)
{
    test_banner("5. Contrast sweep (0 -> 255)");
    fb_clear();
    fb_text(0, 10, "CONTRAST TEST");
    fb_text(0, 30, "WATCH BRIGHTNESS");
    ssd1306_flush();

    int v = 0, dir = 1;
    for (int i = 0; i < 80; i++) {
        ssd1306_contrast(v);
        v += dir * 8;
        if (v >= 255) { v = 255; dir = -1; }
        if (v <= 0)   { v = 0;   dir =  1; }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ssd1306_contrast(0xCF);   // restore default
}

static void test_6_invert(void)
{
    test_banner("6. Invert (blink 3x)");
    fb_clear();
    fb_text(0, 0,  "INVERT TEST");
    fb_text(0, 20, "NORMAL");
    fb_text(0, 40, "INVERTED");
    ssd1306_flush();

    for (int i = 0; i < 3; i++) {
        ssd1306_invert(true);
        vTaskDelay(pdMS_TO_TICKS(400));
        ssd1306_invert(false);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

static void test_7_scroll(void)
{
    test_banner("7. Horizontal scroll (3s)");
    fb_clear();
    // Váº½ 1 thanh dá»c á»Ÿ giá»¯a â€” quan sÃ¡t nÃ³ cuá»™n trÃ¡i/pháº£i
    for (int y = 0; y < DISPLAY_HEIGHT; y++)
        fb_pixel(DISPLAY_WIDTH / 2, y, true);
    fb_text(20, 28, "SCROLLING");
    ssd1306_flush();

    ssd1306_scroll(true,  0, 7);
    vTaskDelay(pdMS_TO_TICKS(3000));
    ssd1306_scroll(false, 0, 7);
    vTaskDelay(pdMS_TO_TICKS(3000));
    oled_cmd(0x2E);   // stop scroll
}

static void test_8_bouncing_ball(void)
{
    test_banner("8. Bouncing ball animation (8s)");
    float x = 32, y = 32;
    float vx = 1.4f, vy = 1.1f;
    int r = 6;
    int frames = (8000 / 30);

    for (int f = 0; f < frames; f++) {
        x += vx; y += vy;
        if (x < r)            { x = r;             vx = -vx; }
        if (x > DISPLAY_WIDTH  - r - 1) { x = DISPLAY_WIDTH  - r - 1; vx = -vx; }
        if (y < r)            { y = r;             vy = -vy; }
        if (y > DISPLAY_HEIGHT - r - 1) { y = DISPLAY_HEIGHT - r - 1; vy = -vy; }

        fb_clear();
        fb_circle((int)x, (int)y, r, true);
        char fps[24];
        snprintf(fps, sizeof(fps), "F=%d/%d", f + 1, frames);
        fb_text(0, 0, fps);
        ssd1306_flush();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void test_9_progress_bar(void)
{
    test_banner("9. Progress bar animation");
    fb_clear();
    fb_rect(0, 24, DISPLAY_WIDTH, 16, true, false);
    for (int i = 0; i <= 100; i++) {
        fb_clear();
        fb_rect(0, 24, DISPLAY_WIDTH, 16, true, false);
        fb_rect(2, 26, (DISPLAY_WIDTH - 4) * i / 100, 12, true, true);
        char txt[16];
        snprintf(txt, sizeof(txt), "%d%%", i);
        fb_text(50, 0, txt);
        ssd1306_flush();
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

static void test_10_bitmap_logo(void)
{
    test_banner("10. Bitmap (checkerboard 8x8 logo)");

    // Bitmap Ä‘Æ¡n giáº£n: chá»¯ "S3" 16x32 pixel (má»—i char 8x32), MSB = pixel trÃªn
    // Má»—i cá»™t = 1 byte. Má»—i glyph = 8 cá»™t Ã— 4 byte (32 rows / 8 bits).
    static const uint8_t logo_S[8][4] = {
        {0xFC, 0xFE, 0x07, 0x03},
        {0x07, 0x07, 0x07, 0x07},
        {0x06, 0x0E, 0x1C, 0x38},
        {0xF0, 0xF8, 0x3C, 0x0E},
        {0x1F, 0x3F, 0x7E, 0x7C},
        {0x00, 0x00, 0x00, 0x00},
        {0x7F, 0x7F, 0x3F, 0x3F},
        {0x3F, 0x3F, 0x7F, 0x7F},
    };
    static const uint8_t logo_3[8][4] = {
        {0xFC, 0xFE, 0x07, 0x03},
        {0x03, 0x07, 0x0E, 0x1C},
        {0x38, 0x70, 0x60, 0xC0},
        {0xC0, 0xC0, 0xC0, 0xE0},
        {0x7F, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00},
        {0x7F, 0x7F, 0x3F, 0x3F},
        {0x3F, 0x3F, 0x7F, 0x7F},
    };

    fb_clear();
    int y_base = 16;
    for (int cx = 0; cx < 8; cx++)
        for (int page = 0; page < 4; page++) {
            uint8_t b = logo_S[cx][page];
            for (int bit = 0; bit < 8; bit++)
                if (b & (1 << bit))
                    fb_pixel(40 + cx, y_base + page * 8 + bit, true);
        }
    for (int cx = 0; cx < 8; cx++)
        for (int page = 0; page < 4; page++) {
            uint8_t b = logo_3[cx][page];
            for (int bit = 0; bit < 8; bit++)
                if (b & (1 << bit))
                    fb_pixel(80 + cx, y_base + page * 8 + bit, true);
        }
    fb_text(28, 0,  "BITMAP LOGO");
    fb_text(20, 56, "ESP32 S3 OLED");
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

static void test_11_stress(void)
{
    test_banner("11. Stress test (full RAM flush x 100)");
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < OLED_BUF_SIZE; j++) g_framebuf[j] = (uint8_t)(i ^ j);
        ssd1306_flush();
    }
    int64_t dt = esp_timer_get_time() - t0;
    ESP_LOGI(TAG, "100 flushes in %lld ms (%.2f ms/frame, %.1f FPS)",
             dt / 1000, dt / 100000.0, 100.0 * 1e6 / dt);
}

/* ===================================================== CONSOLE / MENU ===== */

static void print_menu(void)
{
    printf("\n");
    printf("==============================================\n");
    printf(" OLED SSD1306 Test Menu (%dx%d @ 0x%02X)\n",
           DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_I2C_ADDR);
    printf(" SDA=%d  SCL=%d  MirrorX=%d MirrorY=%d\n",
           DISPLAY_SDA_PIN, DISPLAY_SCL_PIN,
           DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    printf("==============================================\n");
    printf("  0. I2C bus scan\n");
    printf("  1. Clear & fill (alternating)\n");
    printf("  2. Text rendering\n");
    printf("  3. Pixel / Line / Rect / Circle\n");
    printf("  4. Mirror X/Y verification\n");
    printf("  5. Contrast sweep\n");
    printf("  6. Invert blink\n");
    printf("  7. Horizontal scroll\n");
    printf("  8. Bouncing ball animation\n");
    printf("  9. Progress bar\n");
    printf(" 10. Bitmap logo\n");
    printf(" 11. Stress test (perf)\n");
    printf("  a. Run ALL tests sequentially\n");
    printf("  r. Restart (re-init OLED)\n");
    printf("  q. Quit (just idle)\n");
    printf("----------------------------------------------\n");
    printf("> ");
}

static void run_all(void)
{
    void (*tests[])() = {
        test_0_scan, test_1_clear_fill, test_2_text,
        test_3_pixel_line_rect_circle, test_4_mirror, test_5_contrast_sweep,
        test_6_invert, test_7_scroll, test_8_bouncing_ball, test_9_progress_bar,
        test_10_bitmap_logo, test_11_stress,
    };
    int n = sizeof(tests) / sizeof(tests[0]);
    for (int i = 0; i < n; i++) {
        tests[i]();
        ESP_LOGI(TAG, "Test %d done. Pausing 1s...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void console_task(void *arg)
{
    // Unsubscribe this task from TWDT (linenoise blocks on UART read).
    esp_task_wdt_delete(NULL);
    char line[32];
    while (true) {
        print_menu();
        /* linenoise() takes only a prompt and returns a malloc'd line on success,
           or NULL on EOF (Ctrl-D). Caller must free() the returned string. */
        char *raw = linenoise("oled> ");
        if (!raw) {                                  // EOF (Ctrl-D)
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        strncpy(line, raw, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        free(raw);
        if (line[0] == '\0' || line[0] == '\n') continue;

        char c = line[0];
        if (c >= '0' && c <= '9') {
            int n = c - '0';
            switch (n) {
                case 0:  test_0_scan(); break;
                case 1:  test_1_clear_fill(); break;
                case 2:  test_2_text(); break;
                case 3:  test_3_pixel_line_rect_circle(); break;
                case 4:  test_4_mirror(); break;
                case 5:  test_5_contrast_sweep(); break;
                case 6:  test_6_invert(); break;
                case 7:  test_7_scroll(); break;
                case 8:  test_8_bouncing_ball(); break;
                case 9:  test_9_progress_bar(); break;
                case 10: test_10_bitmap_logo(); break;
                case 11: test_11_stress(); break;
            }
        } else if (c == 'a' || c == 'A') {
            run_all();
        } else if (c == 'r' || c == 'R') {
            ESP_LOGW(TAG, "Re-initializing OLED...");
            ESP_ERROR_CHECK(ssd1306_init());
        } else if (c == 'q' || c == 'Q') {
            ESP_LOGW(TAG, "Quitting menu. Device still running.");
            fb_clear(); fb_text(10, 28, "BYE!"); ssd1306_flush();
            vTaskDelete(NULL);
            return;
        } else {
            printf("Unknown command: %s\n", line);
        }
    }
}

/* ============================================================== MAIN ====== */

extern "C" void app_main(void)
{
    // Disable task watchdog entirely for this test firmware (linenoise blocks on UART).
    esp_task_wdt_deinit();

    ESP_LOGI(TAG, "Boot. ESP32-S3-WROOM-1 OLED test firmware.");
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned)esp_get_free_heap_size());

    // Init I2C + OLED
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_LOGI(TAG, "I2C bus ready (SDA=%d SCL=%d %d Hz).",
             DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, DISPLAY_I2C_SPEED_HZ);

    ESP_ERROR_CHECK(ssd1306_init());
    ESP_LOGI(TAG, "SSD1306 initialized.");

    // Init linenoise console (dÃ¹ng UART0 / USB-CDC)
    esp_console_config_t console_cfg = {
        .max_cmdline_length = 256,
        .max_cmdline_args   = 8,
        .hint_color         = 0,
    };
    ESP_ERROR_CHECK(esp_console_init(&console_cfg));
    linenoiseSetDumbMode(1);

    // Splash — Hello World
    fb_clear();
    fb_text(20,  8, "HELLO");
    fb_text(20, 24, "WORLD");
    // Khung trang trí
    for (int x = 0; x < DISPLAY_WIDTH; x++) { fb_pixel(x, 0, true); fb_pixel(x, DISPLAY_HEIGHT - 1, true); }
    for (int y = 0; y < DISPLAY_HEIGHT; y++) { fb_pixel(0, y, true); fb_pixel(DISPLAY_WIDTH - 1, y, true); }
    fb_text(22, 44, "ESP32-S3 OLED");
    fb_text(12, 56, "I2C 0x3C 100KHZ");
    ssd1306_flush();


    // Cháº¡y menu trÃªn core 1 (app core), priority tháº¥p Ä‘á»ƒ khÃ´ng block.
    xTaskCreatePinnedToCore(console_task, "oled_test_menu",
                            4096, NULL, 4, NULL, 1);
}



