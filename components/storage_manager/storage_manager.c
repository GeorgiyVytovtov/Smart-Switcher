#include "storage_manager.h"

static const char *TAG = "STORAGE_MANAGER";


esp_err_t storage_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    return err;
}


esp_err_t storage_set_str(const char *key, const char *value)
{
    nvs_handle_t handle;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_set_str(handle, key, value);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Credentials saved to NVS");
        return ESP_OK;
    }
    ESP_LOGW(TAG, "NVS open failed; creds not saved");
    return ESP_FAIL;
}


esp_err_t storage_get_str(const char *key, char *value, size_t length)
{
    // Read from NVS
    nvs_handle_t handle;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle) == ESP_OK)
    {
        esp_err_t err = nvs_get_str(handle, key, value, &length);
        nvs_close(handle);
        ESP_LOGI(TAG, "'%s': '%s'", key, value);
        if(err != ESP_OK)
        {
            ESP_LOGW(TAG, "NVS read failed; creds may be invalid");
            return err;
        }
        return ESP_OK;
    }
    ESP_LOGW(TAG, "NVS open failed; creds do not saved");
    return ESP_FAIL;
}


esp_err_t storage_erase_all()
{
    esp_err_t err = nvs_flash_erase();
    return err;
}
