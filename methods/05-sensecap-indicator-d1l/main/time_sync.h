// time_sync.h — SNTP time sync with a boot-relative fallback.
//
// fleet_now() returns "seconds" for use as a timestamp. When SNTP is synced it
// returns absolute UTC epoch seconds; before that it returns seconds since boot.
// Because last_seen values are stamped with the same clock that reads them, the
// (now - last_seen) age stays correct across the SNTP transition for any sensor
// heard after sync — only ages computed across the sync boundary are skewed by
// the offset, which the UI tolerates (ages are advisory).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start the SNTP client (non-blocking). Safe to call after Wi-Fi is up.
void time_sync_start(const char *sntp_server);

// True once SNTP has set the system clock at least once.
bool time_sync_is_synced(void);

// Current time in seconds: absolute UTC epoch if synced, else boot-relative.
int64_t fleet_now(void);

#ifdef __cplusplus
}
#endif
