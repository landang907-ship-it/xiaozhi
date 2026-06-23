// ssd1306.h
// Minimal SSD1306 128x64 OLED driver via I2C (no LVGL - that's P4).
// Phase 2.3: prove I2C bus + panel + framebuffer + flush all work.
//
// Protocol: I2C control byte 0x00=command, 0x40=data, followed by bytes.
// Memory: 1 byte = 8 vertical pixels. 128x64 = 1024 bytes framebuffer.

#ifndef _SSD1306_H
#define _SSD1306_H

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "driver/i2c_master.h"

class Ssd1306 {
public:
    Ssd1306(i2c_master_bus_handle_t bus, uint8_t addr, int width, int height);
    ~Ssd1306();

    // Send init sequence (display off, charge pump, addressing mode, etc).
    // Must be called once before any drawing/Display().
    void Init();

    void Clear();                                // fb = 0
    void SetPixel(int x, int y, uint8_t color);  // color: 1=on, 0=off
    void DrawChar(int x, int y, char ch, uint8_t color);   // 5x7 font, mono
    void DrawText(int x, int y, const char* str, uint8_t color);
    void DrawRect(int x, int y, int w, int h, uint8_t color);

    // Flush framebuffer to panel via I2C (full refresh).
    void Display();

    inline int width() const { return width_; }
    inline int height() const { return height_; }

private:
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;  // created in Init()
    uint8_t addr_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> fb_;   // width_ * height_ / 8 bytes

    // Send a command sequence (control=0x00 prefix).
    esp_err_t WriteCommands(const uint8_t* cmds, size_t n);

    // Send data (control=0x40 prefix), full refresh: column 0..127, page 0..7.
    esp_err_t WriteFramebuffer();
};

#endif // _SSD1306_H
