#include "parse.h"
#include "cJSON.h"
#include <string.h>

#define JSON_KEY_STATES     "states"

static const char *TAG = "PARSE";

esp_err_t parse_mqtt_state_json(const char *json_data, uint8_t *out_state)
{
    if (!json_data || !out_state)
        return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_Parse(json_data);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON data");
        return ESP_FAIL;
    }

    cJSON *arr = cJSON_GetObjectItem(root, JSON_KEY_STATES);
    if (!cJSON_IsArray(arr))
    {
        ESP_LOGE(TAG, "'states' is missing or not an array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    uint32_t arr_size = cJSON_GetArraySize(arr);
    uint8_t state = 0;

    for (uint8_t i = 0; i < arr_size && i < COUNT_BUTTONS; i++)
    {
        cJSON* curr_item = cJSON_GetArrayItem(arr, i);
        if(!curr_item)
        {
            ESP_LOGW(TAG, "Missing item in 'states' at index %d", i);
            continue;
        }
        if (cJSON_IsString(curr_item) && curr_item->valuestring)
        {
            if (!strcmp(curr_item->valuestring, "ON"))
            {
                state |= (1 << i);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Invalid item in 'states' at index %d", i);
        }
    }
    *out_state = state;
    cJSON_Delete(root);
    return ESP_OK; 
}

esp_err_t build_mqtt_state_json(char *json_buf, size_t buf_size, uint8_t state)
{
    if (!json_buf || buf_size == 0)
        return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL; 
    }
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    for (uint8_t i = 0; i < COUNT_BUTTONS; i++)
    {
        cJSON_AddItemToArray(arr, cJSON_CreateString((state & (1 << i)) ? "ON" : "OFF"));
    }
    
    cJSON_AddItemToObject(root, JSON_KEY_STATES, arr); 

    char *out = cJSON_PrintUnformatted(root);
    if (!out) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (strlen(out) >= buf_size) {
        ESP_LOGE(TAG, "JSON too long");
        cJSON_free(out);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    strcpy(json_buf, out);

    cJSON_free(out);
    cJSON_Delete(root);
    return ESP_OK;
}

