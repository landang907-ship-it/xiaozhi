// ===========================================================================
// test_mic.cc — INMP441 microphone test on ESP32-S3-WROOM-1
// ---------------------------------------------------------------------------
// Pinout (see PINOUT.md):
//   WS (LRCLK) = GPIO4, SCK (BCLK) = GPIO5, SD (DIN) = GPIO6  (I2S_NUM_1)
//
// Standalone project, independent from test_oled and test_speaker.
// Provides a REPL console (UART0) with commands to initialize I2S RX,
// read raw samples, compute peak/RMS, dump hex, simple FFT (zero‑crossing),
// threshold detection and stress test.
// ===========================================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_console.h"
#include "esp_system.h"
#include "linenoise/linenoise.h"

static const char *TAG = "MicTest";

// ---------------------------------------------------------------------------
// Pin & audio config
// ---------------------------------------------------------------------------
#define MIC_BCLK   GPIO_NUM_5
#define MIC_WS     GPIO_NUM_4
#define MIC_DIN    GPIO_NUM_6

// Default sample rate – can be changed at runtime via REPL command.
#define SAMPLE_RATE   24000
// 24‑bit slot, we read as 32‑bit for simplicity.
#define SLOT_BITS     I2S_DATA_BIT_WIDTH_24BIT
#define SLOT_NUM      I2S_SLOT_MODE_MONO   // INMP441 L/R pin tied to GND → left only

static i2s_chan_handle_t s_rx = nullptr;
static uint32_t g_sample_rate = SAMPLE_RATE;
static uint32_t g_threshold = 2000; // peak threshold for "detect"

// ---------------------------------------------------------------------------
// Helper: init I2S RX (standard mode)
// ---------------------------------------------------------------------------
static esp_err_t mic_init(void) {
    if (s_rx) {
        // already init – deinit first
        i2s_channel_disable(s_rx);
        i2s_del_channel(s_rx);
        s_rx = nullptr;
    }
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(g_sample_rate);
    // For 24-bit slot width, mclk_multiple MUST be a multiple of 3.
    // Default is 256, override to 384 (256 is not divisible by 3 -> init fails).
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(SLOT_BITS, SLOT_NUM);
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = MIC_BCLK;
    std_cfg.gpio_cfg.ws   = MIC_WS;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din  = MIC_DIN;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;
    err = i2s_channel_init_std_mode(s_rx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "I2S RX ready (BCLK=%d WS=%d DIN=%d @ %" PRIu32 " Hz)",
             MIC_BCLK, MIC_WS, MIC_DIN, g_sample_rate);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Helper: read N frames (stereo not used, we get 24‑bit left channel only)
// ---------------------------------------------------------------------------
static void read_samples(uint32_t frames) {
    const size_t buf_words = frames * 2; // 2 words per frame (left+right) but we only use left
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) {
        ESP_LOGE(TAG, "malloc failed");
        return;
    }
    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(2000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s read error: %s", esp_err_to_name(err));
        free(buf);
        return;
    }
    size_t samples = read / sizeof(int32_t);
    // Convert 24‑bit signed to 32‑bit signed (sign‑extend)
    int32_t peak = 0;
    int64_t sum_sq = 0;
    for (size_t i = 0; i < samples; i += 2) { // step 2: left channel only
        int32_t raw = buf[i] >> 8; // keep top 24 bits
        // sign‑extend 24‑bit
        if (raw & 0x800000) raw |= ~0xFFFFFF;
        int32_t absv = std::abs(raw);
        if (absv > peak) peak = absv;
        sum_sq += (int64_t)raw * raw;
    }
    double rms = sqrt((double)sum_sq / (samples/2));
    ESP_LOGI(TAG, "Read %" PRIu32 " frames, peak=%" PRId32 ", RMS=%.1f", frames, peak, rms);
    free(buf);
}

// ---------------------------------------------------------------------------
// Helper: dump hex of N samples (left channel only)
// ---------------------------------------------------------------------------
static void dump_hex(uint32_t frames) {
    const size_t buf_words = frames * 2;
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) { ESP_LOGE(TAG, "malloc failed"); return; }
    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(2000));
    if (err != ESP_OK) { ESP_LOGE(TAG, "i2s read error: %s", esp_err_to_name(err)); free(buf); return; }
    size_t samples = read / sizeof(int32_t);
    for (size_t i = 0; i < samples; i += 2) {
        int32_t raw = buf[i] >> 8;
        if (raw & 0x800000) raw |= ~0xFFFFFF;
         printf("%08" PRIx32 " ", (uint32_t)raw);
        if ((i/2) % 8 == 7) printf("\n");
    }
    printf("\n");
    free(buf);
}

// ---------------------------------------------------------------------------
// Simple zero‑crossing based frequency estimate (very rough)
// ---------------------------------------------------------------------------
static void estimate_freq(uint32_t frames) {
    const size_t buf_words = frames * 2;
    int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
    if (!buf) { ESP_LOGE(TAG, "malloc failed"); return; }
    size_t read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(2000));
    if (err != ESP_OK) { ESP_LOGE(TAG, "i2s read error: %s", esp_err_to_name(err)); free(buf); return; }
    size_t samples = read / sizeof(int32_t);
    uint32_t zero_cross = 0;
    int32_t prev = 0;
    for (size_t i = 0; i < samples; i += 2) {
        int32_t raw = buf[i] >> 8;
        if (raw & 0x800000) raw |= ~0xFFFFFF;
        if ((prev < 0 && raw >= 0) || (prev > 0 && raw <= 0)) zero_cross++;
        prev = raw;
    }
     double freq = (double)zero_cross * g_sample_rate / (2.0 * (samples/2));
     ESP_LOGI(TAG, "Zero‑cross estimate: %.1f Hz (frames=%" PRIu32 ")", freq, frames);
    free(buf);
}

// ---------------------------------------------------------------------------
// REPL command handlers (similar to test_speaker)
// ---------------------------------------------------------------------------
static int cmd_init(int, char**) {
    return mic_init();
}

static int cmd_rate(int argc, char** argv) {
    uint32_t hz = (argc > 1) ? (uint32_t)atoi(argv[1]) : SAMPLE_RATE;
    g_sample_rate = hz;
    ESP_LOGI(TAG, "Set sample rate %" PRIu32 " Hz, re-init", hz);
    return mic_init();
}

static int cmd_read(int argc, char** argv) {
    uint32_t frames = (argc > 1) ? (uint32_t)atoi(argv[1]) : 1024;
    read_samples(frames);
    return 0;
}

static int cmd_hex(int argc, char** argv) {
    uint32_t frames = (argc > 1) ? (uint32_t)atoi(argv[1]) : 256;
    dump_hex(frames);
    return 0;
}

static int cmd_fft(int argc, char** argv) {
    uint32_t frames = (argc > 1) ? (uint32_t)atoi(argv[1]) : 1024;
    estimate_freq(frames);
    return 0;
}

static int cmd_thresh(int argc, char** argv) {
    g_threshold = (argc > 1) ? (uint32_t)atoi(argv[1]) : 2000;
    ESP_LOGI(TAG, "Threshold set to %" PRIu32 " (peak)", g_threshold);
    return 0;
}

static int cmd_watch(int argc, char** argv) {
    uint32_t sec = (argc > 1) ? (uint32_t)atoi(argv[1]) : 10;
    int64_t end = esp_timer_get_time() + (int64_t)sec * 1000000;
    ESP_LOGI(TAG, "Watch for %" PRIu32 " sec, threshold %" PRIu32, sec, g_threshold);
    while (esp_timer_get_time() < end) {
        const size_t buf_words = 256 * 2;
        int32_t *buf = (int32_t *)malloc(buf_words * sizeof(int32_t));
        if (!buf) break;
        size_t read = 0;
        i2s_channel_read(s_rx, buf, buf_words * sizeof(int32_t), &read, pdMS_TO_TICKS(500));
        size_t samples = read / sizeof(int32_t);
        int32_t peak = 0;
        for (size_t i = 0; i < samples; i += 2) {
            int32_t raw = buf[i] >> 8;
            if (raw & 0x800000) raw |= ~0xFFFFFF;
            int32_t absv = std::abs(raw);
            if (absv > peak) peak = absv;
        }
        if (peak > (int32_t)g_threshold) {
            ESP_LOGW(TAG, "Threshold exceeded! peak=%" PRId32, peak);
        }
        free(buf);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return 0;
}

static int cmd_all(int, char**) {
    ESP_LOGI(TAG, "Running full suite");
    mic_init();
    read_samples(1024);
    dump_hex(256);
    estimate_freq(1024);
    cmd_watch(5, nullptr);
    return 0;
}

static void register_commands(void) {
    struct CmdDef { const char* cmd; const char* help; esp_console_cmd_func_t func; };
    const CmdDef defs[] = {
        {"init",   "Initialize I2S RX",               &cmd_init},
        {"rate",   "rate <hz> – set sample rate",      &cmd_rate},
        {"read",   "read [frames] – read & stats",     &cmd_read},
        {"hex",    "hex [frames] – dump raw hex",      &cmd_hex},
        {"fft",    "fft [frames] – rough freq estimate", &cmd_fft},
        {"thresh", "thresh <val> – set peak threshold", &cmd_thresh},
        {"watch",  "watch [sec] – log when threshold exceeded", &cmd_watch},
        {"all",    "Run all tests sequentially",       &cmd_all},
    };
    for (auto &d : defs) {
        esp_console_cmd_t c = {};
        c.command = d.cmd;
        c.help = d.help;
        c.func = d.func;
        ESP_ERROR_CHECK(esp_console_cmd_register(&c));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== INMP441 Microphone Test ===");
    // Start REPL (new_repl_uart internally calls esp_console_init)
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "mic> ";
    repl_cfg.max_cmdline_length = 128;
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl));
    esp_console_register_help_command();
    linenoiseSetDumbMode(1);
    // Register commands AFTER repl is created
    register_commands();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
