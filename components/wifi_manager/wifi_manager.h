#ifndef WIFI_MANAGER_H_
#define WIFI_MANAGER_H_

#include "shearch_component.h"
#include "control.h"
#include "dns_responder.h"
#include "captive_portal.h"
#include "storage_manager.h"
#include "mqtt.h"

#include "esp_system.h"
#include "esp_event.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"

#include "freertos/event_groups.h"
#include "freertos/queue.h"


#define AP_SSID             "ESP32_Config"
#define AP_PASS             "12345678"
#define AP_CHANNEL          1
#define AP_MAX_CONN         4

typedef enum {
    AP_MODE = 0,
    STA_MODE = 1
}WiFiModeState_t;

typedef struct {
    char ssid[64];
    char pass[64];
} wifi_credentials_t;

void wifi_init(void);
void vTaskStartStaWifiConnect(void *pvParameter);
void change_wifi_mode(WiFiModeState_t wifi_mode, wifi_credentials_t *creds_opt);
void launch_wifi_saved_mode(void);


#endif /* WIFI_MANAGER_H_ */
