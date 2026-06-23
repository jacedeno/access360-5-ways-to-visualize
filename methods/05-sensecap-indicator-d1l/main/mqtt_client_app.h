// mqtt_client_app.h — esp-mqtt setup + health-topic parsing.
//
// Connects to the HiveMQ broker (anonymous, no TLS, MQTT 5.0), subscribes to the
// gateway's health channels, parses each JSON payload with cJSON, and folds the
// result into the shared fleet_state table.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Starts the MQTT client using broker host/port from menuconfig. Non-blocking;
// connection state is reflected via fleet_set_broker_connected().
void mqtt_app_start(void);

#ifdef __cplusplus
}
#endif
