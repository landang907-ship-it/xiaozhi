#pragma once
#include <cstdint>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

class Ssd1306 {
public:
    explicit Ssd1306(i2c_master_bus_handle_t bus);
    ~Ssd1306();
    void Init();
    void SetPixel(int x, int y, bool on);
    void Clear();
    void Update();
    void Fill(bool on);
    // Draw rectangle (border only)
    void DrawRect(int x, int y, int w, int h, bool on);
    // Draw text (basic 8x5 bitmap font, row y in pixels)
    void DrawText(int x, int y, const char* text, bool on);
    // Draw raw bitmap
    void DrawBitmap(int x, int y, int w, int h, const uint8_t* bitmap, bool on);
    // Send buffer to display
    void Display();

private:
    i2c_master_bus_handle_t bus_ = nullptr;
    esp_lcd_panel_io_handle_t io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    uint8_t buffer_[1024]{}; // 128x64 / 8 = 1024 bytes

    void HLine(int x, int y, int len, bool on);
    void VLine(int x, int y, int len, bool on);
};
