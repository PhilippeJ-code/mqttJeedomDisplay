// io_expander.cpp
//
#include "sdkconfig.h"

#if defined(CONFIG_WS_USE_USB) || defined(CONFIG_WS_USE_LVGL)
#include "esp_io_expander.hpp"

#define I2C_MASTER_NUM 0

#define TP_RST 1
#define LCD_BL 2
#define LCD_RST 3
#define SD_CS 4
#define USB_SEL 5

#define EXAMPLE_I2C_SDA_PIN (8)
#define EXAMPLE_I2C_SCL_PIN (9)
#define EXAMPLE_I2C_ADDR    (ESP_IO_EXPANDER_I2C_CH422G_ADDRESS)

esp_expander::CH422G *expander = NULL;

static bool bBacklightState = true;

extern "C" void ioExpanderInit()
{
    expander = new esp_expander::CH422G(I2C_NUM_0, EXAMPLE_I2C_ADDR);
    expander->init();
    expander->begin();

    expander->enableOC_PushPull();
//    expander->enableOC_OpenDrain();
//    expander->enableAllIO_Input();
    expander->enableAllIO_Output();

    expander->multiDigitalWrite(63, HIGH);
    expander->digitalWrite(USB_SEL, LOW);
    expander->digitalWrite(SD_CS, LOW);
}

extern "C" void turn_backlight_off()
{
    if ( bBacklightState == false )
        return;
    expander->digitalWrite(LCD_BL, LOW);
    bBacklightState = false;
}

extern "C" void turn_backlight_on()
{
    if ( bBacklightState == true )
        return;
    expander->digitalWrite(LCD_BL, HIGH);
    bBacklightState = true;
}

extern "C" void reset_panel()
{
    expander->digitalWrite(LCD_RST, LOW);
    vTaskDelay(pdMS_TO_TICKS(10));
    expander->digitalWrite(LCD_RST, HIGH);
}
#endif