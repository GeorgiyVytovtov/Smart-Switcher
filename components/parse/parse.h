#ifndef PARSE_H_
#define PARSE_H_


#include <stdio.h>
#include <stdint.h>
#include "shearch_component.h"

esp_err_t parse_mqtt_state_json(const char *json_data, uint8_t *out_state);
esp_err_t build_mqtt_state_json(char *json_buf, size_t buf_size, uint8_t state);


#endif /* PARSE_H_ */