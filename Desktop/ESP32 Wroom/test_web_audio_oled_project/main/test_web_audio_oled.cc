// ===========================================================================
// test_web_audio_oled.cc — WiFi + WebSocket Audio Streaming → OLED & Browser
// ---------------------------------------------------------------------------
// Board:    ESP32-S3-WROOM-1 (N16R8, 4MB Flash, 2MB Octal PSRAM)
//
// OLED:     SSD1306 128x64 I2C, addr 0x3C
//           SDA = GPIO8, SCL = GPIO9 (I2C_NUM_0)
//
// MIC:      INMP441 I2S MEMS
//           WS = GPIO4, SCK = GPIO5, SD = GPIO6  (I2S_NUM_1)
//
// Feature:  - Kết nối WiFi STA (credentials lưu NVS, fallback default)
//           - HTTP server port 80 trả về HTML dashboard
//           - WebSocket /ws gửi audio frame (16-bit PCM) real-time
//           - 3 chế độ OLED: Waveform / VU meter / Combined
//           - Web dashboard hiển thị waveform, VU meter, FFT đơn giản
//
// Build:    rebuild.bat
// Flash:    build_and_flash.bat COM6
// Monitor:  idf.py -p COM6 monitor  (gõ "stream 0", "stream 1", "stream 2")
// ===========================================================================

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"
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

// Wi-Fi / HTTP / WS / NVS
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "cJSON.h"

// Embed HTML
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static const char *TAG = "WebAudio";

// ===========================================================================
// PIN / PARAM CONFIG
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

#define MIC_BCLK             GPIO_NUM_5
#define MIC_WS               GPIO_NUM_4
#define MIC_DIN              GPIO_NUM_6
#define SAMPLE_RATE          16000
#define SLOT_BITS            I2S_DATA_BIT_WIDTH_32BIT   // INMP441 trả 24-bit trong slot 32-bit
#define SLOT_NUM             I2S_SLOT_MODE_MONO

#define WIFI_MAX_RETRY       5
#define DEFAULT_WIFI_SSID    "Thanh  long-2.4G-ext"
#define DEFAULT_WIFI_PASS    "17111976"

#define AUDIO_TASK_STACK     6144
#define OLED_TASK_STACK      4096
#define WS_TASK_STACK        4096
#define WIFI_TASK_STACK      4096

#define WS_SEND_CHUNK        512    // samples per WS send (~32ms @ 16kHz)
#define WS_FRAME_RATE_HZ     30
#define WS_FRAME_PERIOD_MS   (1000 / WS_FRAME_RATE_HZ)

// ===========================================================================
// GLOBAL STATE
// ===========================================================================
static i2s_chan_handle_t          s_rx        = nullptr;
static i2c_master_bus_handle_t    g_i2c_bus   = nullptr;
static esp_lcd_panel_io_handle_t  g_panel_io  = nullptr;
static esp_lcd_panel_handle_t     g_panel     = nullptr;
static uint8_t                    g_framebuf[OLED_BUF_SIZE];

static volatile bool   g_streaming     = false;
static volatile int    g_display_mode  = 0;     // 0=waveform, 1=vu, 2=combined
static volatile float  g_last_peak     = 0.0f;  // 0..1
static volatile float  g_last_rms      = 0.0f;
static volatile float  g_last_freq     = 0.0f;

static char     g_sta_ip[16] = "0.0.0.0";
static httpd_handle_t s_httpd = nullptr;

// Audio ring buffer (between I2S task and OLED/WS tasks)
static RingbufHandle_t s_audio_ring = nullptr;
#define RING_BUFFER_SIZE  (16 * 1024)  // 16KB internal SRAM

// STT text received from browser (Web Speech API)
static char     g_last_text[128]    = {0};
static volatile bool g_text_new    = false;
static int64_t  g_text_expire_us    = 0;     // esp_timer_get_time() deadline
#define TEXT_DISPLAY_MS  6000             // hiển thị 6 giây sau khi nhận text mới
static portMUX_TYPE s_text_lock     = portMUX_INITIALIZER_UNLOCKED;

// ===========================================================================
// 5x7 FONT (subset)
// ===========================================================================
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x01,0x01},
    {0x3E,0x41,0x41,0x51,0x32},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x08,0x04,0x08,0x10,0x08},{0x00,0x00,0x00,0x00,0x00},
};

// ===========================================================================
// OLED helpers
// ===========================================================================
static void oled_draw_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    if (DISPLAY_MIRROR_X) x = DISPLAY_WIDTH - 1 - x;
    if (DISPLAY_MIRROR_Y) y = DISPLAY_HEIGHT - 1 - y;
    uint16_t idx = x + (y / 8) * DISPLAY_WIDTH;
    if (on) g_framebuf[idx] |= (1 << (y & 7));
    else    g_framebuf[idx] &= ~(1 << (y & 7));
}

static void oled_clear(void)  { memset(g_framebuf, 0, sizeof(g_framebuf)); }

static void oled_flush(void)
{
    if (g_panel) {
        esp_lcd_panel_draw_bitmap(g_panel, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, g_framebuf);
    }
}

static void oled_text(int x, int y, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c < 0x20 || c > 0x7E) c = '?';
        const uint8_t *glyph = font5x7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                oled_draw_pixel(x + col, y + row, (glyph[col] >> row) & 1);
            }
        }
        x += 6;
    }
}

// ===========================================================================
// Vietnamese diacritics → ASCII (decode UTF-8, map composite letters to base)
// ===========================================================================
static void vn_unaccent(const char *src, char *dst, size_t dst_size)
{
    if (dst_size == 0) return;
    static const struct { uint32_t cp; char to; } map[] = {
        // Uppercase
        {0x00C1,'A'}, {0x00C0,'A'}, {0x1EA2,'A'}, {0x00C3,'A'}, {0x00C2,'A'}, {0x1EA0,'A'},
        {0x0102,'A'}, {0x1EAE,'A'}, {0x1EB0,'A'}, {0x1EB2,'A'}, {0x1EB4,'A'}, {0x1EB6,'A'},
        {0x00C9,'E'}, {0x00C8,'E'}, {0x1EBA,'E'}, {0x00CA,'E'}, {0x1EB8,'E'},
        {0x00CD,'I'}, {0x00CC,'I'}, {0x1EC8,'I'},
        {0x00D3,'O'}, {0x00D2,'O'}, {0x1ECE,'O'}, {0x00D5,'O'}, {0x00D4,'O'}, {0x1ECC,'O'},
        {0x01A0,'O'}, {0x1ED0,'O'}, {0x1ED2,'O'}, {0x1ED4,'O'}, {0x1ED6,'O'}, {0x1ED8,'O'},
        {0x00DA,'U'}, {0x00D9,'U'}, {0x1EE6,'U'}, {0x0168,'U'}, {0x1EE4,'U'},
        {0x01AF,'U'}, {0x1EE8,'U'}, {0x1EEA,'U'}, {0x1EEC,'U'}, {0x1EEE,'U'}, {0x1EF0,'U'},
        {0x00DD,'Y'}, {0x1EF2,'Y'}, {0x1EF4,'Y'}, {0x1EF6,'Y'}, {0x1EF8,'Y'},
        {0x0110,'D'},
        // Lowercase
        {0x00E1,'a'}, {0x00E0,'a'}, {0x1EA3,'a'}, {0x00E3,'a'}, {0x00E2,'a'}, {0x1EA1,'a'},
        {0x0103,'a'}, {0x1EAF,'a'}, {0x1EB1,'a'}, {0x1EB3,'a'}, {0x1EB5,'a'}, {0x1EB7,'a'},
        {0x00E9,'e'}, {0x00E8,'e'}, {0x1EBB,'e'}, {0x00EA,'e'}, {0x1EB9,'e'},
        {0x00ED,'i'}, {0x00EC,'i'}, {0x1EC9,'i'},
        {0x00F3,'o'}, {0x00F2,'o'}, {0x1ECF,'o'}, {0x00F5,'o'}, {0x00F4,'o'}, {0x1ECD,'o'},
        {0x01A1,'o'}, {0x1ED1,'o'}, {0x1ED3,'o'}, {0x1ED5,'o'}, {0x1ED7,'o'}, {0x1ED9,'o'},
        {0x00FA,'u'}, {0x00F9,'u'}, {0x1EE7,'u'}, {0x0169,'u'}, {0x1EE5,'u'},
        {0x01B0,'u'}, {0x1EE9,'u'}, {0x1EEB,'u'}, {0x1EED,'u'}, {0x1EEF,'u'}, {0x1EF1,'u'},
        {0x00FD,'y'}, {0x1EF3,'y'}, {0x1EF5,'y'}, {0x1EF7,'y'}, {0x1EF9,'y'},
        {0x0111,'d'},
    };
    size_t di = 0;
    const uint8_t *p = (const uint8_t *)src;
    while (*p && di + 1 < dst_size) {
        uint32_t cp = 0;
        int extra = 0;
        if (*p < 0x80)              { cp = *p; extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = (*p & 0x1F) << 6;  extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = (*p & 0x0F) << 12; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = (*p & 0x07) << 18; extra = 3; }
        else { p++; continue; }
        // validate continuation bytes
        bool bad = false;
        for (int i = 1; i <= extra; i++) {
            if ((p[i] & 0xC0) != 0x80) { bad = true; break; }
        }
        if (bad) { p++; continue; }
        for (int i = 1; i <= extra; i++) cp |= ((uint32_t)(p[i] & 0x3F) << (6 * (extra - i)));
        p += 1 + extra;

        char out = 0;
        for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
            if (map[i].cp == cp) { out = map[i].to; break; }
        }
        if (out == 0 && cp >= 0x20 && cp <= 0x7E) out = (char)cp;
        if (out) dst[di++] = out;
    }
    dst[di] = 0;
}

// ===========================================================================
// Display wrapped multi-line text (ASCII only). Title on row 0 (optional).
// ===========================================================================
static void oled_show_text_wrapped(const char *title, const char *text)
{
    oled_clear();
    const int char_w = 6;        // 5 px glyph + 1 px gap
    const int char_h = 8;        // 7 px glyph + 1 px gap
    const int max_cols = DISPLAY_WIDTH / char_w;   // 21
    const int max_rows = DISPLAY_HEIGHT / char_h;  // 8
    int row = 0, col = 0;
    if (title) {
        oled_text(0, 0, title);
        row = 1;
    }
    char line[32];
    const char *p = text;
    while (*p && row < max_rows) {
        // copy one word (until space or newline)
        int n = 0;
        while (*p && *p != ' ' && *p != '\n' && n < (int)sizeof(line) - 1) {
            line[n++] = *p++;
        }
        line[n] = 0;
        if (n == 0) {
            // only whitespace
            if (*p == '\n') { row++; col = 0; }
            else if (col < max_cols) { col++; }
            p++;
            continue;
        }
        // wrap if needed
        if (col + n > max_cols) {
            row++;
            col = 0;
            if (row >= max_rows) break;
        }
        // chunk-print word if too long for one line
        int printed = 0;
        while (printed < n && row < max_rows) {
            int space = max_cols - col;
            int chunk = n - printed;
            if (chunk > space) chunk = space;
            char tmp[24];
            memcpy(tmp, line + printed, chunk);
            tmp[chunk] = 0;
            oled_text(col * char_w, row * char_h, tmp);
            col += chunk;
            printed += chunk;
            if (col >= max_cols && printed < n) {
                row++;
                col = 0;
            }
        }
        if (*p == ' ' && col < max_cols) col++;
        else if (*p == '\n') { row++; col = 0; }
        if (*p) p++;
    }
    oled_flush();
}

// ===========================================================================
// OLED INIT
// ===========================================================================
static esp_err_t oled_init(void)
{
    ESP_LOGI(TAG, "Init OLED SSD1306 (SDA=%d SCL=%d)", DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = DISPLAY_I2C_NUM;
    bus_cfg.sda_io_num = DISPLAY_SDA_PIN;
    bus_cfg.scl_io_num = DISPLAY_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &g_i2c_bus));

    esp_lcd_panel_io_handle_t io = nullptr;
    esp_lcd_panel_io_i2c_config_t io_cfg = {};
    io_cfg.dev_addr = DISPLAY_I2C_ADDR;
    io_cfg.scl_speed_hz = 400 * 1000;
    io_cfg.control_phase_bytes = 1;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.dc_bit_offset = 6;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(g_i2c_bus, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.bits_per_pixel = 1;
    panel_cfg.reset_gpio_num = GPIO_NUM_NC;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io, &panel_cfg, &g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));

    g_panel_io = io;
    oled_clear();
    oled_flush();
    ESP_LOGI(TAG, "OLED ready");
    return ESP_OK;
}

// ===========================================================================
// I2S MIC INIT
// ===========================================================================
static esp_err_t mic_init(void)
{
    ESP_LOGI(TAG, "Init I2S mic (WS=%d SCK=%d SD=%d)", MIC_WS, MIC_BCLK, MIC_DIN);
    i2s_chan_config_t chan_cfg = {};
    chan_cfg.id = I2S_NUM_1;
    chan_cfg.role = I2S_ROLE_MASTER;
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 256;
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &s_rx));

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg.data_bit_width = SLOT_BITS;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = SLOT_NUM;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    std_cfg.slot_cfg.ws_width = 32;
    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = true;
    std_cfg.gpio_cfg.bclk = MIC_BCLK;
    std_cfg.gpio_cfg.ws   = MIC_WS;
    std_cfg.gpio_cfg.din  = MIC_DIN;
    std_cfg.gpio_cfg.dout = GPIO_NUM_NC;
    std_cfg.gpio_cfg.mclk = GPIO_NUM_NC;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
    ESP_LOGI(TAG, "I2S mic ready @ %d Hz", SAMPLE_RATE);
    return ESP_OK;
}

// ===========================================================================
// Convert 24-bit-in-32-bit I2S sample to signed 16-bit mono
// ===========================================================================
static inline int16_t sample_to_int16(int32_t raw)
{
    // INMP441: 24-bit left-justified in 32-bit slot. Shift down by 8.
    int32_t s = raw >> 8;
    // Saturate to int16
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    return (int16_t)s;
}

// ===========================================================================
// NVS helpers for WiFi credentials
// ===========================================================================
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

// ===========================================================================
// WiFi STA
// ===========================================================================
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
static int s_wifi_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGW(TAG, "WiFi retry %d/%d", s_wifi_retry_num, WIFI_MAX_RETRY);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(g_sta_ip, sizeof(g_sta_ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "WiFi connected: IP=%s", g_sta_ip);
        s_wifi_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    char ssid[32] = {0};
    char pass[64] = {0};
    if (wifi_creds_load(ssid, sizeof(ssid), pass, sizeof(pass)) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi creds from NVS: SSID='%s'", ssid);
    } else {
        strncpy(ssid, DEFAULT_WIFI_SSID, sizeof(ssid));
        strncpy(pass, DEFAULT_WIFI_PASS, sizeof(pass));
        ESP_LOGW(TAG, "WiFi using default: SSID='%s'", ssid);
    }

    wifi_config_t wifi_cfg = {};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done, waiting for connection...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE, pdMS_TO_TICKS(20000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "WiFi connect failed (timeout)");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ===========================================================================
// HTTP / WebSocket handlers
// ===========================================================================
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const uint32_t len = index_html_end - index_html_start;
    return httpd_resp_send(req, (const char *)index_html_start, len);
}

#define WS_CLIENTS_MAX 4
static int s_ws_fds[WS_CLIENTS_MAX] = { -1, -1, -1, -1 };
static portMUX_TYPE s_ws_lock = portMUX_INITIALIZER_UNLOCKED;

static void ws_add_fd(int fd)
{
    taskENTER_CRITICAL(&s_ws_lock);
    for (int i = 0; i < WS_CLIENTS_MAX; i++) {
        if (s_ws_fds[i] < 0) { s_ws_fds[i] = fd; break; }
    }
    taskEXIT_CRITICAL(&s_ws_lock);
}

static void ws_remove_fd(int fd)
{
    taskENTER_CRITICAL(&s_ws_lock);
    for (int i = 0; i < WS_CLIENTS_MAX; i++) {
        if (s_ws_fds[i] == fd) { s_ws_fds[i] = -1; break; }
    }
    taskEXIT_CRITICAL(&s_ws_lock);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WS handshake from fd=%d", httpd_req_to_sockfd(req));
        ws_add_fd(httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    httpd_ws_frame_t pkt = {};
    pkt.type = HTTPD_WS_TYPE_CLOSE;
    return httpd_ws_send_frame(req, &pkt);
}

// Send audio chunk (16-bit PCM little-endian) to all connected WebSocket clients
static void ws_broadcast_audio(const int16_t *samples, size_t count)
{
    taskENTER_CRITICAL(&s_ws_lock);
    for (int i = 0; i < WS_CLIENTS_MAX; i++) {
        int fd = s_ws_fds[i];
        if (fd < 0) continue;
        httpd_ws_frame_t pkt = {};
        pkt.payload = (uint8_t *)samples;
        pkt.len = count * sizeof(int16_t);
        pkt.type = HTTPD_WS_TYPE_BINARY;
        httpd_ws_send_frame_async(s_httpd, fd, &pkt);
    }
    taskEXIT_CRITICAL(&s_ws_lock);
}

static esp_err_t ctrl_post_handler(httpd_req_t *req)
{
    char buf[64] = {0};
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = 0;
    ESP_LOGI(TAG, "CTRL: %s", buf);
    if (strncmp(buf, "mode=", 5) == 0) {
        g_display_mode = atoi(buf + 5);
    } else if (strncmp(buf, "stream=", 7) == 0) {
        g_streaming = (atoi(buf + 7) != 0);
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// Receive STT text from browser (Web Speech API). Body is JSON: {"text":"..."}
static esp_err_t text_post_handler(httpd_req_t *req)
{
    char *buf = (char *)calloc(1, 513);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    int total = 0;
    int r;
    while (total < 512) {
        r = httpd_req_recv(req, buf + total, 512 - total);
        if (r <= 0) break;
        total += r;
    }
    buf[total] = 0;

    if (total == 0) {
        free(buf);
        httpd_resp_send(req, "EMPTY", 5);
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(buf);
    const char *txt = root ? cJSON_GetObjectItem(root, "text")->valuestring : nullptr;
    if (!txt) txt = "";

    // ascii-safe copy + vn_unaccent for OLED rendering
    char ascii_buf[sizeof(g_last_text)];
    vn_unaccent(txt, ascii_buf, sizeof(ascii_buf));

    portENTER_CRITICAL(&s_text_lock);
    strncpy(g_last_text, ascii_buf, sizeof(g_last_text) - 1);
    g_last_text[sizeof(g_last_text) - 1] = 0;
    g_text_expire_us = esp_timer_get_time() + (int64_t)TEXT_DISPLAY_MS * 1000;
    g_text_new = true;
    portEXIT_CRITICAL(&s_text_lock);

    ESP_LOGI(TAG, "STT text: '%s'", ascii_buf);
    if (root) cJSON_Delete(root);
    free(buf);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":1}", 7);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 6;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return nullptr;
    }

    httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr };
    httpd_register_uri_handler(server, &uri_root);

    httpd_uri_t uri_ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .user_ctx = nullptr };
    httpd_register_uri_handler(server, &uri_ws);

    httpd_uri_t uri_ctrl = { .uri = "/ctrl", .method = HTTP_POST, .handler = ctrl_post_handler, .user_ctx = nullptr };
    httpd_register_uri_handler(server, &uri_ctrl);

    httpd_uri_t uri_text = { .uri = "/api/text", .method = HTTP_POST, .handler = text_post_handler, .user_ctx = nullptr };
    httpd_register_uri_handler(server, &uri_text);

    ESP_LOGI(TAG, "Webserver started on port 80");
    return server;
}

// ===========================================================================
// I2S READER TASK — đọc mic, push int16 vào ring buffer
// ===========================================================================
static void i2s_reader_task(void *arg)
{
    ESP_LOGI(TAG, "i2s_reader_task started on core %d", xPortGetCoreID());
    int32_t raw[256];
    while (true) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
        if (err != ESP_OK || bytes_read == 0) continue;

        size_t n_samples = bytes_read / sizeof(int32_t);
        int16_t out[256];
        for (size_t i = 0; i < n_samples; i++) {
            out[i] = sample_to_int16(raw[i]);
        }

        if (s_audio_ring) {
            // Send via WebSocket directly (avoid double-buffering)
            ws_broadcast_audio(out, n_samples);

            // Push to ring for OLED task
            if (xRingbufferSend(s_audio_ring, out, n_samples * sizeof(int16_t), 0) != pdTRUE) {
                // Drop oldest if full
                void *item = nullptr;
                size_t sz = 0;
                item = xRingbufferReceiveUpTo(s_audio_ring, &sz, 0, n_samples * sizeof(int16_t));
                if (item) vRingbufferReturnItem(s_audio_ring, item);
                xRingbufferSend(s_audio_ring, out, n_samples * sizeof(int16_t), 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ===========================================================================
// OLED RENDER TASK — đọc ring buffer, vẽ waveform / VU
// ===========================================================================
static void compute_stats(const int16_t *s, size_t n, float &peak, float &rms, float &freq)
{
    int32_t p = 0;
    int64_t sum_sq = 0;
    int zero_cross = 0;
    int prev_sign = 0;
    for (size_t i = 0; i < n; i++) {
        int v = s[i];
        int a = (v < 0) ? -v : v;
        if (a > p) p = a;
        sum_sq += (int64_t)v * v;

        int sign = (v > 0) ? 1 : (v < 0 ? -1 : 0);
        if (i > 0 && sign != 0 && prev_sign != 0 && sign != prev_sign) zero_cross++;
        if (sign != 0) prev_sign = sign;
    }
    peak = (float)p / 32768.0f;
    rms  = sqrtf((float)sum_sq / n) / 32768.0f;
    freq = (float)zero_cross * (float)SAMPLE_RATE / (2.0f * n);
}

static void oled_render_task(void *arg)
{
    ESP_LOGI(TAG, "oled_render_task started on core %d", xPortGetCoreID());
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(33);  // 30 FPS

    while (true) {
        // ---- TEXT OVERRIDE: nếu có text mới & chưa hết hạn -> hiển thị text thay cho waveform/VU ----
        bool show_text = false;
        char text_copy[sizeof(g_last_text)];
        text_copy[0] = 0;
        portENTER_CRITICAL(&s_text_lock);
        if (g_text_new && esp_timer_get_time() < g_text_expire_us && g_last_text[0]) {
            strncpy(text_copy, g_last_text, sizeof(text_copy));
            text_copy[sizeof(text_copy) - 1] = 0;
            show_text = true;
        }
        portEXIT_CRITICAL(&s_text_lock);
        if (show_text) {
            oled_show_text_wrapped("STT:", text_copy);
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        if (!g_streaming) {
            oled_clear();
            char buf[24];
            snprintf(buf, sizeof(buf), "IP:%s", g_sta_ip);
            oled_text(0, 0, "WiFi Audio OLED");
            oled_text(0, 16, buf);
            oled_text(0, 32, "open browser at");
            oled_text(0, 48, buf);
            oled_flush();
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        // Read up to 512 samples from ring buffer (covers ~32 ms @ 16kHz)
        size_t want_bytes = 512 * sizeof(int16_t);
        size_t got = 0;
        void *item = nullptr;
        item = xRingbufferReceiveUpTo(s_audio_ring, &got, pdMS_TO_TICKS(20), want_bytes);
        if (!item || got == 0) {
            if (item) vRingbufferReturnItem(s_audio_ring, item);
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        const int16_t *samples = (const int16_t *)item;
        size_t n = got / sizeof(int16_t);

        float peak = 0, rms = 0, freq = 0;
        compute_stats(samples, n, peak, rms, freq);
        g_last_peak = peak;
        g_last_rms  = rms;
        g_last_freq = freq;

        oled_clear();

        int mode = g_display_mode;
        if (mode == 2) mode = 1;  // combined -> waveform upper + VU lower

        if (mode == 0) {
            // ===== MODE 0: full-screen WAVEFORM for voice =====
            // Auto-gain: khuếch đại để giọng nhỏ vẫn thấy rõ, không bùng nhiễu khi im
            float gain = 1.0f / (peak + 0.05f);
            if (gain > 8.0f) gain = 8.0f;
            gain *= 0.7f;

            // Baseline + grid (mỗi 16 px một vạch dọc)
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                oled_draw_pixel(x, 32, true);                              // baseline
                if ((x & 15) == 0) oled_draw_pixel(x, 31, true);            // tick baseline
                if ((x & 15) == 0) { oled_draw_pixel(x, 16, true); oled_draw_pixel(x, 48, true); }
            }

            // Vẽ sóng: lấy max-min cho mỗi cột để thấy cả đỉnh lẫn đáy
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                size_t idx0 = (size_t)((int64_t)x * n / DISPLAY_WIDTH);
                size_t idx1 = (size_t)((int64_t)(x + 1) * n / DISPLAY_WIDTH);
                if (idx1 <= idx0) idx1 = idx0 + 1;
                if (idx1 > n) idx1 = n;

                int col_min =  32767;
                int col_max = -32768;
                for (size_t k = idx0; k < idx1; k++) {
                    int v = samples[k];
                    if (v < col_min) col_min = v;
                    if (v > col_max) col_max = v;
                }
                // Áp gain rồi vẽ min..max
                int ymin = 32 - (int)((float)col_max * gain * 30.0f / 32768.0f);
                int ymax = 32 - (int)((float)col_min * gain * 30.0f / 32768.0f);
                if (ymin > ymax) { int t = ymin; ymin = ymax; ymax = t; }
                if (ymin < 0) ymin = 0;
                if (ymax > 63) ymax = 63;
                for (int y = ymin; y <= ymax; y++) oled_draw_pixel(x, y, true);
            }
        } else {
            // ===== MODE 1 (VU) / combined fallback =====
            char hdr[32];
            snprintf(hdr, sizeof(hdr), "P%.2f R%.2f F%.0f", peak, rms, freq);
            oled_text(0, 0, hdr);

            if (mode == 2) {
                int y0 = 10, y1 = 44;
                for (int x = 0; x < DISPLAY_WIDTH; x++) {
                    size_t idx = (size_t)((int64_t)x * n / DISPLAY_WIDTH);
                    int v = samples[idx];
                    int y = y0 + (y1 - y0) / 2 - (v * (y1 - y0)) / 65536;
                    if (y < y0) y = y0;
                    if (y > y1) y = y1;
                    oled_draw_pixel(x, y, true);
                }
            }

            int bar_w = (int)(peak * DISPLAY_WIDTH);
            for (int x = 0; x < bar_w; x++) {
                for (int y = 50; y < 62; y++) oled_draw_pixel(x, y, true);
            }
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                for (int y = 62; y < 63; y++) oled_draw_pixel(x, y, true);
            }
            char vubuf[24];
            snprintf(vubuf, sizeof(vubuf), "RMS %.2f", rms);
            oled_text(0, 50, vubuf);
        }

        oled_flush();
        vRingbufferReturnItem(s_audio_ring, item);
        vTaskDelayUntil(&last_wake, period);
    }
}

// ===========================================================================
// Console command processing
// ===========================================================================
static int s_cmd_idx = 0;
static char s_cmd_buf[128];

static void console_task(void *arg)
{
    ESP_LOGI(TAG, "Console ready. Commands:");
    ESP_LOGI(TAG, "  stream 0/1    - stop / start streaming");
    ESP_LOGI(TAG, "  mode 0|1|2    - waveform | vu | combined");
    ESP_LOGI(TAG, "  status        - show ip + state");
    while (true) {
        int c = getchar();
        if (c == EOF) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        if (c == '\n' || c == '\r') {
            s_cmd_buf[s_cmd_idx] = 0;
            if (s_cmd_idx > 0) {
                if (strcmp(s_cmd_buf, "stream 1") == 0) g_streaming = true;
                else if (strcmp(s_cmd_buf, "stream 0") == 0) g_streaming = false;
                else if (strncmp(s_cmd_buf, "mode ", 5) == 0) g_display_mode = atoi(s_cmd_buf + 5);
                else if (strcmp(s_cmd_buf, "mode") == 0) ESP_LOGI(TAG, "mode=%d", g_display_mode);
                else if (strcmp(s_cmd_buf, "status") == 0) {
                    ESP_LOGI(TAG, "ip=%s streaming=%d mode=%d",
                             g_sta_ip, (int)g_streaming, g_display_mode);
                }
                else if (strncmp(s_cmd_buf, "text ", 5) == 0) {
                    char ascii_buf[sizeof(g_last_text)];
                    vn_unaccent(s_cmd_buf + 5, ascii_buf, sizeof(ascii_buf));
                    portENTER_CRITICAL(&s_text_lock);
                    strncpy(g_last_text, ascii_buf, sizeof(g_last_text) - 1);
                    g_last_text[sizeof(g_last_text) - 1] = 0;
                    g_text_expire_us = esp_timer_get_time() + (int64_t)TEXT_DISPLAY_MS * 1000;
                    g_text_new = true;
                    portEXIT_CRITICAL(&s_text_lock);
                    ESP_LOGI(TAG, "text set: '%s'", ascii_buf);
                }
                else ESP_LOGW(TAG, "unknown: '%s'", s_cmd_buf);
            }
            s_cmd_idx = 0;
        } else if (s_cmd_idx < (int)sizeof(s_cmd_buf) - 1) {
            s_cmd_buf[s_cmd_idx++] = (char)c;
        }
    }
}

// ===========================================================================
// app_main
// ===========================================================================
extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "====== test_web_audio_oled starting ======");

    s_audio_ring = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!s_audio_ring) {
        ESP_LOGE(TAG, "Ringbuffer alloc failed");
        return;
    }

    ESP_ERROR_CHECK(oled_init());
    ESP_ERROR_CHECK(mic_init());

    if (wifi_init_sta() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi failed, OLED shows 'WiFi FAIL'");
        oled_clear();
        oled_text(0, 24, "WiFi FAIL");
        oled_flush();
        return;
    }

    s_httpd = start_webserver();
    if (!s_httpd) {
        ESP_LOGE(TAG, "Webserver failed to start");
    }

    // Auto-enable streaming so OLED shows mic waveform right after boot.
    g_streaming = true;

    xTaskCreatePinnedToCore(i2s_reader_task, "i2s_rd", AUDIO_TASK_STACK, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(oled_render_task, "oled",  OLED_TASK_STACK,  nullptr, 4, nullptr, 1);
    xTaskCreatePinnedToCore(console_task, "console", 4096, nullptr, 3, nullptr, 1);

    ESP_LOGI(TAG, "Open browser at http://%s/", g_sta_ip);
}
