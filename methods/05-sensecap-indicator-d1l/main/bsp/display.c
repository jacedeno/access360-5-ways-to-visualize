// display.c — display + touch + LVGL 9 bring-up for the SenseCAP Indicator D1L.
//
// The D1L drives a 4" 480x480 IPS panel from the ESP32-S3 over a 16-bit RGB
// interface (ST7701S init via 3-wire SPI), with an FT5x06-family capacitive
// touch controller on I2C. The exact GPIO map, RGB timing, and the ST7701S
// init sequence are board-specific; the values below come from the SenseCAP
// Indicator reference design and are marked for hardware verification.
//
// TODO: verify against D1L hardware — every pin/timing constant in this file,
// and whether the panel really is ST7701S over 16-bit RGB (vs. a Seeed BSP that
// owns this). If Seeed's BSP managed component is available, prefer it and have
// it create the esp_lcd panel + touch handles, then keep only the LVGL glue
// (lvgl_port / display+indev registration) from this file.

#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"

#include "lvgl.h"

static const char *TAG = "bsp_display";

// ---------------------------------------------------------------------------
// Hardware constants — TODO: verify against D1L hardware
// ---------------------------------------------------------------------------
// 16-bit parallel RGB data lines + sync. These are placeholders from the
// reference design; confirm against the D1L schematic before flashing.
#define LCD_PIXEL_CLOCK_HZ     (16 * 1000 * 1000)
#define LCD_PIN_PCLK           45
#define LCD_PIN_VSYNC          41
#define LCD_PIN_HSYNC          39
#define LCD_PIN_DE             40
#define LCD_PIN_DISP_EN        -1   // not wired / always on
// R0..R4, G0..G5, B0..B4 (RGB565). TODO: verify against D1L hardware.
#define LCD_PIN_DATA0          21   // B0
#define LCD_PIN_DATA1          14
#define LCD_PIN_DATA2          13
#define LCD_PIN_DATA3          12
#define LCD_PIN_DATA4          11
#define LCD_PIN_DATA5          10   // G0
#define LCD_PIN_DATA6          9
#define LCD_PIN_DATA7          46
#define LCD_PIN_DATA8          3
#define LCD_PIN_DATA9          8
#define LCD_PIN_DATA10         18
#define LCD_PIN_DATA11         17   // R0
#define LCD_PIN_DATA12         16
#define LCD_PIN_DATA13         15
#define LCD_PIN_DATA14         7
#define LCD_PIN_DATA15         6

// RGB porch timing (typical ST7701S 480x480). TODO: verify against D1L hardware.
#define LCD_HSYNC_BACK_PORCH   40
#define LCD_HSYNC_FRONT_PORCH  20
#define LCD_HSYNC_PULSE_WIDTH  10
#define LCD_VSYNC_BACK_PORCH   30
#define LCD_VSYNC_FRONT_PORCH  20
#define LCD_VSYNC_PULSE_WIDTH  10

// Touch (FT5x06 family) on I2C. TODO: verify against D1L hardware.
#define TOUCH_I2C_NUM          I2C_NUM_0
#define TOUCH_I2C_SDA          39   // TODO: D1L uses dedicated touch I2C pins
#define TOUCH_I2C_SCL          38
#define TOUCH_I2C_FREQ_HZ      (400 * 1000)
#define TOUCH_PIN_RST          -1
#define TOUCH_PIN_INT          -1

#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_STACK        8192
#define LVGL_TASK_PRIO         4

// ---------------------------------------------------------------------------

static SemaphoreHandle_t s_lvgl_mutex;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static lv_display_t  *s_disp;
static lv_indev_t    *s_indev;

// LVGL 9 flush: copy the rendered area into the RGB framebuffer.
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    int x1 = area->x1, y1 = area->y1, x2 = area->x2, y2 = area->y2;
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

// LVGL 9 touch read.
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    uint16_t x = 0, y = 0;
    uint8_t cnt = 0;
    esp_lcd_touch_read_data(s_touch);
    bool pressed = esp_lcd_touch_get_coordinates(s_touch, &x, &y, NULL, &cnt, 1);
    if (pressed && cnt > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (1) {
        if (bsp_display_lock(-1)) {
            uint32_t delay_ms = lv_timer_handler();
            bsp_display_unlock();
            if (delay_ms > 100) delay_ms = 100;
            if (delay_ms < 5)   delay_ms = 5;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

static int panel_init(void)
{
    // 16-bit RGB panel. TODO: verify against D1L hardware (and add the ST7701S
    // 3-wire SPI init sequence the D1L needs before RGB streaming — typically a
    // short SPI command burst; some Seeed BSPs ship it as a separate vendor
    // init array).
    esp_lcd_rgb_panel_config_t panel_cfg = {
        .data_width = 16,
        .psram_trans_align = 64,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .disp_gpio_num = LCD_PIN_DISP_EN,
        .pclk_gpio_num = LCD_PIN_PCLK,
        .vsync_gpio_num = LCD_PIN_VSYNC,
        .hsync_gpio_num = LCD_PIN_HSYNC,
        .de_gpio_num = LCD_PIN_DE,
        .data_gpio_nums = {
            LCD_PIN_DATA0, LCD_PIN_DATA1, LCD_PIN_DATA2, LCD_PIN_DATA3,
            LCD_PIN_DATA4, LCD_PIN_DATA5, LCD_PIN_DATA6, LCD_PIN_DATA7,
            LCD_PIN_DATA8, LCD_PIN_DATA9, LCD_PIN_DATA10, LCD_PIN_DATA11,
            LCD_PIN_DATA12, LCD_PIN_DATA13, LCD_PIN_DATA14, LCD_PIN_DATA15,
        },
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .hsync_back_porch = LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
            .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
            .vsync_back_porch = LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
            .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
    };

    esp_err_t err = esp_lcd_new_rgb_panel(&panel_cfg, &s_panel);
    if (err != ESP_OK) { ESP_LOGE(TAG, "rgb panel create failed: %d", err); return err; }
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    return ESP_OK;
}

static int touch_init(void)
{
    // TODO: verify against D1L hardware — touch I2C pins, address, swap/mirror.
    const i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = TOUCH_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(TOUCH_I2C_NUM, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(TOUCH_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0));

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_I2C_NUM,
                                             &tp_io_cfg, &tp_io));

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = TOUCH_PIN_RST,
        .int_gpio_num = TOUCH_PIN_INT,
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    esp_err_t err = esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &s_touch);
    if (err != ESP_OK) {
        // Touch is non-fatal: screens still render and auto-advance is available
        // via a fallback timer in the UI, so don't abort the whole bring-up.
        ESP_LOGW(TAG, "touch init failed (%d) — running display-only", err);
        s_touch = NULL;
    }
    return ESP_OK;
}

static int lvgl_init(void)
{
    lv_init();

    // Two partial draw buffers in PSRAM. RGB565 -> 2 bytes/pixel. A band of
    // ~1/10 the screen keeps RAM modest while staying smooth for a status UI.
    size_t buf_px = BSP_LCD_H_RES * (BSP_LCD_V_RES / 10);
    size_t buf_bytes = buf_px * sizeof(lv_color16_t);
    void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) { ESP_LOGE(TAG, "LVGL buffer alloc failed"); return -1; }

    s_disp = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_display_set_user_data(s_disp, s_panel);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

    if (s_touch) {
        s_indev = lv_indev_create();
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indev, lvgl_touch_cb);
    }

    // LVGL tick from esp_timer.
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb, .name = "lvgl_tick",
    };
    esp_timer_handle_t tick;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, LVGL_TICK_PERIOD_MS * 1000));
    return 0;
}

int bsp_display_start(void)
{
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();

    if (panel_init() != ESP_OK) return -1;
    touch_init();
    if (lvgl_init() != 0) return -1;

    xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL, LVGL_TASK_PRIO, NULL);
    ESP_LOGI(TAG, "display + LVGL up (%dx%d)", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return 0;
}

bool bsp_display_lock(int timeout_ms)
{
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, ticks) == pdTRUE;
}

void bsp_display_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}
