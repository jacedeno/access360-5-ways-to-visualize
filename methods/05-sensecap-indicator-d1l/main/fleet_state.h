// fleet_state.h — shared, thread-safe fleet-health state for the SenseCAP
// Indicator D1L. The MQTT task writes into this table; a periodic timer/UI task
// reads it to recompute "online" and per-sensor ages and push values into LVGL.
//
// All mutating and reading helpers take an internal mutex, so callers from the
// MQTT event task and the LVGL/timer task are safe without external locking.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLEET_MAX_SENSORS        16
#define FLEET_SERIAL_STR_LEN     16
#define FLEET_MODEL_STR_LEN      12
#define FLEET_ERROR_STR_LEN      96

// Sensor model, derived channel-first then by serial prefix (see
// docs/sensors-and-gateways.md). proc/reading/notify always implies WS100.
typedef enum {
    FLEET_MODEL_UNKNOWN = 0,
    FLEET_MODEL_WS100,   // process control (proc/reading/notify)
    FLEET_MODEL_WS200,   // single-axial dynamic (serial 1125...)
    FLEET_MODEL_WS300,   // triaxial dynamic   (serial 2225...)
} fleet_model_t;

// Per-sensor health record. last_seen is a UTC epoch when SNTP is synced, or a
// monotonic boot-relative seconds value when it is not (see time_sync.h).
typedef struct {
    uint32_t serial;                 // sensor serial (payload "Serial")
    char     serial_str[FLEET_SERIAL_STR_LEN];
    fleet_model_t model;

    bool     have_batt;
    int      batt_pct;               // 0..100

    bool     have_rssi;
    int      rssi_dbm;               // negative; closer to 0 = stronger

    bool     have_seen;
    int64_t  last_seen;              // seconds (epoch if absolute, else boot-relative)
} fleet_sensor_t;

// 4G / Hologram SIM summary, published by the .150 poller on
// access360/<gw>/sim/usage. All best-effort; have_sim gates display.
typedef struct {
    bool     have_sim;
    int      used_mb;
    int      limit_mb;
    int      pct;                    // used/limit * 100 (poller-computed)
    bool     paused;
    int64_t  last_connect_age_s;     // best-effort; -1 if unknown
    int64_t  updated_at;             // when this device last received a sim msg
} fleet_sim_t;

void fleet_state_init(void);

// --- Model classification (channel-first, prefix-second) --------------------
// channel is the topic suffix after the gateway serial, e.g.
// "proc/reading/notify" or "dyn/vib/notify". serial may be 0 if unknown.
fleet_model_t fleet_model_for(uint32_t serial, const char *channel);
const char   *fleet_model_name(fleet_model_t m);

// --- Writers (called from the MQTT event task) ------------------------------
// Each updater also bumps the sensor's last_seen to "now" (any message proves
// the sensor was just heard from). Pass model_hint from fleet_model_for().
void fleet_touch_sensor(uint32_t serial, fleet_model_t model_hint, int64_t now);
void fleet_set_batt(uint32_t serial, fleet_model_t model_hint, int batt_pct, int64_t now);
void fleet_set_rssi(uint32_t serial, fleet_model_t model_hint, int rssi_dbm, int64_t now);

void fleet_set_broker_connected(bool connected);
void fleet_set_gateway_online(bool online);
void fleet_set_last_error(const char *err);     // copies up to FLEET_ERROR_STR_LEN
void fleet_note_message(void);                  // bump messages-counter (sliding window)
void fleet_note_error(void);                    // bump error-counter (sliding window)

void fleet_set_sim(const fleet_sim_t *sim);

// --- Readers / derived metrics (called from the timer/UI task) --------------
bool fleet_get_broker_connected(void);
bool fleet_get_gateway_online(void);
void fleet_get_last_error(char *out, size_t out_len);

// Messages/s over the sliding window (computed from the ring of timestamps).
float fleet_messages_per_sec(void);
float fleet_errors_per_sec(void);

// Snapshot the sensor table (sorted by serial). Returns the count written.
// online_cutoff_s + now are used to derive online/age in the UI layer.
int fleet_snapshot_sensors(fleet_sensor_t *out, int max_out);

// Count sensors with (now - last_seen) < cutoff. Sensors with no last_seen yet
// are treated as offline.
int fleet_online_count(int64_t now, int online_cutoff_s);

void fleet_get_sim(fleet_sim_t *out);

#ifdef __cplusplus
}
#endif
