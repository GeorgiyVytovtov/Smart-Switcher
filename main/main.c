#include "wifi_manager.h"
#include "mqtt.h"
#include "control.h"
#include "storage_manager.h"


void app_main(void)
{
    gpio_init();
    storage_init();

    wifi_init();
    launch_wifi_saved_mode();

    xTaskCreate(vTaskIndicateState, "vTaskIndicateState", 2048, NULL, 1, NULL);
    xTaskCreate(vTaskButtonScan, "vTaskButtonScan", 3072, NULL, 3, NULL);

    xTaskCreate(vTaskStartStaWifiConnect, "start_sta_wifi_connect_task", 4096, NULL, 5, NULL);

    xTaskCreate(vTaskParseFromMqtt, "vTaskParseFromMqtt", 4096, NULL, 6, NULL);
    xTaskCreate(vTaskMqttPublish, "vTaskMqttPublish", 4096, NULL, 6, NULL);
}
