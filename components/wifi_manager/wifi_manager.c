#include "wifi_manager.h"

static const char *TAG = "WIFI_MANAGER";

static esp_netif_t *sta_netif = NULL;

static esp_event_handler_instance_t wifi_event_instance;
static esp_event_handler_instance_t ip_event_instance;

typedef enum
{
    WIFI_MODE_NO_INIT = 0,
    WIFI_MODE_AP_ON,
    WIFI_MODE_STA_ON,
} wifi_state_t;

static wifi_state_t wifi_state = WIFI_MODE_NO_INIT;

static EventGroupHandle_t wifi_event_group;
const int IP_GOT_IP_BIT = BIT0;

static QueueHandle_t wifi_creds_queue = NULL;

static void create_sync_primitives(void)
{
    wifi_event_group = xEventGroupCreate();
    wifi_creds_queue = xQueueCreate(1, sizeof(wifi_credentials_t));
    if (wifi_event_group == NULL || wifi_creds_queue == NULL) 
    {
        ESP_LOGE(TAG, "Failed to create FreeRTOS sync primitives!");
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_AP_STACONNECTED)
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            ESP_LOGI(TAG, "STA_MODE disconnected. Attempting reconnect...");
            change_blink_time(1000);
            esp_wifi_connect();
        }
    }

    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "STA_MODE successfully got IP from router: " IPSTR, IP2STR(&event->ip_info.ip));

            if (wifi_event_group)
            {
                xEventGroupSetBits(wifi_event_group, IP_GOT_IP_BIT);
            }
        }
    }
}

static void register_wifi_handlers(void)
{
    if (wifi_event_instance || ip_event_instance)
    {
        ESP_LOGW(TAG, "Wi-Fi handlers already registered");
        return;
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_event_instance));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &ip_event_instance));
}

static void stop_previos_wifi_mode(void)
{
    captive_portal_stop();
    dns_responder_stop();

    if (wifi_state == WIFI_MODE_AP_ON || wifi_state == WIFI_MODE_STA_ON)
    {
        esp_wifi_stop();
    }

    if (sta_netif)
    {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }
    esp_netif_t *old_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (old_ap)
    {
        esp_netif_destroy(old_ap);
        old_ap = NULL;
    }
    wifi_state = WIFI_MODE_NO_INIT;
}

static bool wait_for_ip_address(uint32_t timeout_ms)
{
    if (!wifi_event_group)
    {
        ESP_LOGE(TAG, "Wi-Fi event group not created");
        return false;
    }
    EventBits_t uxBits = xEventGroupWaitBits(
        wifi_event_group,
        IP_GOT_IP_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if ((uxBits & IP_GOT_IP_BIT) != 0)
    {
        ESP_LOGI(TAG, "Got IP address within timeout");
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Timeout waiting for IP address");
        return false;
    }
}

static void save_wifi_credentials(const char *ssid, const char *pass)
{
    storage_set_str("ssid", ssid);
    storage_set_str("pass", pass);
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    register_wifi_handlers(); 
    create_sync_primitives();
}

void start_ap_wifi_mode(void)
{
    stop_previos_wifi_mode();
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = 0,
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = 0}};
    if (strlen(AP_PASS) == 0)
        ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    wifi_state = WIFI_MODE_AP_ON;
}

void vTaskStartStaWifiConnect(void *pvParameter)
{
    wifi_credentials_t wifi_credentials;
    while (1)
    {
        if (xQueueReceive(wifi_creds_queue, &wifi_credentials, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Received credentials from queue: SSID: '%s'", wifi_credentials.ssid);

            stop_previos_wifi_mode();
            sta_netif = esp_netif_create_default_wifi_sta();

            ESP_LOGI(TAG, "STA_MODE wifi_mode will use DHCP (waiting for IP_EVENT_STA_GOT_IP)");

            wifi_config_t sta_cfg;
            memset(&sta_cfg, 0, sizeof(sta_cfg));
            strncpy((char *)sta_cfg.sta.ssid, wifi_credentials.ssid, sizeof(sta_cfg.sta.ssid) - 1);
            strncpy((char *)sta_cfg.sta.password, wifi_credentials.pass, sizeof(sta_cfg.sta.password) - 1);

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
            ESP_ERROR_CHECK(esp_wifi_start());
            ESP_ERROR_CHECK(esp_wifi_connect());

            bool ip_ok = wait_for_ip_address(20000);

            if (ip_ok)
            {
                captive_portal_stop();
                dns_responder_stop();
                change_blink_time(portMAX_DELAY);
                mqtt_app_start();

                save_wifi_credentials(wifi_credentials.ssid, wifi_credentials.pass);
                wifi_state = WIFI_MODE_STA_ON;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to connect to SSID: '%s'", wifi_credentials.ssid);
                start_ap_wifi_mode();
                ESP_ERROR_CHECK(esp_wifi_start());
                dns_responder_start();
                captive_portal_start();
                change_blink_time(400);
                mqtt_app_stop();
            }
        }
    }
}

void change_wifi_mode(WiFiModeState_t wifi_mode, wifi_credentials_t *creds_opt)
{
    switch (wifi_mode)
    {
    case AP_MODE:
        if (wifi_state != WIFI_MODE_AP_ON) 
        {
            start_ap_wifi_mode(); 
            ESP_ERROR_CHECK(esp_wifi_start());
            dns_responder_start();
            captive_portal_start();
            change_blink_time(400);
            mqtt_app_stop();
        }
        break;
    case STA_MODE:
        if (wifi_state != WIFI_MODE_STA_ON) 
        {
            if (xQueueSend(wifi_creds_queue, creds_opt, 0) != pdPASS) 
            {
                ESP_LOGE(TAG, "Failed to send credentials to queue.");
            }
        }
        break;
    }
}

static WiFiModeState_t storage_has_credentials(wifi_credentials_t *creds_opt)
{
    esp_err_t ssid_err = storage_get_str("ssid", creds_opt->ssid, sizeof(creds_opt->ssid));
    esp_err_t pass_err = storage_get_str("pass", creds_opt->pass, sizeof(creds_opt->pass));
    if (ssid_err == ESP_OK && pass_err == ESP_OK && strlen(creds_opt->ssid) > 0 && strlen(creds_opt->pass) > 0) 
    {
        return STA_MODE;
    }
    return AP_MODE;
}

void launch_wifi_saved_mode(void)
{
    wifi_credentials_t wifi_credentials;
    WiFiModeState_t wifi_mode = storage_has_credentials(&wifi_credentials);
    if (wifi_mode == AP_MODE)
        change_wifi_mode(wifi_mode, NULL);
    else if (wifi_mode == STA_MODE)
    {
        change_wifi_mode(wifi_mode, &wifi_credentials);
    }
}
