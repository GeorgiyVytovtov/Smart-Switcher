#ifndef CONTROL_H_
#define CONTROL_H_

#include "driver/gpio.h"
#include "shearch_component.h"
#include "mqtt.h"
#include "wifi_manager.h"

#define INDICATE_STATE_LED  GPIO_NUM_7  
#define RESET_MODE_BUTTON   GPIO_NUM_0 

static const gpio_num_t led_gpio_pins[COUNT_BUTTONS] = {
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10
};

static const gpio_num_t button_gpio_pins[COUNT_BUTTONS] = {
    GPIO_NUM_0,
    GPIO_NUM_1,
    GPIO_NUM_2
};

#define BUTTON_DEBOUNCE_MS          30
#define BUTTON_SCAN_PERIOD_MS       70
#define RESET_HOLD_TICKS            100  // ~10s

void gpio_init(void);
void switch_led_state(const uint8_t command);
void change_blink_time(TickType_t new_time_ms);
void vTaskButtonScan(void *pvParameter);
void vTaskIndicateState(void* pvParameter);


#endif /* CONTROL_H_ */