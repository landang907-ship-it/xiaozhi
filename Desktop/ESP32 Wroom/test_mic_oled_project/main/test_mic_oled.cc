// ===========================================================================
// test_mic_oled.cc — INMP441 Mic + SSD1306 OLED + Google Speech-to-Text
// ---------------------------------------------------------------------------
// Board:    ESP32-S3-WROOM-1 (N16R8)
//
// OLED:     SSD1306 128x64 I2C, addr 0x3C
//           SDA = GPIO8, SCL = GPIO9 (I2C_NUM_0)
//           Mirror X = true, Mirror Y = true (180° rotation)
//
// MIC:      INMP441 I2S MEMS microphone
//           WS (LRCLK) = GPIO4, SCK (BCLK) = GPIO5, SD (DIN) = GPIO6
//           I2S_NUM_1, 24-bit mono, 24000 Hz
//
// Function: Records mic audio → sends to Google Speech-to-Text API →
//           displays recognized text on OLED.
//           Also supports waveform/level meter display modes.
//
// Setup:
//   1. Edit WIFI_SSID, WIFI_PASS, GOOGLE_API_KEY below
//   2. Build & flash: idf.py set-target esp32s3 && idf.py build
//   3. idf.py -p COM6 -b 921600 flash monitor
//   4. In console: type "wifi <ssid> <pass>" then "speech"
// ===========================================================================

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_http_server.h"

// Wi-Fi / HTTP / JSON / Base64
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

static const char *TAG = "MicOled";

// ===========================================================================
// OLED CONFIG
// ===========================================================================
#define DISPLAY_SDA_PIN      GPIO_NUM_8
#define DISPLAY_SCL_PIN      GPIO_NUM_9
#define DISPLAY_I2C_NUM      I2C_NUM_0
#define DISPLAY_I2C_ADDR     0x3C
#define DISPLAY_WIDTH        128
#define DISPLAY_HEIGHT       64
#define DISPLAY_MIRROR_X     true
#define DISPLAY_MIRROR_Y     true

#define OLED_PAGES           (DISPLAY_HEIGHT / 8)
#define OLED_BUF_SIZE        (DISPLAY_WIDTH * OLED_PAGES)

// ===========================================================================
// MIC CONFIG
// ===========================================================================
#define MIC_BCLK             GPIO_NUM_5
#define MIC_WS               GPIO_NUM_4
#define MIC_DIN              GPIO_NUM_6
#define SAMPLE_RATE          24000
#define SLOT_BITS            I2S_DATA_BIT_WIDTH_24BIT
#define SLOT_NUM             I2S_SLOT_MODE_MONO

// ===========================================================================
// SPEECH-TO-TEXT CONFIG
// ===========================================================================
#define RECORD_SECONDS       2
#define MAX_API_KEY_LEN      128
#define MAX_LANG_LEN         16
#define WIFI_MAX_RETRY       5
#define DEFAULT_WIFI_SSID    "Thanh  long-2.4G-ext"
#define DEFAULT_WIFI_PASS    "17111976"

// ===========================================================================
// GLOBAL STATE
// ===========================================================================
static i2s_chan_handle_t          s_rx        = nullptr;
static i2c_master_bus_handle_t   g_i2c_bus   = nullptr;
static esp_lcd_panel_io_handle_t g_panel_io  = nullptr;
static esp_lcd_panel_handle_t    g_panel     = nullptr;
static uint8_t                   g_framebuf[OLED_BUF_SIZE];
static uint32_t                  g_sample_rate = SAMPLE_RATE;
static uint32_t                  g_threshold   = 2000;
static volatile bool             g_streaming   = false;
static volatile int              g_display_mode = 0;

// Speech-to-text state
static char     g_api_key[MAX_API_KEY_LEN]  = "";
static char     g_speech_lang[8] = "vi-VN";
static bool     g_wifi_connected = false;
static char     g_sta_ip[16] = "";

// ---- NVS helpers for storing WiFi credentials ----
static const char *NVS_NS = "wificfg";
static const char *NVS_KEY_SSID = "ssid";
static const char *NVS_KEY_PASS = "pass";

static esp_err_t wifi_creds_save(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, NVS_KEY_SSID, ssid);
    if (err == ESP_OK) err = nvs_set_str(h, NVS_KEY_PASS, pass);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t wifi_creds_load(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t s = ssid_len, p = pass_len;
    err = nvs_get_str(h, NVS_KEY_SSID, ssid, &s);
    if (err == ESP_OK) err = nvs_get_str(h, NVS_KEY_PASS, pass, &p);
    nvs_close(h);
    return err;
}

static void wifi_creds_clear(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
}

// Wi-Fi event group
static EventGroupHandle_t s_wifi_event_group = nullptr;
static int s_wifi_retry_num = 0;

// ===========================================================================
// FONT 5x7 (subset: 0x20..0x5A)
// ===========================================================================
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

// ===========================================================================
// OLED FRAMEBUFFER OPS
// ===========================================================================
static inline int fb_idx(int x, int y) { return (y / 8) * DISPLAY_WIDTH + x; }

static void fb_clear(void) { memset(g_framebuf, 0, sizeof(g_framebuf)); }
static void fb_fill(void)  { memset(g_framebuf, 0xFF, sizeof(g_framebuf)); }

static void fb_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    if (on) g_framebuf[fb_idx(x, y)] |=  (1u << (y & 7));
    else    g_framebuf[fb_idx(x, y)] &= ~(1u << (y & 7));
}

static void fb_line(int x0, int y0, int x1, int y1, bool on)
{
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
        fb_line(x, y, x + w - 1, y, on);
        fb_line(x + w - 1, y, x + w - 1, y + h - 1, on);
        fb_line(x + w - 1, y + h - 1, x, y + h - 1, on);
        fb_line(x, y + h - 1, x, y, on);
    }
}

static void fb_char(int x, int y, char c)
{
    if (c < 0x20 || c > 0x5A) return;
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
        x += 6;
        if (x > DISPLAY_WIDTH - 6) { x = 0; y += 8; }
    }
}

// ===========================================================================
// OLED INIT (SSD1306 via esp_lcd)
// ===========================================================================
static esp_err_t i2c_bus_init(void)
{
    if (g_i2c_bus != nullptr) return ESP_OK;
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = DISPLAY_I2C_NUM;
    bus_cfg.sda_io_num = DISPLAY_SDA_PIN;
    bus_cfg.scl_io_num = DISPLAY_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.intr_priority = 0;
    bus_cfg.trans_queue_depth = 0;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &g_i2c_bus));
    return ESP_OK;
}

static esp_err_t ssd1306_init(void)
{
    if (g_panel != nullptr) return ESP_OK;
    ESP_ERROR_CHECK(i2c_bus_init());

    const uint8_t  kAddrs[]  = { 0x3C, 0x3D };
    const uint32_t kSpeeds[] = { 100000, 400000 };

    for (uint8_t addr : kAddrs) {
        for (uint32_t speed : kSpeeds) {
            esp_lcd_panel_io_handle_t io  = nullptr;
            esp_lcd_panel_handle_t    pnl = nullptr;

            esp_lcd_panel_io_i2c_config_t io_cfg = {};
            io_cfg.dev_addr = addr;
            io_cfg.scl_speed_hz = speed;
            io_cfg.control_phase_bytes = 1;
            io_cfg.lcd_cmd_bits = 8;
            io_cfg.lcd_param_bits = 8;
            io_cfg.dc_bit_offset = 6;
            io_cfg.flags.disable_control_phase = 1;

            esp_err_t err = esp_lcd_new_panel_io_i2c(g_i2c_bus, &io_cfg, &io);
            if (err != ESP_OK) continue;

            esp_lcd_panel_ssd1306_config_t ssd_cfg = {};
            ssd_cfg.height = DISPLAY_HEIGHT;
            esp_lcd_panel_dev_config_t dev_cfg = {};
            dev_cfg.reset_gpio_num = -1;
            dev_cfg.bits_per_pixel = 1;
            dev_cfg.vendor_config = &ssd_cfg;

            err = esp_lcd_new_panel_ssd1306(io, &dev_cfg, &pnl);
            if (err != ESP_OK) { esp_lcd_panel_io_del(io); continue; }

            err = esp_lcd_panel_reset(pnl);
            if (err == ESP_OK) err = esp_lcd_panel_init(pnl);
            if (err != ESP_OK) { esp_lcd_panel_del(pnl); esp_lcd_panel_io_del(io); continue; }

            err = esp_lcd_panel_disp_on_off(pnl, true);
            if (err != ESP_OK) { esp_lcd_panel_del(pnl); esp_lcd_panel_io_del(io); continue; }

            if (DISPLAY_MIRROR_X)
                esp_lcd_panel_io_tx_param(io, 0xA1, nullptr, 0);
            else
                esp_lcd_panel_io_tx_param(io, 0xA0, nullptr, 0);
            if (DISPLAY_MIRROR_Y)
                esp_lcd_panel_io_tx_param(io, 0xC8, nullptr, 0);
            else
                esp_lcd_panel_io_tx_param(io, 0xC0, nullptr, 0);

            g_panel_io = io;
            g_panel = pnl;
            ESP_LOGI(TAG, "SSD1306 ready at 0x%02X @ %" PRIu32 " Hz", addr, speed);
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "SSD1306 not found");
    return ESP_FAIL;
}

static esp_err_t ssd1306_flush(void)
{
    if (g_panel == nullptr) return ESP_FAIL;
    return esp_lcd_panel_draw_bitmap(g_panel, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, g_framebuf);
}

// ===========================================================================
// MIC INIT (I2S RX)
// ===========================================================================
static esp_err_t mic_init(void)
{
    if (s_rx) {
        i2s_channel_disable(s_rx);
        i2s_del_channel(s_rx);
        s_rx = nullptr;
    }
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &s_rx);
    if (err != ESP_OK) { ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(err)); return err; }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(g_sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(SLOT_BITS, SLOT_NUM),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MIC_BCLK,
            .ws   = MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_DIN,
            .invert_flags = {false, false, false},
        },
    };
    // For 24-bit data, mclk_multiple must be a multiple of 3 (e.g. 384)
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    // Don't actually output MCLK since we don't use it
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    err = i2s_channel_init_std_mode(s_rx, &std_cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "i2s_channel_init_std_mode: %s", esp_err_to_name(err)); return err; }
    err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) { ESP_LOGE(TAG, "i2s_channel_enable: %s", esp_err_to_name(err)); return err; }

    ESP_LOGI(TAG, "I2S RX ready (BCLK=%d WS=%d DIN=%d @ %" PRIu32 " Hz)",
             MIC_BCLK, MIC_WS, MIC_DIN, g_sample_rate);
    return ESP_OK;
}

// ===========================================================================
// MIC READ HELPERS
// ===========================================================================
static size_t mic_read_stats(int32_t *out_peak, double *out_rms)
{
    const size_t buf_words = 512 * 2;
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) return 0;

    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(500));
    if (err != ESP_OK || read == 0) { free(buf); return 0; }

    size_t samples = read / sizeof(int32_t);
    int32_t peak = 0;
    int64_t sum_sq = 0;
    for (size_t i = 0; i < samples; i += 2) {
        int32_t raw = buf[i] >> 8;
        if (raw & 0x800000) raw |= ~0xFFFFFF;
        int32_t absv = std::abs(raw);
        if (absv > peak) peak = absv;
        sum_sq += (int64_t)raw * raw;
    }
    *out_peak = peak;
    *out_rms = sqrt((double)sum_sq / (samples / 2));
    free(buf);
    return samples / 2;
}

static void mic_read_waveform(int32_t *wave, int num_points)
{
    const int frames_per_point = 8;
    const size_t buf_words = frames_per_point * num_points * 2;
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) { memset(wave, 0, num_points * sizeof(int32_t)); return; }

    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(1000));
    if (err != ESP_OK || read == 0) { free(buf); memset(wave, 0, num_points * sizeof(int32_t)); return; }

    size_t total_samples = read / sizeof(int32_t);
    for (int i = 0; i < num_points; i++) {
        int32_t peak = 0;
        size_t start = (size_t)i * frames_per_point * 2;
        for (size_t j = 0; j < (size_t)frames_per_point * 2 && (start + j) < total_samples; j += 2) {
            int32_t raw = buf[start + j] >> 8;
            if (raw & 0x800000) raw |= ~0xFFFFFF;
            int32_t absv = std::abs(raw);
            if (absv > peak) peak = absv;
        }
        wave[i] = peak;
    }
    free(buf);
}

static double mic_estimate_freq(void)
{
    const size_t buf_words = 1024 * 2;
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) return 0.0;

    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(1000));
    if (err != ESP_OK || read == 0) { free(buf); return 0.0; }

    size_t samples = read / sizeof(int32_t);
    uint32_t zero_cross = 0;
    int32_t prev = 0;
    for (size_t i = 0; i < samples; i += 2) {
        int32_t raw = buf[i] >> 8;
        if (raw & 0x800000) raw |= ~0xFFFFFF;
        if ((prev < 0 && raw >= 0) || (prev > 0 && raw <= 0)) zero_cross++;
        prev = raw;
    }
    double freq = (double)zero_cross * g_sample_rate / (2.0 * (samples / 2));
    free(buf);
    return freq;
}

// ===========================================================================
// WI-FI FUNCTIONS
// ===========================================================================
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        if (s_wifi_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGI(TAG, "Wi-Fi reconnecting... attempt %d", s_wifi_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_num = 0;
        g_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(const char *ssid, const char *password)
{
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    // Init NVS (required by Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any_id, inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, &inst_got_ip));

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi '%s'...", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected!");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", WIFI_MAX_RETRY);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

// Start ESP32 as Access Point (AP mode) so user can connect and configure
static esp_err_t wifi_init_ap(void)
{
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {};
    strncpy((char *)ap_config.ap.ssid, "ESP32-MIC-OLED", sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen("ESP32-MIC-OLED");
    strncpy((char *)ap_config.ap.password, "12345678", sizeof(ap_config.ap.password) - 1);
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID: ESP32-MIC-OLED, PASS: 12345678");
    g_wifi_connected = true;  // AP mode is always "connected"
    return ESP_OK;
}

// ===========================================================================
// SPEECH-TO-TEXT (Google Cloud Speech-to-Text REST API)
// ===========================================================================

// Display helper: show centered text on OLED
static void oled_show_status(const char *line1, const char *line2)
{
    fb_clear();
    if (line1) {
        int len1 = strlen(line1);
        int x1 = (DISPLAY_WIDTH - len1 * 6) / 2;
        if (x1 < 0) x1 = 0;
        fb_text(x1, 20, line1);
    }
    if (line2) {
        int len2 = strlen(line2);
        int x2 = (DISPLAY_WIDTH - len2 * 6) / 2;
        if (x2 < 0) x2 = 0;
        fb_text(x2, 36, line2);
    }
    ssd1306_flush();
}

// Display recognized text on OLED with word wrap
static void oled_show_text(const char *title, const char *text)
{
    fb_clear();
    fb_text(0, 0, title);
    fb_line(0, 9, DISPLAY_WIDTH - 1, 9, true);

    int x = 0, y = 14;
    for (const char *p = text; *p && y < 56; p++) {
        if (*p == '\n' || x > 118) {
            x = 0;
            y += 9;
            if (y >= 56) break;
            if (*p == '\n') continue;
        }
        // Only render supported chars (0x20-0x5A)
        if (*p >= 0x20 && *p <= 0x5A) {
            fb_char(x, y, *p);
        } else {
            // For unsupported chars, show '?'
            fb_char(x, y, '?');
        }
        x += 6;
    }
    ssd1306_flush();
}

static esp_err_t speech_recognize_and_display(void)
{
    // --- Check prerequisites ---
    if (!g_wifi_connected) {
        ESP_LOGE(TAG, "Wi-Fi not connected. Use: wifi <ssid> <password>");
        oled_show_status("WIFI NOT", "CONNECTED");
        return ESP_ERR_NOT_FINISHED;
    }
    if (strlen(g_api_key) == 0) {
        ESP_LOGE(TAG, "API key not set. Use: setkey <key>");
        oled_show_status("NO API KEY", "USE: SETKEY");
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_rx) {
        ESP_LOGE(TAG, "Mic not initialized");
        oled_show_status("MIC NOT", "INITIALIZED");
        return ESP_ERR_INVALID_STATE;
    }

    // --- Step 1: Show recording status ---
    ESP_LOGI(TAG, "Recording %d seconds of audio...", RECORD_SECONDS);
    oled_show_status("RECORDING...", nullptr);

    // --- Step 2: Record audio from mic ---
    const size_t record_samples = g_sample_rate * RECORD_SECONDS;
    const size_t raw_buf_size = record_samples * sizeof(int32_t);
    int32_t *raw_buf = (int32_t *)heap_caps_malloc(raw_buf_size, MALLOC_CAP_SPIRAM);
    if (!raw_buf) {
        ESP_LOGE(TAG, "Failed to allocate raw buffer (%u bytes)", (unsigned)raw_buf_size);
        oled_show_status("MEM ERROR", nullptr);
        return ESP_ERR_NO_MEM;
    }

    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(s_rx, raw_buf, raw_buf_size, &bytes_read,
                                      pdMS_TO_TICKS(RECORD_SECONDS * 1000 + 2000));
    if (err != ESP_OK || bytes_read == 0) {
        ESP_LOGE(TAG, "Mic read failed: %s", esp_err_to_name(err));
        free(raw_buf);
        oled_show_status("MIC READ", "ERROR");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Recorded %u bytes (%u samples)", (unsigned)bytes_read,
             (unsigned)(bytes_read / sizeof(int32_t)));

    // --- Step 3: Convert 24-bit to 16-bit PCM ---
    size_t total_samples = bytes_read / sizeof(int32_t);
    size_t pcm_count = total_samples / 2;  // take every other sample (mono)
    int16_t *pcm_buf = (int16_t *)heap_caps_malloc(pcm_count * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!pcm_buf) {
        free(raw_buf);
        oled_show_status("MEM ERROR", nullptr);
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < total_samples; i += 2) {
        int32_t raw = raw_buf[i] >> 8;  // shift to 24-bit
        if (raw & 0x800000) raw |= ~0xFFFFFF;  // sign extend
        pcm_buf[i / 2] = (int16_t)(raw >> 8);  // convert to 16-bit
    }
    free(raw_buf);

    // --- Step 4: Base64 encode ---
    ESP_LOGI(TAG, "Encoding audio (%u bytes PCM)...", (unsigned)(pcm_count * sizeof(int16_t)));
    size_t b64_len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64_len,
                          (const unsigned char *)pcm_buf, pcm_count * sizeof(int16_t));

    char *b64_buf = (char *)heap_caps_malloc(b64_len + 1, MALLOC_CAP_SPIRAM);
    if (!b64_buf) {
        free(pcm_buf);
        oled_show_status("MEM ERROR", nullptr);
        return ESP_ERR_NO_MEM;
    }

    mbedtls_base64_encode((unsigned char *)b64_buf, b64_len + 1, &b64_len,
                          (const unsigned char *)pcm_buf, pcm_count * sizeof(int16_t));
    free(pcm_buf);
    b64_buf[b64_len] = '\0';

    ESP_LOGI(TAG, "Base64 length: %u bytes", (unsigned)b64_len);

    // --- Step 5: Build JSON request ---
    cJSON *root = cJSON_CreateObject();
    cJSON *config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddStringToObject(config, "encoding", "LINEAR16");
    cJSON_AddNumberToObject(config, "sampleRateHertz", g_sample_rate);
    cJSON_AddStringToObject(config, "languageCode", g_speech_lang);
    cJSON_AddNumberToObject(config, "maxAlternatives", 1);
    cJSON *audio = cJSON_AddObjectToObject(root, "audio");
    cJSON_AddStringToObject(audio, "content", b64_buf);
    char *json_str = cJSON_PrintUnformatted(root);
    free(b64_buf);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "JSON request size: %u bytes", (unsigned)strlen(json_str));

    // --- Step 6: Show processing status ---
    oled_show_status("PROCESSING...", nullptr);

    // --- Step 7: HTTP POST to Google Speech-to-Text API ---
    char url[256];
    snprintf(url, sizeof(url),
             "https://speech.googleapis.com/v1/speech:recognize?key=%s", g_api_key);

    esp_http_client_config_t http_cfg = {};
    http_cfg.url = url;
    http_cfg.timeout_ms = 15000;
    http_cfg.buffer_size = 4096;
    http_cfg.buffer_size_tx = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        ESP_LOGE(TAG, "HTTP client init failed");
        free(json_str);
        oled_show_status("HTTP INIT", "ERROR");
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    err = esp_http_client_open(client, strlen(json_str));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        free(json_str);
        esp_http_client_cleanup(client);
        oled_show_status("HTTP OPEN", "ERROR");
        return err;
    }

    int wlen = esp_http_client_write(client, json_str, strlen(json_str));
    free(json_str);
    if (wlen < 0) {
        ESP_LOGE(TAG, "HTTP write failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        oled_show_status("HTTP WRITE", "ERROR");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "HTTP wrote %d bytes", wlen);

    // --- Step 8: Read response ---
    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status: %d, content-length: %d", status, content_length);

    // Allocate response buffer
    int resp_buf_size = (content_length > 0) ? content_length + 1 : 4096;
    if (resp_buf_size > 16384) resp_buf_size = 16384;  // cap at 16KB
    char *response_buf = (char *)heap_caps_malloc(resp_buf_size, MALLOC_CAP_SPIRAM);
    if (!response_buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        oled_show_status("MEM ERROR", nullptr);
        return ESP_ERR_NO_MEM;
    }

    int read_len = esp_http_client_read(client, response_buf, resp_buf_size - 1);
    if (read_len < 0) read_len = 0;
    response_buf[read_len] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "Response (%d bytes): %s", read_len, response_buf);

    // --- Step 9: Parse JSON response ---
    const char *text = "NO SPEECH";
    cJSON *resp = cJSON_Parse(response_buf);
    free(response_buf);

    if (resp) {
        // Check for error
        cJSON *error = cJSON_GetObjectItem(resp, "error");
        if (error) {
            cJSON *message = cJSON_GetObjectItem(error, "message");
            if (message && message->valuestring) {
                text = message->valuestring;
                ESP_LOGE(TAG, "API error: %s", text);
            }
        } else {
            // Extract transcription
            cJSON *results = cJSON_GetObjectItem(resp, "results");
            if (results && cJSON_GetArraySize(results) > 0) {
                cJSON *first = cJSON_GetArrayItem(results, 0);
                cJSON *alts = cJSON_GetObjectItem(first, "alternatives");
                if (alts && cJSON_GetArraySize(alts) > 0) {
                    cJSON *alt = cJSON_GetArrayItem(alts, 0);
                    cJSON *transcript = cJSON_GetObjectItem(alt, "transcript");
                    if (transcript && transcript->valuestring) {
                        text = transcript->valuestring;
                    }
                }
            }
        }
    }

    // --- Step 10: Display result on OLED ---
    ESP_LOGI(TAG, "Recognized: %s", text);
    oled_show_text(">> SPEECH <<", text);

    if (resp) cJSON_Delete(resp);
    return ESP_OK;
}

// ===========================================================================
// DISPLAY MODES — stream mic data to OLED
// ===========================================================================

static void display_waveform(void)
{
    const int W = DISPLAY_WIDTH;
    const int H = DISPLAY_HEIGHT;
    int32_t wave[W];

    mic_read_waveform(wave, W);

    int32_t max_val = 1;
    for (int i = 0; i < W; i++)
        if (wave[i] > max_val) max_val = wave[i];

    fb_clear();
    fb_line(0, H / 2, W - 1, H / 2, true);

    int prev_y = H / 2;
    for (int x = 0; x < W; x++) {
        int y = H / 2 - (int)((int64_t)wave[x] * (H / 2 - 2) / max_val);
        if (y < 0) y = 0;
        if (y >= H) y = H - 1;
        fb_line(x, prev_y, x, y, true);
        prev_y = y;
    }

    fb_text(0, 0, "WAVE");
    ssd1306_flush();
}

static void display_level_meter(void)
{
    int32_t peak = 0;
    double rms = 0.0;
    mic_read_stats(&peak, &rms);

    fb_clear();

    int bar_w = (int)((int64_t)peak * (DISPLAY_WIDTH - 2) / 8388607);
    if (bar_w > DISPLAY_WIDTH - 2) bar_w = DISPLAY_WIDTH - 2;
    if (bar_w < 0) bar_w = 0;
    fb_rect(1, 1, bar_w, 12, true, true);
    fb_rect(1, 1, DISPLAY_WIDTH - 2, 12, true, false);

    int rms_bar = (int)(rms * (DISPLAY_WIDTH - 2) / 8388607);
    if (rms_bar > DISPLAY_WIDTH - 2) rms_bar = DISPLAY_WIDTH - 2;
    if (rms_bar < 0) rms_bar = 0;
    fb_rect(1, 18, rms_bar, 12, true, true);
    fb_rect(1, 18, DISPLAY_WIDTH - 2, 12, true, false);

    fb_text(0, 34, "PEAK");
    fb_text(0, 44, "RMS");

    char buf[24];
    snprintf(buf, sizeof(buf), "%" PRId32, peak);
    fb_text(40, 34, buf);

    snprintf(buf, sizeof(buf), "%.0f", rms);
    fb_text(40, 44, buf);

    int thresh_x = (int)((int64_t)g_threshold * (DISPLAY_WIDTH - 2) / 8388607);
    if (thresh_x > 0 && thresh_x < DISPLAY_WIDTH) {
        fb_line(thresh_x, 0, thresh_x, 30, true);
    }

    ssd1306_flush();
}

static void display_combined(void)
{
    const int W = DISPLAY_WIDTH;
    const int wave_h = 32;
    const int meter_h = 32;

    int32_t wave[W];
    mic_read_waveform(wave, W);

    int32_t peak = 0;
    for (int i = 0; i < W; i++)
        if (wave[i] > peak) peak = wave[i];

    fb_clear();

    int32_t max_val = 1;
    for (int i = 0; i < W; i++)
        if (wave[i] > max_val) max_val = wave[i];

    int prev_y = wave_h / 2;
    for (int x = 0; x < W; x++) {
        int y = wave_h / 2 - (int)((int64_t)wave[x] * (wave_h / 2 - 1) / max_val);
        if (y < 0) y = 0;
        if (y >= wave_h) y = wave_h - 1;
        fb_pixel(x, y, true);
        prev_y = y;
    }

    fb_line(0, wave_h, W - 1, wave_h, true);

    int bar_w = (int)((int64_t)peak * (W - 2) / 8388607);
    if (bar_w > W - 2) bar_w = W - 2;
    if (bar_w < 0) bar_w = 0;
    fb_rect(1, wave_h + 2, bar_w, 10, true, true);
    fb_rect(1, wave_h + 2, W - 2, 10, true, false);

    char buf[24];
    snprintf(buf, sizeof(buf), "PK:%" PRId32, peak);
    fb_text(0, wave_h + 14, buf);

    static int freq_counter = 0;
    static double last_freq = 0;
    if (++freq_counter >= 5) {
        freq_counter = 0;
        last_freq = mic_estimate_freq();
    }
    snprintf(buf, sizeof(buf), "%.0fHZ", last_freq);
    fb_text(0, wave_h + 24, buf);

    ssd1306_flush();
}

// ===========================================================================
// STREAMING TASK — runs on core 1
// ===========================================================================
static void stream_task(void *arg)
{
    esp_task_wdt_delete(NULL);
    ESP_LOGI(TAG, "Stream task started on core 1");

    while (g_streaming) {
        switch (g_display_mode) {
            case 0: display_waveform();    break;
            case 1: display_level_meter(); break;
            case 2: display_combined();    break;
            default: display_waveform();   break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGI(TAG, "Stream task stopped");
    vTaskDelete(NULL);
}

// ===========================================================================
// REPL COMMANDS
// ===========================================================================
static int cmd_init(int, char**) { return mic_init(); }

static int cmd_rate(int argc, char** argv) {
    uint32_t hz = (argc > 1) ? (uint32_t)atoi(argv[1]) : SAMPLE_RATE;
    g_sample_rate = hz;
    ESP_LOGI(TAG, "Set sample rate %" PRIu32 " Hz, re-init", hz);
    return mic_init();
}

static int cmd_stream(int argc, char** argv) {
    if (argc > 1) {
        int mode = atoi(argv[1]);
        if (mode < 0 || mode > 2) { ESP_LOGE(TAG, "Mode 0-2 only"); return 1; }
        g_display_mode = mode;
    }
    if (!g_streaming) {
        g_streaming = true;
        xTaskCreatePinnedToCore(stream_task, "stream", 8192, NULL, 5, NULL, 1);
        ESP_LOGI(TAG, "Streaming started (mode %d)", g_display_mode);
    } else {
        ESP_LOGI(TAG, "Already streaming (mode %d). Use 'stop' first.", g_display_mode);
    }
    return 0;
}

static int cmd_stop(int, char**) {
    if (g_streaming) {
        g_streaming = false;
        ESP_LOGI(TAG, "Stopping stream...");
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ESP_LOGI(TAG, "Not streaming");
    }
    return 0;
}

static int cmd_mode(int argc, char** argv) {
    if (argc > 1) {
        int m = atoi(argv[1]);
        if (m >= 0 && m <= 2) {
            g_display_mode = m;
            ESP_LOGI(TAG, "Display mode: %d (%s)", m,
                     m == 0 ? "waveform" : m == 1 ? "level meter" : "combined");
        } else {
            ESP_LOGE(TAG, "Mode 0-2 only");
        }
    } else {
        ESP_LOGI(TAG, "Current mode: %d (%s)", g_display_mode,
                 g_display_mode == 0 ? "waveform" : g_display_mode == 1 ? "level meter" : "combined");
    }
    return 0;
}

static int cmd_thresh(int argc, char** argv) {
    if (argc > 1) g_threshold = (uint32_t)atoi(argv[1]);
    ESP_LOGI(TAG, "Threshold: %" PRIu32, g_threshold);
    return 0;
}

static int cmd_read(int argc, char** argv) {
    uint32_t frames = (argc > 1) ? (uint32_t)atoi(argv[1]) : 1024;
    const size_t buf_words = frames * 2;
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) { ESP_LOGE(TAG, "malloc fail"); return 1; }
    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(2000));
    if (err != ESP_OK) { ESP_LOGE(TAG, "read err: %s", esp_err_to_name(err)); free(buf); return 1; }
    size_t samples = read / sizeof(int32_t);
    int32_t peak = 0;
    int64_t sum_sq = 0;
    for (size_t i = 0; i < samples; i += 2) {
        int32_t raw = buf[i] >> 8;
        if (raw & 0x800000) raw |= ~0xFFFFFF;
        int32_t absv = std::abs(raw);
        if (absv > peak) peak = absv;
        sum_sq += (int64_t)raw * raw;
    }
    double rms = sqrt((double)sum_sq / (samples / 2));
    ESP_LOGI(TAG, "Read %" PRIu32 " frames, peak=%" PRId32 ", RMS=%.1f", frames, peak, rms);
    free(buf);
    return 0;
}

static int cmd_fft(int, char**) {
    double freq = mic_estimate_freq();
    ESP_LOGI(TAG, "Freq estimate: %.1f Hz", freq);
    return 0;
}

static int cmd_hello(int, char**) {
    fb_clear();
    fb_text(20, 8, "HELLO");
    fb_text(10, 24, "MIC + OLED");
    fb_text(10, 40, "COMBINED!");
    fb_text(10, 56, "ESP32-S3");
    ssd1306_flush();
    ESP_LOGI(TAG, "Hello displayed on OLED");
    return 0;
}

static int cmd_cls(int, char**) {
    fb_clear();
    ssd1306_flush();
    return 0;
}

// --- NEW COMMANDS: Wi-Fi & Speech-to-Text ---

static int cmd_wifi(int argc, char** argv) {
    if (argc < 3) {
        ESP_LOGI(TAG, "Usage: wifi <ssid> <password>");
        ESP_LOGI(TAG, "Current status: %s", g_wifi_connected ? "CONNECTED" : "DISCONNECTED");
        return 0;
    }
    oled_show_status("WIFI CONNECT", argv[1]);
    esp_err_t err = wifi_init_sta(argv[1], argv[2]);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi connected!");
        oled_show_status("WIFI OK!", nullptr);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed");
        oled_show_status("WIFI FAILED", nullptr);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return 0;
}

static int cmd_setkey(int argc, char** argv) {
    if (argc < 2) {
        ESP_LOGI(TAG, "Usage: setkey <google_api_key>");
        ESP_LOGI(TAG, "Current key: %s", strlen(g_api_key) > 0 ? "(set)" : "(empty)");
        return 0;
    }
    strncpy(g_api_key, argv[1], sizeof(g_api_key) - 1);
    g_api_key[sizeof(g_api_key) - 1] = '\0';
    ESP_LOGI(TAG, "API key set (%d chars)", (int)strlen(g_api_key));
    return 0;
}

static int cmd_lang(int argc, char** argv) {
    if (argc < 2) {
        ESP_LOGI(TAG, "Usage: lang <language_code>");
        ESP_LOGI(TAG, "Current: %s", g_speech_lang);
        ESP_LOGI(TAG, "Examples: vi-VN, en-US, ja-JP, ko-KR, zh-CN");
        return 0;
    }
    strncpy(g_speech_lang, argv[1], sizeof(g_speech_lang) - 1);
    g_speech_lang[sizeof(g_speech_lang) - 1] = '\0';
    ESP_LOGI(TAG, "Language set to: %s", g_speech_lang);
    return 0;
}

static int cmd_speech(int, char**) {
    return speech_recognize_and_display();
}

// ===========================================================================
// WEB SERVER — HTTP API for Wi-Fi, API key, language, speech
// ===========================================================================

static httpd_handle_t s_web_server = nullptr;

// HTML page for configuration
static const char HTML_PAGE[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 Mic + OLED</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;padding:16px}
h1{text-align:center;color:#e94560;margin:16px 0;font-size:1.5em}
.card{background:#16213e;border-radius:12px;padding:16px;margin:12px 0;box-shadow:0 4px 12px rgba(0,0,0,.3)}
.card h2{color:#0f3460;font-size:1.1em;margin-bottom:10px;color:#e94560}
label{display:block;margin:8px 0 4px;font-size:.9em;color:#aaa}
input[type=text],input[type=password],select{width:100%;padding:10px;border:1px solid #333;border-radius:8px;background:#0f3460;color:#fff;font-size:1em}
button{width:100%;padding:12px;margin:8px 0;border:none;border-radius:8px;font-size:1em;font-weight:bold;cursor:pointer;transition:.2s}
.btn-wifi{background:#e94560;color:#fff}
.btn-key{background:#0f3460;color:#fff}
.btn-lang{background:#533483;color:#fff}
.btn-speech{background:#e94560;color:#fff;font-size:1.2em;padding:16px}
button:hover{opacity:.85;transform:scale(1.02)}
.status{margin-top:10px;padding:10px;border-radius:8px;background:#0f3460;font-size:.9em;min-height:40px;word-break:break-all}
.ok{border-left:4px solid #4caf50}
.err{border-left:4px solid #f44336}
.result{font-size:1.1em;color:#4caf50;font-weight:bold}
</style>
</head>
<body>
<h1>&#127908; ESP32-S3 Mic + OLED</h1>

<div class="card">
<h2>&#128246; Wi-Fi</h2>
<label>SSID</label>
<input type="text" id="ssid" placeholder="Ten mang WiFi">
<label>Password</label>
<input type="password" id="pass" placeholder="Mat khau">
<button class="btn-wifi" onclick="setWifi()">Ket noi WiFi</button>
<div id="wifiStatus" class="status"></div>
</div>

<div class="card">
<h2>&#128273; Google API Key</h2>
<label>API Key</label>
<input type="text" id="apikey" placeholder="Google Speech-to-Text API Key">
<button class="btn-key" onclick="setKey()">Dat API Key</button>
<div id="keyStatus" class="status"></div>
</div>

<div class="card">
<h2>&#127760; Ngon ngu</h2>
<select id="lang">
<option value="vi-VN">Tieng Viet (vi-VN)</option>
<option value="en-US">English (en-US)</option>
<option value="ja-JP">Japanese (ja-JP)</option>
<option value="ko-KR">Korean (ko-KR)</option>
<option value="zh-CN">Chinese (zh-CN)</option>
<option value="fr-FR">French (fr-FR)</option>
<option value="de-DE">German (de-DE)</option>
<option value="es-ES">Spanish (es-ES)</option>
</select>
<button class="btn-lang" onclick="setLang()">Dat ngon ngu</button>
<div id="langStatus" class="status"></div>
</div>

<div class="card">
<h2>&#127908; Nhan dien giong noi</h2>
<button class="btn-speech" onclick="doSpeech()">&#9654; Ghi am & Nhan dien</button>
<div id="speechStatus" class="status"></div>
</div>

<script>
function show(id,cls,html){var e=document.getElementById(id);e.className='status '+cls;e.innerHTML=html}
function setWifi(){
  var s=document.getElementById('ssid').value,p=document.getElementById('pass').value;
  if(!s){show('wifiStatus','err','Nhap SSID!');return}
  show('wifiStatus','ok','Dang ket noi...');
  fetch('/api/wifi?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))
    .then(r=>r.json()).then(d=>show('wifiStatus',d.ok?'ok':'err',d.msg))
    .catch(e=>show('wifiStatus','err','Loi: '+e));
}
function setKey(){
  var k=document.getElementById('apikey').value;
  if(!k){show('keyStatus','err','Nhap API Key!');return}
  show('keyStatus','ok','Dang luu...');
  fetch('/api/key?key='+encodeURIComponent(k))
    .then(r=>r.json()).then(d=>show('keyStatus',d.ok?'ok':'err',d.msg))
    .catch(e=>show('keyStatus','err','Loi: '+e));
}
function setLang(){
  var l=document.getElementById('lang').value;
  show('langStatus','ok','Dang dat...');
  fetch('/api/lang?lang='+encodeURIComponent(l))
    .then(r=>r.json()).then(d=>show('langStatus',d.ok?'ok':'err',d.msg))
    .catch(e=>show('langStatus','err','Loi: '+e));
}
function doSpeech(){
  show('speechStatus','ok','Dang ghi am 2s...');
  fetch('/api/speech')
    .then(r=>r.json()).then(d=>{
      if(d.ok) show('speechStatus','ok','<span class="result">'+d.text+'</span>');
      else show('speechStatus','err',d.msg);
    }).catch(e=>show('speechStatus','err','Loi: '+e));
}
</script>
</body>
</html>
)rawliteral";

// GET / — serve HTML page
static esp_err_t handler_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
}

// GET /api/status — return current status as JSON
static esp_err_t handler_status(httpd_req_t *req)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"wifi\":%s,\"key\":%s,\"lang\":\"%s\"}",
        g_wifi_connected ? "true" : "false",
        strlen(g_api_key) > 0 ? "true" : "false",
        g_speech_lang);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, strlen(buf));
}

// GET /api/wifi?ssid=...&pass=...
static esp_err_t handler_wifi(httpd_req_t *req)
{
    char ssid[64] = {0}, pass[64] = {0};
    httpd_req_get_url_query_str(req, ssid, sizeof(ssid));
    // Parse manually
    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[64];
        if (httpd_query_key_value(query, "ssid", val, sizeof(val)) == ESP_OK)
            strncpy(ssid, val, sizeof(ssid) - 1);
        if (httpd_query_key_value(query, "pass", val, sizeof(val)) == ESP_OK)
            strncpy(pass, val, sizeof(pass) - 1);
    }

    if (strlen(ssid) == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":false,\"msg\":\"Thieu SSID\"}", -1);
    }

    ESP_LOGI(TAG, "Web: wifi ssid=%s", ssid);
    oled_show_status("WIFI WEB", ssid);

    // Save credentials to NVS so we can auto-connect next boot
    wifi_creds_save(ssid, pass);

    esp_err_t err = wifi_init_sta(ssid, pass);
    const char *resp = err == ESP_OK
        ? "{\"ok\":true,\"msg\":\"WiFi da ket noi!\"}"
        : "{\"ok\":false,\"msg\":\"WiFi that bai!\"}";

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, strlen(resp));
}

// GET /api/key?key=...
static esp_err_t handler_key(httpd_req_t *req)
{
    char query[128] = {0};
    char key[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "key", key, sizeof(key));
    }

    if (strlen(key) == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":false,\"msg\":\"Thieu API Key\"}", -1);
    }

    strncpy(g_api_key, key, sizeof(g_api_key) - 1);
    ESP_LOGI(TAG, "Web: API key set (%d chars)", (int)strlen(g_api_key));

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true,\"msg\":\"API Key da luu!\"}", -1);
}

// GET /api/lang?lang=...
static esp_err_t handler_lang(httpd_req_t *req)
{
    char query[64] = {0};
    char lang[16] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "lang", lang, sizeof(lang));
    }

    if (strlen(lang) == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":false,\"msg\":\"Thieu ngon ngu\"}", -1);
    }

    strncpy(g_speech_lang, lang, sizeof(g_speech_lang) - 1);
    ESP_LOGI(TAG, "Web: language set to %s", g_speech_lang);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"msg\":\"Ngon ngu: %s\"}", g_speech_lang);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, strlen(resp));
}

// GET /api/speech — record and recognize
static esp_err_t handler_speech(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Web: speech request");

    if (!g_wifi_connected) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":false,\"msg\":\"Chua ket noi WiFi!\"}", -1);
    }
    if (strlen(g_api_key) == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":false,\"msg\":\"Chua dat API Key!\"}", -1);
    }

    // Run speech recognition (blocking, ~5-10s)
    esp_err_t err = speech_recognize_and_display();

    // The recognized text is on OLED; return it as JSON
    // We need to capture the text - let's re-run the logic to get the text
    // For simplicity, return status
    if (err == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":true,\"text\":\"Da nhan dien! Xem OLED\"}", -1);
    } else {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":false,\"msg\":\"Nhan dien that bai!\"}", -1);
    }
}

static void start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.stack_size = 8192;

    if (httpd_start(&s_web_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        return;
    }

    httpd_uri_t uri_index = { .uri = "/",           .method = HTTP_GET, .handler = handler_index };
    httpd_uri_t uri_status = { .uri = "/api/status", .method = HTTP_GET, .handler = handler_status };
    httpd_uri_t uri_wifi = { .uri = "/api/wifi",    .method = HTTP_GET, .handler = handler_wifi };
    httpd_uri_t uri_key = { .uri = "/api/key",      .method = HTTP_GET, .handler = handler_key };
    httpd_uri_t uri_lang = { .uri = "/api/lang",    .method = HTTP_GET, .handler = handler_lang };
    httpd_uri_t uri_speech = { .uri = "/api/speech", .method = HTTP_GET, .handler = handler_speech };

    httpd_register_uri_handler(s_web_server, &uri_index);
    httpd_register_uri_handler(s_web_server, &uri_status);
    httpd_register_uri_handler(s_web_server, &uri_wifi);
    httpd_register_uri_handler(s_web_server, &uri_key);
    httpd_register_uri_handler(s_web_server, &uri_lang);
    httpd_register_uri_handler(s_web_server, &uri_speech);

    ESP_LOGI(TAG, "Web server started on port 80");
}

// ===========================================================================
// APP MAIN
// ===========================================================================
static void get_sta_ip_into(char *out, size_t len)
{
    if (!out || len == 0) return;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_is_netif_up(netif)) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
            snprintf(out, len, IPSTR, IP2STR(&info.ip));
            return;
        }
    }
    snprintf(out, len, "0.0.0.0");
}

extern "C" void app_main(void)
{
    esp_task_wdt_deinit();

    ESP_LOGI(TAG, "=== MIC + OLED + Speech-to-Text ===");
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned)esp_get_free_heap_size());

    // Init NVS first (needed by both Wi-Fi STA/AP and our WiFi creds storage)
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Init OLED
    ESP_ERROR_CHECK(ssd1306_init());
    ESP_LOGI(TAG, "OLED initialized");

    // Splash screen
    fb_clear();
    fb_text(10, 8, "MIC + OLED");
    fb_text(10, 24, "SPEECH");
    fb_text(10, 40, "TO TEXT");
    fb_text(10, 56, "ESP32-S3");
    ssd1306_flush();
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Init mic
    ESP_ERROR_CHECK(mic_init());
    ESP_LOGI(TAG, "Mic initialized");

    // Try auto-connect to home Wi-Fi from saved credentials
    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};
    bool have_creds = (wifi_creds_load(saved_ssid, sizeof(saved_ssid),
                                       saved_pass, sizeof(saved_pass)) == ESP_OK
                       && saved_ssid[0] != '\0');

    bool sta_ok = false;
    if (have_creds) {
        ESP_LOGI(TAG, "Found saved Wi-Fi: '%s' — trying auto-connect...", saved_ssid);
        oled_show_status("WIFI AUTO", saved_ssid);
        esp_err_t err = wifi_init_sta(saved_ssid, saved_pass);
        if (err == ESP_OK) {
            sta_ok = true;
            get_sta_ip_into(g_sta_ip, sizeof(g_sta_ip));
            ESP_LOGI(TAG, "STA connected, IP=%s", g_sta_ip);
        } else {
            ESP_LOGW(TAG, "Auto-connect failed, fallback to AP mode");
        }
    } else {
        ESP_LOGI(TAG, "No saved Wi-Fi credentials");
    }

    if (!sta_ok) {
        ESP_LOGI(TAG, "Starting Wi-Fi AP mode (configuration)...");
        oled_show_status("WIFI AP", "ESP32-MIC-OLED");
        wifi_init_ap();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Start web server (works in both STA and AP mode)
    start_web_server();
    ESP_LOGI(TAG, "Web server ready!");

    // Show connection info on OLED
    fb_clear();
    fb_text(10, 0, "WEB SERVER");
    fb_line(0, 10, DISPLAY_WIDTH - 1, 10, true);
    if (sta_ok) {
        // STA mode: show SSID + IP
        fb_text(0, 16, "WiFi: STA");
        char line[32];
        snprintf(line, sizeof(line), "SSID:%s", saved_ssid);
        fb_text(0, 26, line);
        snprintf(line, sizeof(line), "IP:%s", g_sta_ip);
        fb_text(0, 40, line);
        fb_text(0, 54, "WEB READY");
    } else {
        // AP mode: show hotspot info
        fb_text(0, 16, "WiFi: ESP32-MIC-OLED");
        fb_text(0, 26, "Pass: 12345678");
        fb_line(0, 36, DISPLAY_WIDTH - 1, 36, true);
        fb_text(0, 42, "Open:");
        fb_text(0, 52, "192.168.4.1");
    }
    ssd1306_flush();
}
