// ui.h — clean LVGL 9 UI API for the fleet-health monitor.
//
// The firmware calls ui_create() once (under the LVGL lock), then pushes state
// in via the ui_set_* functions on a timer. The UI owns four screens with
// back/next navigation (also swipeable):
//   0 Broker / Gateway
//   1 Sensors
//   2 Power & Signal
//   3 4G / SIM
//
// All ui_set_* calls MUST be made while holding bsp_display_lock() because they
// mutate LVGL objects. The status push task in main.c does this.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Severity drives badge/row color, mapped to the thresholds in
// docs/fleet-health-metrics.md.
typedef enum {
    UI_OK = 0,     // green
    UI_WARN,       // amber
    UI_CRIT,       // red
    UI_UNKNOWN,    // grey (no data yet)
} ui_sev_t;

#define UI_SCREEN_COUNT 4

// Build all screens and show screen 0. Call once, under the LVGL lock.
void ui_create(void);

// --- Screen 1: Broker / Gateway --------------------------------------------
void ui_set_broker_connected(bool connected);
void ui_set_gateway_online(bool online);
void ui_set_messages_per_sec(float mps);
void ui_set_last_error(const char *err);   // "" or NULL => "none"

// --- Screen 2: Sensors ------------------------------------------------------
void ui_set_online_count(int online, int total);
// Update one sensor row. Call with each known sensor; rows are addressed by
// index 0..(rows-1). serial_str/model are short strings. age_s < 0 => unknown.
void ui_set_sensor_row(int row, const char *serial_str, const char *model,
                       int64_t age_s, bool online);
void ui_set_sensor_row_count(int count);   // hide unused rows

// --- Screen 3: Power & Signal ----------------------------------------------
// have_batt/have_rssi gate whether the value or a dash is shown.
void ui_set_power_row(int row, const char *serial_str, const char *model,
                      bool have_batt, int batt_pct,
                      bool have_rssi, int rssi_dbm);
void ui_set_power_row_count(int count);

// --- Screen 4: 4G / SIM -----------------------------------------------------
void ui_set_sim(bool have_sim, int used_mb, int limit_mb, int pct,
                bool paused, int64_t last_connect_age_s, int64_t poll_age_s);

#ifdef __cplusplus
}
#endif
