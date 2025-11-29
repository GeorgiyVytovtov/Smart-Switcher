#ifndef MQTT_H_
#define MQTT_H_

#include "esp_log.h"
#include "mqtt_client.h"

#define MQTT_DATA_MAX_LEN   256
#define MQTT_BROKER_URI     "mqtt://192.168.0.102:1883"
#define MQTT_TOPIC_SUB      "home/rooms/living/lights/id1/cmd"
#define MQTT_TOPIC_PUB      "home/rooms/living/lights/id1/state"

typedef struct {
    char data[MQTT_DATA_MAX_LEN]; 
    uint16_t data_len;
} MqttData;

void mqtt_app_start(void);
void mqtt_app_stop(void);
void mqtt_send_to_publish(uint8_t command);

void vTaskMqttPublish(void *pvParameter);
void vTaskParseFromMqtt(void* pvParameter);

#endif /* MQTT_H_ */