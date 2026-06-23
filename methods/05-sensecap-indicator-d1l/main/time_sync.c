// time_sync.c — SNTP bring-up + monotonic fallback clock.
#include "time_sync.h"

#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"

static const char *TAG = "time_sync";
static volatile bool s_synced = false;

static void on_sntp_sync(struct timeval *tv)
{
    (void)tv;
    s_synced = true;
    ESP_LOGI(TAG, "SNTP time synchronized");
}

void time_sync_start(const char *sntp_server)
{
    if (esp_sntp_enabled()) {
        return;
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, sntp_server ? sntp_server : "pool.ntp.org");
    sntp_set_time_sync_notification_cb(on_sntp_sync);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started (server=%s)", sntp_server ? sntp_server : "pool.ntp.org");
}

bool time_sync_is_synced(void)
{
    // Also consider the clock synced if it has been pushed past a sane epoch
    // floor (year 2022) by any means.
    if (s_synced) return true;
    time_t now = time(NULL);
    return now > 1640995200; // 2022-01-01 UTC
}

int64_t fleet_now(void)
{
    if (time_sync_is_synced()) {
        return (int64_t)time(NULL);              // absolute UTC epoch seconds
    }
    return (int64_t)(esp_timer_get_time() / 1000000); // boot-relative seconds
}
