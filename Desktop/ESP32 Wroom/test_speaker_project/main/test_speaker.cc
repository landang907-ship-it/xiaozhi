// ============================================================================
// test_speaker.cc — Bài test loa MAX98357A (I2S amplifier) trên ESP32-S3-WROOM-1
// ----------------------------------------------------------------------------
// Pinout (theo PINOUT.md):
//   BCLK = GPIO15, WS(LRC) = GPIO16, DOUT(DIN) = GPIO7   (I2S_NUM_0)
//
// Project HOÀN TOÀN ĐỘC LẬP với test_oled_project.
//
// Console menu (gõ qua Serial Monitor, 115200, UART0):
//   tone <freq> <ms>      - phát sine tại freq Hz trong ms mili-giây
//   sweep <f0> <f1> <ms>  - quét tần số từ f0 -> f1
//   wave <type> <ms>      - sine | square | triangle | saw
//   chan <l|r|both> <ms>  - test kênh trái / phải / cả hai
//   vol <ms>              - quét âm lượng 0 -> max
//   beep <n>              - n tiếng bíp ngắn
//   noise <ms>           - white noise
//   stress <sec>         - phát liên tục để test ổn định
//   all                  - chạy tuần tự toàn bộ test
//   help                 - in menu
// ============================================================================

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_console.h"
#include "esp_system.h"

static const char* TAG = "SpkTest";

// ---- Pin & audio config -----------------------------------------------------
#define SPK_BCLK   GPIO_NUM_15
#define SPK_WS     GPIO_NUM_16
#define SPK_DOUT   GPIO_NUM_7

#define SAMPLE_RATE   44100
#define AMP_MAX       28000   // biên độ tối đa (tránh clip ở 32767)

static i2s_chan_handle_t s_tx = nullptr;

// ---- Khởi tạo I2S TX --------------------------------------------------------
static esp_err_t speaker_init() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;   // tự động phát 0 khi hết data -> tránh tiếng rè
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = SPK_BCLK,
            .ws   = SPK_WS,
            .dout = SPK_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "I2S TX ready (BCLK=%d WS=%d DOUT=%d @ %d Hz)",
             SPK_BCLK, SPK_WS, SPK_DOUT, SAMPLE_RATE);
    return ESP_OK;
}

// ---- Helper: ghi 1 block stereo (L,R) interleaved ---------------------------
static void write_block(const int16_t* stereo, size_t frames) {
    size_t written = 0;
    i2s_channel_write(s_tx, stereo, frames * 2 * sizeof(int16_t),
                      &written, portMAX_DELAY);
}

// ---- Các dạng sóng ----------------------------------------------------------
enum WaveType { WAVE_SINE, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW };

static int16_t wave_sample(WaveType t, float phase /*0..1*/, float amp) {
    float v;
    switch (t) {
        case WAVE_SQUARE:   v = (phase < 0.5f) ? 1.0f : -1.0f; break;
        case WAVE_TRIANGLE: v = (phase < 0.5f) ? (4.0f * phase - 1.0f)
                                               : (3.0f - 4.0f * phase); break;
        case WAVE_SAW:      v = 2.0f * phase - 1.0f; break;
        case WAVE_SINE:
        default:            v = sinf(2.0f * (float)M_PI * phase); break;
    }
    return (int16_t)(v * amp);
}

// ---- Phát 1 tần số / dạng sóng trong ms, chọn kênh -------------------------
// chan_mask: bit0 = Left, bit1 = Right
static void play_wave(WaveType type, float freq, int ms, float amp, uint8_t chan_mask) {
    const size_t kFrames = 512;
    static int16_t buf[kFrames * 2];
    double phase = 0.0;
    double phase_inc = (double)freq / SAMPLE_RATE;
    int total_frames = (int)((int64_t)SAMPLE_RATE * ms / 1000);
    int done = 0;
    while (done < total_frames) {
        size_t n = kFrames;
        if ((int)n > total_frames - done) n = total_frames - done;
        for (size_t i = 0; i < n; ++i) {
            int16_t s = wave_sample(type, (float)phase, amp);
            buf[2 * i + 0] = (chan_mask & 0x1) ? s : 0;  // Left
            buf[2 * i + 1] = (chan_mask & 0x2) ? s : 0;  // Right
            phase += phase_inc;
            if (phase >= 1.0) phase -= 1.0;
        }
        write_block(buf, n);
        done += n;
    }
}

// ---- Các bài test -----------------------------------------------------------
static void test_tone(float freq, int ms) {
    ESP_LOGI(TAG, "TONE %.0f Hz, %d ms", freq, ms);
    play_wave(WAVE_SINE, freq, ms, AMP_MAX, 0x3);
}

static void test_sweep(float f0, float f1, int ms) {
    ESP_LOGI(TAG, "SWEEP %.0f -> %.0f Hz, %d ms", f0, f1, ms);
    const size_t kFrames = 256;
    static int16_t buf[kFrames * 2];
    int total = (int)((int64_t)SAMPLE_RATE * ms / 1000);
    double phase = 0.0;
    int done = 0;
    while (done < total) {
        size_t n = kFrames;
        if ((int)n > total - done) n = total - done;
        for (size_t i = 0; i < n; ++i) {
            float t = (float)(done + i) / total;
            float freq = f0 + (f1 - f0) * t;
            int16_t s = wave_sample(WAVE_SINE, (float)phase, AMP_MAX);
            buf[2 * i] = buf[2 * i + 1] = s;
            phase += (double)freq / SAMPLE_RATE;
            if (phase >= 1.0) phase -= 1.0;
        }
        write_block(buf, n);
        done += n;
    }
}

static void test_volume(int ms) {
    ESP_LOGI(TAG, "VOLUME ramp 0 -> max, %d ms", ms);
    const size_t kFrames = 256;
    static int16_t buf[kFrames * 2];
    int total = (int)((int64_t)SAMPLE_RATE * ms / 1000);
    double phase = 0.0;
    int done = 0;
    while (done < total) {
        size_t n = kFrames;
        if ((int)n > total - done) n = total - done;
        for (size_t i = 0; i < n; ++i) {
            float amp = AMP_MAX * (float)(done + i) / total;
            int16_t s = wave_sample(WAVE_SINE, (float)phase, amp);
            buf[2 * i] = buf[2 * i + 1] = s;
            phase += 440.0 / SAMPLE_RATE;
            if (phase >= 1.0) phase -= 1.0;
        }
        write_block(buf, n);
        done += n;
    }
}

static void test_beep(int count) {
    ESP_LOGI(TAG, "BEEP x%d", count);
    for (int i = 0; i < count; ++i) {
        play_wave(WAVE_SINE, 1000, 120, AMP_MAX, 0x3);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void test_noise(int ms) {
    ESP_LOGI(TAG, "NOISE %d ms", ms);
    const size_t kFrames = 256;
    static int16_t buf[kFrames * 2];
    int total = (int)((int64_t)SAMPLE_RATE * ms / 1000);
    int done = 0;
    while (done < total) {
        size_t n = kFrames;
        if ((int)n > total - done) n = total - done;
        for (size_t i = 0; i < n; ++i) {
            int16_t s = (int16_t)((rand() % (2 * AMP_MAX)) - AMP_MAX);
            buf[2 * i] = buf[2 * i + 1] = s;
        }
        write_block(buf, n);
        done += n;
    }
}

static void test_channels() {
    ESP_LOGI(TAG, "CHANNEL test: Left only");
    play_wave(WAVE_SINE, 440, 800, AMP_MAX, 0x1);
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_LOGI(TAG, "CHANNEL test: Right only");
    play_wave(WAVE_SINE, 440, 800, AMP_MAX, 0x2);
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_LOGI(TAG, "CHANNEL test: Both");
    play_wave(WAVE_SINE, 440, 800, AMP_MAX, 0x3);
}

static void test_waveforms() {
    const char* names[] = {"sine", "square", "triangle", "saw"};
    WaveType types[] = {WAVE_SINE, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW};
    for (int i = 0; i < 4; ++i) {
        ESP_LOGI(TAG, "WAVE %s", names[i]);
        play_wave(types[i], 440, 700, AMP_MAX, 0x3);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void test_stress(int sec) {
    ESP_LOGI(TAG, "STRESS %d s liên tục", sec);
    int64_t t0 = esp_timer_get_time();
    float freq = 220;
    while ((esp_timer_get_time() - t0) < (int64_t)sec * 1000000) {
        play_wave(WAVE_SINE, freq, 500, AMP_MAX, 0x3);
        freq *= 1.5f;
        if (freq > 4000) freq = 220;
    }
    ESP_LOGI(TAG, "STRESS done");
}

static void test_all() {
    test_tone(440, 800);  vTaskDelay(pdMS_TO_TICKS(300));
    test_tone(1000, 800); vTaskDelay(pdMS_TO_TICKS(300));
    test_sweep(200, 4000, 2000); vTaskDelay(pdMS_TO_TICKS(300));
    test_waveforms();
    test_channels();
    test_volume(2000);
    test_beep(3);
    test_noise(800);
    ESP_LOGI(TAG, "=== ALL TESTS DONE ===");
}

// ---- Console command handlers ----------------------------------------------
static int cmd_tone(int argc, char** argv) {
    float f = (argc > 1) ? atof(argv[1]) : 440;
    int ms  = (argc > 2) ? atoi(argv[2]) : 1000;
    test_tone(f, ms);
    return 0;
}
static int cmd_sweep(int argc, char** argv) {
    float f0 = (argc > 1) ? atof(argv[1]) : 200;
    float f1 = (argc > 2) ? atof(argv[2]) : 4000;
    int ms   = (argc > 3) ? atoi(argv[3]) : 2000;
    test_sweep(f0, f1, ms);
    return 0;
}
static int cmd_wave(int argc, char** argv) {
    WaveType t = WAVE_SINE;
    if (argc > 1) {
        if      (!strcmp(argv[1], "square"))   t = WAVE_SQUARE;
        else if (!strcmp(argv[1], "triangle")) t = WAVE_TRIANGLE;
        else if (!strcmp(argv[1], "saw"))      t = WAVE_SAW;
    }
    int ms = (argc > 2) ? atoi(argv[2]) : 1000;
    play_wave(t, 440, ms, AMP_MAX, 0x3);
    return 0;
}
static int cmd_chan(int argc, char** argv) {
    uint8_t mask = 0x3;
    if (argc > 1) {
        if      (!strcmp(argv[1], "l")) mask = 0x1;
        else if (!strcmp(argv[1], "r")) mask = 0x2;
    }
    int ms = (argc > 2) ? atoi(argv[2]) : 1000;
    play_wave(WAVE_SINE, 440, ms, AMP_MAX, mask);
    return 0;
}
static int cmd_vol(int argc, char** argv) {
    int ms = (argc > 1) ? atoi(argv[1]) : 2000;
    test_volume(ms);
    return 0;
}
static int cmd_beep(int argc, char** argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 3;
    test_beep(n);
    return 0;
}
static int cmd_noise(int argc, char** argv) {
    int ms = (argc > 1) ? atoi(argv[1]) : 1000;
    test_noise(ms);
    return 0;
}
static int cmd_stress(int argc, char** argv) {
    int sec = (argc > 1) ? atoi(argv[1]) : 10;
    test_stress(sec);
    return 0;
}
static int cmd_all(int, char**) { test_all(); return 0; }

static void register_commands() {
    struct CmdDef { const char* cmd; const char* help; esp_console_cmd_func_t func; };
    const CmdDef defs[] = {
        {"tone",   "tone <freq> <ms>",                     &cmd_tone},
        {"sweep",  "sweep <f0> <f1> <ms>",                 &cmd_sweep},
        {"wave",   "wave <sine|square|triangle|saw> <ms>", &cmd_wave},
        {"chan",   "chan <l|r|both> <ms>",                 &cmd_chan},
        {"vol",    "vol <ms>",                             &cmd_vol},
        {"beep",   "beep <n>",                             &cmd_beep},
        {"noise",  "noise <ms>",                           &cmd_noise},
        {"stress", "stress <sec>",                         &cmd_stress},
        {"all",    "run all tests",                        &cmd_all},
    };
    for (auto& d : defs) {
        esp_console_cmd_t c = {};
        c.command = d.cmd;
        c.help    = d.help;
        c.func    = d.func;
        ESP_ERROR_CHECK(esp_console_cmd_register(&c));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== MAX98357A Speaker Test ===");
    if (speaker_init() != ESP_OK) {
        ESP_LOGE(TAG, "Speaker init FAILED - dừng.");
        return;
    }

    // Bíp khởi động để xác nhận loa hoạt động
    test_beep(2);
    ESP_LOGI(TAG, "Gõ 'help' để xem danh sách lệnh.");

    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "spk>";
    repl_cfg.max_cmdline_length = 128;

    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl));
    esp_console_register_help_command();
    register_commands();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
