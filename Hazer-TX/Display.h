// Display.h  (Hazer-TX) - LovyanGFX setup for the ESP32-2432S028R (CYD)
#pragma once
#include <Arduino.h>
#include "Config.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_XPT2046 _touch;
public:
  LGFX() {
    {
      auto c = _bus.config();
      c.spi_host = SPI2_HOST; c.spi_mode = 0;
      c.freq_write = 40000000; c.freq_read = 16000000;
      c.spi_3wire = false; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 14; c.pin_mosi = 13; c.pin_miso = 12; c.pin_dc = 2;
      _bus.config(c); _panel.setBus(&_bus);
    }
    {
      auto c = _panel.config();
      c.pin_cs = 15; c.pin_rst = -1; c.pin_busy = -1;
      c.panel_width = 240; c.panel_height = 320;
      c.offset_x = 0; c.offset_y = 0; c.offset_rotation = 0;
      c.readable = false; c.invert = DISPLAY_INVERT; c.rgb_order = DISPLAY_RGB_ORDER;
      c.dlen_16bit = false; c.bus_shared = false;
      _panel.config(c);
    }
    {
      auto c = _light.config();
      c.pin_bl = 21; c.invert = false; c.freq = 44100; c.pwm_channel = 7;
      _light.config(c); _panel.setLight(&_light);
    }
    {
      auto c = _touch.config();
      c.x_min = 300; c.x_max = 3900; c.y_min = 200; c.y_max = 3700;
      c.pin_int = 36; c.bus_shared = false; c.offset_rotation = 6;
      c.spi_host = SPI3_HOST; c.freq = 1000000;
      c.pin_sclk = 25; c.pin_mosi = 32; c.pin_miso = 39; c.pin_cs = 33;
      _touch.config(c); _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};

extern LGFX lcd;

void displayBegin();   // init panel, rotation, backlight (+ optional touch calibration)
