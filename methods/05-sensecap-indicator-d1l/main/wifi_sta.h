// wifi_sta.h — minimal Wi-Fi station bring-up for the Indicator.
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Connects in STA mode using the SSID/PSK from menuconfig (Kconfig.projbuild).
// Blocks until the first successful connection or until max retries are
// exhausted; returns true on connect. Auto-reconnect continues in the
// background after the first connect.
bool wifi_sta_start(void);

// True once an IP has been obtained.
bool wifi_sta_is_connected(void);

#ifdef __cplusplus
}
#endif
