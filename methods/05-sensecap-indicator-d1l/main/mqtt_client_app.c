// mqtt_client_app.c — esp-mqtt client + health-topic JSON parsing.
#include "mqtt_client_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "sdkconfig.h"

#include "fleet_state.h"
#include "time_sync.h"

static const char *TAG = "mqtt_app";

#define GW   CONFIG_FLEET_GATEWAY_SN   // e.g. "43250372"

// Subscriptions — the health channels from the README "How it gets its data".
// We add the .150 Hologram-poller summary topic for the 4G/SIM screen.
static const char *const SUB_TOPICS[] = {
    "access360/" GW "/proc/checkin/notify",
    "access360/" GW "/dyn/batt/notify",
    "access360/" GW "/proc/reading/notify",   // WS100 carries Batt here too
    "access360/" GW "/rssi/notify",
    "access360/" GW "/error/notify",
    "access360/" GW "/status/notify",
    "access360/" GW "/ap/notify",
    "access360/" GW "/will",
    "access360/" GW "/sim/usage",             // from the Hologram poller on .150
};

static esp_mqtt_client_handle_t s_client;

// --- helpers ----------------------------------------------------------------

// Extract the channel suffix (everything after "access360/<gw>/") from a topic.
// Returns a pointer into `topic` or NULL if the prefix doesn't match.
static const char *channel_of(const char *topic, int topic_len)
{
    static const char prefix[] = "access360/" GW "/";
    size_t plen = sizeof(prefix) - 1;
    if ((int)plen > topic_len) return NULL;
    if (strncmp(topic, prefix, plen) != 0) return NULL;
    return topic + plen;
}

static uint32_t json_serial(const cJSON *root)
{
    const cJSON *s = cJSON_GetObjectItemCaseSensitive(root, "Serial");
    if (cJSON_IsNumber(s)) return (uint32_t)s->valuedouble;
    if (cJSON_IsString(s) && s->valuestring) return (uint32_t)strtoul(s->valuestring, NULL, 10);
    return 0;
}

static bool json_int(const cJSON *root, const char *key, int *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(v)) { *out = (int)v->valuedouble; return true; }
    if (cJSON_IsString(v) && v->valuestring) { *out = atoi(v->valuestring); return true; }
    return false;
}

static bool json_bool(const cJSON *root, const char *key, bool *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsBool(v)) { *out = cJSON_IsTrue(v); return true; }
    if (cJSON_IsNumber(v)) { *out = v->valuedouble != 0; return true; }
    return false;
}

// --- per-channel handlers ---------------------------------------------------

static void handle_payload(const char *channel, const char *data, int data_len)
{
    fleet_note_message();

    // Parse JSON (esp-mqtt gives us a non-null-terminated buffer).
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (!root) {
        // A failed parse is itself a fleet "error" (per fleet-health-metrics).
        fleet_note_error();
        ESP_LOGW(TAG, "JSON parse failed on %s", channel);
        return;
    }

    int64_t now = fleet_now();
    uint32_t serial = json_serial(root);
    fleet_model_t model = fleet_model_for(serial, channel);

    if (strstr(channel, "proc/checkin")) {
        // Heartbeat: pure presence/last-seen.
        fleet_touch_sensor(serial, model, now);

    } else if (strstr(channel, "dyn/batt") || strstr(channel, "proc/reading")) {
        // Battery: dyn/batt/notify and (WS100) proc/reading/notify both carry Batt.
        int batt;
        if (json_int(root, "Batt", &batt)) {
            fleet_set_batt(serial, model, batt, now);
        } else {
            // proc/reading without Batt still proves the sensor is alive.
            fleet_touch_sensor(serial, model, now);
        }

    } else if (strstr(channel, "rssi")) {
        int rssi;
        if (json_int(root, "Rssi", &rssi)) {
            fleet_set_rssi(serial, model, rssi, now);
        }

    } else if (strstr(channel, "error")) {
        fleet_note_error();
        const cJSON *e = cJSON_GetObjectItemCaseSensitive(root, "Error");
        fleet_set_last_error((cJSON_IsString(e) && e->valuestring) ? e->valuestring : "error");

    } else if (strstr(channel, "status")) {
        // Status events don't change health directly; counted as a message above.

    } else if (strstr(channel, "ap/notify")) {
        // Gateway presence.
        bool connected = true;
        json_bool(root, "Connected", &connected);
        fleet_set_gateway_online(connected);

    } else if (strstr(channel, "will")) {
        // Last-Will fired ⇒ gateway dropped.
        fleet_set_gateway_online(false);

    } else if (strstr(channel, "sim/usage")) {
        // From the Hologram poller on .150. All fields best-effort.
        fleet_sim_t sim = { 0 };
        sim.last_connect_age_s = -1;
        json_int(root, "used_mb", &sim.used_mb);
        json_int(root, "limit_mb", &sim.limit_mb);
        if (!json_int(root, "pct", &sim.pct) && sim.limit_mb > 0) {
            sim.pct = (int)((100.0 * sim.used_mb) / sim.limit_mb);
        }
        json_bool(root, "paused", &sim.paused);
        int age;
        if (json_int(root, "last_connect_age_s", &age)) sim.last_connect_age_s = age;
        sim.updated_at = now;
        fleet_set_sim(&sim);
    }

    cJSON_Delete(root);
}

// --- esp-mqtt event handler -------------------------------------------------

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "broker connected");
        fleet_set_broker_connected(true);
        for (size_t i = 0; i < sizeof(SUB_TOPICS) / sizeof(SUB_TOPICS[0]); i++) {
            esp_mqtt_client_subscribe(s_client, SUB_TOPICS[i], 1); // QoS 1
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "broker disconnected");
        fleet_set_broker_connected(false);
        break;

    case MQTT_EVENT_DATA: {
        // esp-mqtt may deliver a payload across multiple DATA events for very
        // large messages, but the health topics here are all small single-frame
        // JSON, so we handle each DATA event as a complete payload. (The big
        // multipart dyn/vib waveform is intentionally NOT subscribed.)
        const char *ch = channel_of(event->topic, event->topic_len);
        if (ch) {
            // ch points into event->topic (not null-terminated); make a small
            // null-terminated copy for strstr() matching.
            char chbuf[64];
            int chlen = event->topic_len - (int)(ch - event->topic);
            if (chlen < 0) chlen = 0;
            if (chlen >= (int)sizeof(chbuf)) chlen = sizeof(chbuf) - 1;
            memcpy(chbuf, ch, chlen);
            chbuf[chlen] = '\0';
            handle_payload(chbuf, event->data, event->data_len);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
}

void mqtt_app_start(void)
{
    char uri[64];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d",
             CONFIG_FLEET_MQTT_BROKER_HOST, CONFIG_FLEET_MQTT_BROKER_PORT);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .session.keepalive = CONFIG_FLEET_MQTT_KEEPALIVE,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,   // MQTT 5.0; anonymous
        .credentials.client_id = "indicator-d1l",
        .network.reconnect_timeout_ms = 5000,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT client started -> %s", uri);
}
