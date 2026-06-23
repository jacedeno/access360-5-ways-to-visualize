// ui_internal.h — shared state + helpers between ui.c and the screen modules.
#pragma once

#include "lvgl.h"
#include "ui.h"
#include "display.h"   // BSP_LCD_*/BSP_CONTENT_* geometry

#ifdef __cplusplus
extern "C" {
#endif

#define UI_MAX_ROWS 8   // >= FLEET_MAX expected (desk fleet is 5)

// Threshold colors (docs/fleet-health-metrics.md).
#define UI_COLOR_OK      lv_color_hex(0x2ecc71)   // green
#define UI_COLOR_WARN    lv_color_hex(0xf39c12)   // amber
#define UI_COLOR_CRIT    lv_color_hex(0xe74c3c)   // red
#define UI_COLOR_UNKNOWN lv_color_hex(0x7f8c8d)   // grey
#define UI_COLOR_BG      lv_color_hex(0x10141a)
#define UI_COLOR_CARD    lv_color_hex(0x1b212b)
#define UI_COLOR_TEXT    lv_color_hex(0xecf0f1)

lv_color_t ui_sev_color(ui_sev_t sev);

// Each screen builds itself into a parent container and exposes its widgets via
// this struct (one global instance, ui_g).
typedef struct {
    // Navigation
    lv_obj_t *screens[UI_SCREEN_COUNT];
    int       active;
    lv_obj_t *dots[UI_SCREEN_COUNT];   // page-indicator dots in the footer

    // Screen 1: Broker / Gateway
    lv_obj_t *broker_led;
    lv_obj_t *broker_lbl;
    lv_obj_t *gw_led;
    lv_obj_t *gw_lbl;
    lv_obj_t *mps_lbl;
    lv_obj_t *err_lbl;

    // Screen 2: Sensors
    lv_obj_t *online_lbl;
    lv_obj_t *sensor_rows[UI_MAX_ROWS];     // a container per row
    lv_obj_t *sensor_serial[UI_MAX_ROWS];
    lv_obj_t *sensor_age[UI_MAX_ROWS];
    lv_obj_t *sensor_dot[UI_MAX_ROWS];

    // Screen 3: Power & Signal
    lv_obj_t *power_rows[UI_MAX_ROWS];
    lv_obj_t *power_serial[UI_MAX_ROWS];
    lv_obj_t *power_batt[UI_MAX_ROWS];
    lv_obj_t *power_rssi[UI_MAX_ROWS];

    // Screen 4: 4G / SIM
    lv_obj_t *sim_bar;
    lv_obj_t *sim_pct_lbl;
    lv_obj_t *sim_used_lbl;
    lv_obj_t *sim_state_lbl;
    lv_obj_t *sim_conn_lbl;
    lv_obj_t *sim_poll_lbl;
} ui_globals_t;

extern ui_globals_t ui_g;

// Navigation (called by buttons/swipe and exposed to screen builders).
void ui_nav_goto(int idx);
void ui_nav_next(void);
void ui_nav_prev(void);

// Build a standard screen scaffold: a full-screen container with a title bar and
// a footer holding Back / page-dots / Next. Returns the content area object the
// caller fills, and writes the screen container into *out_screen.
lv_obj_t *ui_make_scaffold(const char *title, lv_obj_t **out_screen);

// Screen builders (each returns its top-level screen object).
lv_obj_t *ui_build_screen_broker(void);
lv_obj_t *ui_build_screen_sensors(void);
lv_obj_t *ui_build_screen_power(void);
lv_obj_t *ui_build_screen_sim(void);

#ifdef __cplusplus
}
#endif
