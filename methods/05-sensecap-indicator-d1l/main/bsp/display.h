// display.h — SenseCAP Indicator D1L display + touch + LVGL bring-up.
//
// Self-contained esp_lcd path so the project builds without a specific Seeed BSP
// version. Panel/touch constants below are taken from the SenseCAP Indicator
// reference design and MUST be verified against the actual D1L hardware before
// flashing (search for "TODO: verify against D1L hardware").
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Panel geometry (480x480 capacitive touchscreen).
#define BSP_LCD_H_RES   480
#define BSP_LCD_V_RES   480

// Content area inside the standard screen scaffold (below the title bar, above
// the Back/dots/Next footer). Used by the UI screen builders.
#define BSP_CONTENT_W   456
#define BSP_CONTENT_H   356

// Initializes the RGB/QSPI panel, the capacitive touch controller, and LVGL 9
// (display + input device + tick). After this returns, take bsp_display_lock()
// before touching any LVGL object, and bsp_display_unlock() after.
//
// Returns ESP_OK on success.
int bsp_display_start(void);

// LVGL is not thread-safe. Any code that creates/updates widgets from outside
// the LVGL task (e.g. the periodic fleet-state push) must hold this lock.
bool bsp_display_lock(int timeout_ms);   // -1 = wait forever
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif
