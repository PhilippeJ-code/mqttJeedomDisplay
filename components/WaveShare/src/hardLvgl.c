// hardLvgl.c
//
#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_LVGL

#include "hardLvgl.h"
#include "gt911.h"
#include "rgbPanel.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_timer.h"

#include "ioExpander.h"
#include "lvgl.h"

#define LCD_H_RES 800
#define LCD_V_RES 480

#define LCD_LVGL_TICK_PERIOD_MS 2
#define LCD_LVGL_TASK_MAX_DELAY_MS 500
#define LCD_LVGL_TASK_MIN_DELAY_MS 1
#define LCD_LVGL_TASK_STACK_SIZE (8 * 1024)
#define LCD_LVGL_TASK_PRIORITY 2

static SemaphoreHandle_t lvgl_mux = NULL;

static lv_display_t *disp;
static lv_indev_t *indev;

#ifdef CONFIG_WS_USE_SCREENSAVER
static bool bScreenSaver = false;
static bool bResetScreenSaver = false;
#endif

static void hardLvglFlush(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(drv);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_display_flush_ready(drv);
}

static void hardLvglTouch(lv_indev_t *indev, lv_indev_data_t *data)
{

    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    assert(tp);

    uint16_t touchpad_x;
    uint16_t touchpad_y;
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_read_data(tp);

    bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, &touchpad_x, &touchpad_y, NULL, &touchpad_cnt, 1);

#ifdef CONFIG_WS_USE_SCREENSAVER
    if (touchpad_pressed && bScreenSaver)
    {
        bResetScreenSaver = true;
        data->point.x = 0;
        data->point.y = 0;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
#endif

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        data->point.x = touchpad_x;
        data->point.y = touchpad_y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void hardLvglIncreaseTick(void *arg)
{
    lv_tick_inc(LCD_LVGL_TICK_PERIOD_MS);
}

static void hardLvglTask(void *arg)
{
    uint32_t task_delay_ms = LCD_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {

        if (hardLvglLock(-1))
        {

            task_delay_ms = lv_timer_handler();
            if (task_delay_ms > LCD_LVGL_TASK_MAX_DELAY_MS)
            {
                task_delay_ms = LCD_LVGL_TASK_MAX_DELAY_MS;
            }
            else if (task_delay_ms < LCD_LVGL_TASK_MIN_DELAY_MS)
            {
                task_delay_ms = LCD_LVGL_TASK_MIN_DELAY_MS;
            }

#ifdef CONFIG_WS_USE_SCREENSAVER
            if (lv_disp_get_inactive_time(NULL) > CONFIG_WS_TIMER_VALUE * 1000)
            {
                if (bScreenSaver == false)
                {
                    turn_backlight_off();
                    bScreenSaver = true;
                }
            }
            if ( bResetScreenSaver && bScreenSaver )
            {
                bResetScreenSaver = false;
                bScreenSaver = false;
                turn_backlight_on();
            }
#endif
            hardLvglUnlock();
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void hardLvglInit()
{
    lvgl_mux = xSemaphoreCreateRecursiveMutex();

    lv_init();

    void *buf1 = NULL;
    void *buf2 = NULL;

    disp = lv_display_create(LCD_H_RES, LCD_V_RES);

    uint8_t pixelSize = lv_color_format_get_size(lv_display_get_color_format(disp));
    buf1 = heap_caps_malloc(LCD_H_RES * 100 * pixelSize, MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_malloc(LCD_H_RES * 100 * pixelSize, MALLOC_CAP_SPIRAM);
    assert(buf2);

    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_flush_cb(disp, hardLvglFlush);
    lv_display_set_buffers(disp, buf1, buf2, LCD_H_RES * 100 * pixelSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(indev, gt911GetTp());
    lv_indev_set_read_cb(indev, hardLvglTouch);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &hardLvglIncreaseTick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LCD_LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreate(hardLvglTask, "LVGL", LCD_LVGL_TASK_STACK_SIZE, NULL, LCD_LVGL_TASK_PRIORITY, NULL);
}

char hardLvglLock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void hardLvglUnlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

#endif