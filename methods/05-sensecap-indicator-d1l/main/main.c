// main.c — app entry for the SenseCAP Indicator D1L fleet-health monitor.
//
// Boot sequence:
//   1. NVS init        (Wi-Fi calibration + config storage)
//   2. Display + LVGL  (so the UI shows "connecting" immediately)
//   3. Wi-Fi STA       (SSID/PSK from menuconfig)
//   4. SNTP            (absolute time for last-seen ages; falls back to boot time)
//   5. esp-mqtt        (subscribe to the gateway health topics)
//   6. Refresh timer   (recompute derived metrics + push into LVGL @1 Hz)
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "bsp/display.h"
#include "ui/ui.h"
#include "wifi_sta.h"
#include "time_sync.h"
#include "mqtt_client_app.h"
#include "fleet_state.h"

static const char *TAG = "main";

// Push the current fleet state into the LVGL widgets. Runs on the esp_timer
// task; takes the LVGL lock around all ui_set_* calls.
static void refresh_cb(void *arg)
{
    (void)arg;
    int64_t now = fleet_now();

    // --- gather (no LVGL lock needed; fleet_state is internally locked) -----
    bool broker = fleet_get_broker_connected();
    bool gw     = fleet_get_gateway_online();
    float mps   = fleet_messages_per_sec();
    char err[FLEET_ERROR_STR_LEN];
    fleet_get_last_error(err, sizeof(err));

    fleet_sensor_t sensors[FLEET_MAX_SENSORS];
    int n = fleet_snapshot_sensors(sensors, FLEET_MAX_SENSORS);
    int online = fleet_online_count(now, CONFIG_FLEET_ONLINE_CUTOFF_S);

    fleet_sim_t sim;
    fleet_get_sim(&sim);
    int64_t poll_age = sim.have_sim ? (now - sim.updated_at) : -1;

    // --- push into LVGL -----------------------------------------------------
    if (!bsp_display_lock(50)) return;   // skip a frame rather than block MQTT

    ui_set_broker_connected(broker);
    ui_set_gateway_online(gw);
    ui_set_messages_per_sec(mps);
    ui_set_last_error(err);

    ui_set_online_count(online, n);
    ui_set_sensor_row_count(n);
    ui_set_power_row_count(n);
    for (int i = 0; i < n; i++) {
        const fleet_sensor_t *s = &sensors[i];
        const char *model = fleet_model_name(s->model);
        int64_t age = s->have_seen ? (now - s->last_seen) : -1;
        bool is_online = s->have_seen && age >= 0 && age < CONFIG_FLEET_ONLINE_CUTOFF_S;
        ui_set_sensor_row(i, s->serial_str, model, age, is_online);
        ui_set_power_row(i, s->serial_str, model,
                         s->have_batt, s->batt_pct,
                         s->have_rssi, s->rssi_dbm);
    }

    ui_set_sim(sim.have_sim, sim.used_mb, sim.limit_mb, sim.pct,
               sim.paused, sim.last_connect_age_s, poll_age);

    bsp_display_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "SenseCAP Indicator D1L — fleet-health monitor starting");

    // 1. NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Shared state + display/LVGL + UI.
    fleet_state_init();
    if (bsp_display_start() != 0) {
        ESP_LOGE(TAG, "display bring-up failed — halting");
        return;
    }
    bsp_display_lock(-1);
    ui_create();
    bsp_display_unlock();

    // 3. Wi-Fi (blocks until first connect or retries exhausted).
    if (!wifi_sta_start()) {
        ESP_LOGW(TAG, "Wi-Fi not connected — UI stays on 'broker down', will keep retrying");
        // Continue: MQTT auto-reconnect + Wi-Fi auto-reconnect keep trying.
    }

    // 4. SNTP for absolute time (non-blocking).
    time_sync_start(CONFIG_FLEET_SNTP_SERVER);

    // 5. MQTT.
    mqtt_app_start();

    // 6. 1 Hz refresh timer that pushes state into LVGL.
    const esp_timer_create_args_t targs = {
        .callback = refresh_cb, .name = "refresh",
    };
    esp_timer_handle_t refresh;
    ESP_ERROR_CHECK(esp_timer_create(&targs, &refresh));
    ESP_ERROR_CHECK(esp_timer_start_periodic(refresh, 1000 * 1000));  // 1 s

    ESP_LOGI(TAG, "bring-up complete");
}
