#pragma once
#include <cstdint>
#include <driver/i2c_master.h>

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
    // Send buffer to display
    void Display();

private:
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    uint8_t buffer_[1024]{}; // 128x64 / 8 = 1024 bytes

    void HLine(int x, int y, int len, bool on);
    void VLine(int x, int y, int len, bool on);
};
