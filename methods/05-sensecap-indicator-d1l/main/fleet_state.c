// fleet_state.c — implementation of the shared fleet-health table.
#include "fleet_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MSG_WINDOW_SLOTS   64     // ring of recent message timestamps (ms)
#define MSG_WINDOW_MS      5000   // sliding window length for rates

static SemaphoreHandle_t s_mtx;

static fleet_sensor_t s_sensors[FLEET_MAX_SENSORS];
static int            s_sensor_count;

static bool s_broker_connected;
static bool s_gateway_online;
static char s_last_error[FLEET_ERROR_STR_LEN];

static fleet_sim_t s_sim;

// Sliding-window ring buffers of arrival times (esp_timer ms via xTaskGetTickCount-free clock).
static int64_t s_msg_ring[MSG_WINDOW_SLOTS];
static int     s_msg_head;
static int64_t s_err_ring[MSG_WINDOW_SLOTS];
static int     s_err_head;

// We use a millisecond monotonic clock from FreeRTOS ticks for the rate windows
// (rates don't need wall-clock; they just need monotonic deltas).
static inline int64_t now_ms(void)
{
    return (int64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static inline void lock(void)   { xSemaphoreTake(s_mtx, portMAX_DELAY); }
static inline void unlock(void) { xSemaphoreGive(s_mtx); }

void fleet_state_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    s_sensor_count = 0;
    s_broker_connected = false;
    s_gateway_online = false;
    s_last_error[0] = '\0';
    memset(&s_sim, 0, sizeof(s_sim));
    s_sim.last_connect_age_s = -1;
    memset(s_msg_ring, 0, sizeof(s_msg_ring));
    memset(s_err_ring, 0, sizeof(s_err_ring));
    s_msg_head = 0;
    s_err_head = 0;
}

// --- Model classification ---------------------------------------------------
fleet_model_t fleet_model_for(uint32_t serial, const char *channel)
{
    // Channel wins: a proc/reading message is always a WS100, even though its
    // serial may share the 1125... prefix with a WS200.
    if (channel && strstr(channel, "proc/reading") != NULL) {
        return FLEET_MODEL_WS100;
    }
    char s[FLEET_SERIAL_STR_LEN];
    snprintf(s, sizeof(s), "%lu", (unsigned long)serial);
    if (strncmp(s, "2225", 4) == 0) return FLEET_MODEL_WS300;  // triaxial dynamic
    if (strncmp(s, "1125", 4) == 0) return FLEET_MODEL_WS200;  // single-axial dynamic
    return FLEET_MODEL_UNKNOWN;
}

const char *fleet_model_name(fleet_model_t m)
{
    switch (m) {
        case FLEET_MODEL_WS100: return "WS100";
        case FLEET_MODEL_WS200: return "WS200";
        case FLEET_MODEL_WS300: return "WS300";
        default:                return "?";
    }
}

// Find or create the row for a serial (caller holds the lock). Returns NULL if
// the table is full.
static fleet_sensor_t *find_or_add(uint32_t serial, fleet_model_t model_hint)
{
    for (int i = 0; i < s_sensor_count; i++) {
        if (s_sensors[i].serial == serial) {
            // Upgrade model if we learned a more specific one (e.g. a later
            // proc/reading message confirms WS100).
            if (s_sensors[i].model == FLEET_MODEL_UNKNOWN &&
                model_hint != FLEET_MODEL_UNKNOWN) {
                s_sensors[i].model = model_hint;
            }
            // proc/reading is authoritative for WS100 — let it override.
            if (model_hint == FLEET_MODEL_WS100) {
                s_sensors[i].model = FLEET_MODEL_WS100;
            }
            return &s_sensors[i];
        }
    }
    if (s_sensor_count >= FLEET_MAX_SENSORS) return NULL;
    fleet_sensor_t *e = &s_sensors[s_sensor_count++];
    memset(e, 0, sizeof(*e));
    e->serial = serial;
    snprintf(e->serial_str, sizeof(e->serial_str), "%lu", (unsigned long)serial);
    e->model = model_hint;
    return e;
}

// --- Writers ----------------------------------------------------------------
void fleet_touch_sensor(uint32_t serial, fleet_model_t model_hint, int64_t now)
{
    if (serial == 0) return;
    lock();
    fleet_sensor_t *e = find_or_add(serial, model_hint);
    if (e) {
        e->have_seen = true;
        e->last_seen = now;
    }
    unlock();
}

void fleet_set_batt(uint32_t serial, fleet_model_t model_hint, int batt_pct, int64_t now)
{
    if (serial == 0) return;
    lock();
    fleet_sensor_t *e = find_or_add(serial, model_hint);
    if (e) {
        e->have_batt = true;
        e->batt_pct = batt_pct;
        e->have_seen = true;
        e->last_seen = now;
    }
    unlock();
}

void fleet_set_rssi(uint32_t serial, fleet_model_t model_hint, int rssi_dbm, int64_t now)
{
    if (serial == 0) return;
    lock();
    fleet_sensor_t *e = find_or_add(serial, model_hint);
    if (e) {
        e->have_rssi = true;
        e->rssi_dbm = rssi_dbm;
        e->have_seen = true;
        e->last_seen = now;
    }
    unlock();
}

void fleet_set_broker_connected(bool connected)
{
    lock(); s_broker_connected = connected; unlock();
}

void fleet_set_gateway_online(bool online)
{
    lock(); s_gateway_online = online; unlock();
}

void fleet_set_last_error(const char *err)
{
    lock();
    if (err) {
        strncpy(s_last_error, err, sizeof(s_last_error) - 1);
        s_last_error[sizeof(s_last_error) - 1] = '\0';
    }
    unlock();
}

void fleet_note_message(void)
{
    lock();
    s_msg_ring[s_msg_head] = now_ms();
    s_msg_head = (s_msg_head + 1) % MSG_WINDOW_SLOTS;
    unlock();
}

void fleet_note_error(void)
{
    lock();
    s_err_ring[s_err_head] = now_ms();
    s_err_head = (s_err_head + 1) % MSG_WINDOW_SLOTS;
    unlock();
}

void fleet_set_sim(const fleet_sim_t *sim)
{
    if (!sim) return;
    lock();
    s_sim = *sim;
    s_sim.have_sim = true;
    unlock();
}

// --- Readers ----------------------------------------------------------------
bool fleet_get_broker_connected(void)
{
    lock(); bool v = s_broker_connected; unlock(); return v;
}

bool fleet_get_gateway_online(void)
{
    lock(); bool v = s_gateway_online; unlock(); return v;
}

void fleet_get_last_error(char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    lock();
    strncpy(out, s_last_error, out_len - 1);
    out[out_len - 1] = '\0';
    unlock();
}

static float rate_from_ring(const int64_t *ring, int64_t cutoff)
{
    int n = 0;
    for (int i = 0; i < MSG_WINDOW_SLOTS; i++) {
        if (ring[i] != 0 && ring[i] >= cutoff) n++;
    }
    return (float)n / (MSG_WINDOW_MS / 1000.0f);
}

float fleet_messages_per_sec(void)
{
    int64_t cutoff = now_ms() - MSG_WINDOW_MS;
    lock(); float r = rate_from_ring(s_msg_ring, cutoff); unlock();
    return r;
}

float fleet_errors_per_sec(void)
{
    int64_t cutoff = now_ms() - MSG_WINDOW_MS;
    lock(); float r = rate_from_ring(s_err_ring, cutoff); unlock();
    return r;
}

static int cmp_serial(const void *a, const void *b)
{
    const fleet_sensor_t *x = a, *y = b;
    if (x->serial < y->serial) return -1;
    if (x->serial > y->serial) return 1;
    return 0;
}

int fleet_snapshot_sensors(fleet_sensor_t *out, int max_out)
{
    if (!out || max_out <= 0) return 0;
    lock();
    int n = s_sensor_count < max_out ? s_sensor_count : max_out;
    memcpy(out, s_sensors, n * sizeof(fleet_sensor_t));
    unlock();
    qsort(out, n, sizeof(fleet_sensor_t), cmp_serial);
    return n;
}

int fleet_online_count(int64_t now, int online_cutoff_s)
{
    int c = 0;
    lock();
    for (int i = 0; i < s_sensor_count; i++) {
        if (s_sensors[i].have_seen &&
            (now - s_sensors[i].last_seen) < online_cutoff_s) {
            c++;
        }
    }
    unlock();
    return c;
}

void fleet_get_sim(fleet_sim_t *out)
{
    if (!out) return;
    lock(); *out = s_sim; unlock();
}
