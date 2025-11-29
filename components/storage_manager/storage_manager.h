#ifndef STORAGE_MANAGER_H_
#define STORAGE_MANAGER_H_


#include <stdio.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_err.h"


#define STORAGE_NAMESPACE "wifi_creds"

esp_err_t storage_init(void);
esp_err_t storage_set_str(const char *key, const char *value);
esp_err_t storage_get_str(const char *key, char *value, size_t length);
esp_err_t storage_erase_all(void);


#endif /* STORAGE_MANAGER_H_ */