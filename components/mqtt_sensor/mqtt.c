#include "mqtt.h"
#include "parse.h"
#include "control.h"
#include "shearch_component.h"

static const char *TAG = "MQTT_SENSOR";

static esp_mqtt_client_handle_t client = NULL;

static QueueHandle_t xMqttSubQueue = NULL;
static QueueHandle_t xMqttPubQueue = NULL;

static volatile bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_SUB, 0);
        mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnecte");
        mqtt_connected = false;
        break;
    case MQTT_EVENT_DATA:
        MqttData mqtt_data = {0};

        printf("TOPIC = %.*s\r\n", event->topic_len, event->topic);
        printf("DATA = %.*s\r\n", event->data_len, event->data);

        mqtt_data.data_len = event->data_len < sizeof(mqtt_data.data) - 1 ? event->data_len :sizeof(mqtt_data.data) - 1;
        memcpy(mqtt_data.data, event->data, mqtt_data.data_len);
        mqtt_data.data[mqtt_data.data_len] = '\0'; 

        if (xMqttSubQueue)
            xQueueSend(xMqttSubQueue, &mqtt_data, pdMS_TO_TICKS(10));
        break;
    default:
        break;
    }
}

void mqtt_app_start(void)
{
    if(client != NULL)
    {
        ESP_LOGW(TAG, "MQTT client already started");
        return;
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void vTaskMqttPublish(void *pvParameter)
{
    MqttData mqtt_data;
    uint8_t command;
    char json_data[MQTT_DATA_MAX_LEN] = {0};

    xMqttPubQueue = xQueueCreate(10, sizeof(uint8_t));
    if (xMqttPubQueue == NULL) {
        ESP_LOGE(TAG, "xMqttPubQueue is NULL!");
        vTaskDelete(NULL);
    }
    while (1)
    {
        if(xQueueReceive(xMqttPubQueue, &command, portMAX_DELAY))
        {
            if(mqtt_connected)
            {
                if (build_mqtt_state_json(json_data, MQTT_DATA_MAX_LEN, command) == ESP_OK)
                {
                    strcpy(mqtt_data.data, json_data);
                    mqtt_data.data_len = strlen(json_data);
                    ESP_LOGI(TAG, "Received data from queue: %s", mqtt_data.data);
                    esp_mqtt_client_publish(client, MQTT_TOPIC_PUB, mqtt_data.data, mqtt_data.data_len, 1, 0);
                }
            }
        }
    }
}

void mqtt_app_stop()
{
    if(client !=NULL)
    {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
        mqtt_connected = false;
    }
}

void mqtt_send_to_publish(uint8_t command)
{
    if (xMqttPubQueue != NULL)
    {
        xQueueSend(xMqttPubQueue, &command, pdMS_TO_TICKS(100));
    }
    else
    {
        ESP_LOGE(TAG, "xMqttPubQueue is NULL! Cannot send command to publish.");
    }
}

void vTaskParseFromMqtt(void *pvParameter)
{
    MqttData mqtt_data;
    xMqttSubQueue = xQueueCreate(10, sizeof(MqttData));
    if (xMqttSubQueue == NULL) {
        ESP_LOGE(TAG, "xMqttSubQueue is NULL!");
        vTaskDelete(NULL);
    }
    uint8_t state = 0;
    while (1)
    {
        if (xQueueReceive(xMqttSubQueue, &mqtt_data, portMAX_DELAY))
        {
            if(parse_mqtt_state_json(mqtt_data.data, &state) == ESP_OK)
            {
                if (state < (1<<COUNT_BUTTONS))
                {
                    switch_led_state(state);
                }else
                {
                    ESP_LOGE(TAG, "Failed to parse state from MQTT data");
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to parse state from MQTT data");
                continue;
            }
        }
    }
}
