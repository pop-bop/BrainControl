#pragma once

#include <Arduino.h>
#include "TinkerKit_Pins.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_TinkerKit : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789      _panel_instance;
    lgfx::Bus_SPI           _bus_instance;
    lgfx::Light_PWM         _light_instance;
    lgfx::Touch_FT5x06      _touch_instance;

public:
    LGFX_TinkerKit() {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_LCD_SCK;   // 39
            cfg.pin_mosi = PIN_LCD_MOSI;  // 40
            cfg.pin_miso = PIN_LCD_MISO;  // 38
            cfg.pin_dc   = PIN_LCD_DC;    // 5
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = PIN_LCD_CS;   // 4
            cfg.pin_rst          = PIN_LCD_RST;  // 6
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0; // Native orientation
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true; // Shared with SD card
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = PIN_LCD_BL; // 15
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.i2c_port   = 0;
            cfg.i2c_addr   = I2C_ADDR_TOUCH; // 0x38
            cfg.pin_sda    = PIN_I2C0_SDA;   // 42
            cfg.pin_scl    = PIN_I2C0_SCL;   // 41
            cfg.pin_int    = -1;
            cfg.freq       = I2C0_FREQ;
            cfg.x_min      = 0;
            cfg.x_max      = 239;
            cfg.y_min      = 0;
            cfg.y_max      = 319;
            cfg.bus_shared = true;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

class TinkerKit_Display {
public:
    LGFX_TinkerKit gfx;

    bool begin() {
        gfx.init();
        gfx.setRotation(1); // 320 wide x 240 high
        gfx.setBrightness(128);
        gfx.fillScreen(TFT_BLACK);
        return true;
    }

    bool getTouch(uint16_t &x, uint16_t &y) {
        return gfx.getTouch(&x, &y);
    }

    bool getTouchRaw(uint16_t &x, uint16_t &y) {
        return gfx.getTouchRaw(&x, &y);
    }

    void setBrightness(uint8_t brightness) {
        gfx.setBrightness(brightness);
    }
};
