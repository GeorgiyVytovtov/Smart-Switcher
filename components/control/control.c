#include "control.h"

static const char *TAG = "CONTROL";

typedef enum
{
    BUTTON_STATE_NONE = 0,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_HOLD
} ButtonState_t;

typedef struct {
    ButtonState_t prev;
    ButtonState_t curr;

    uint8_t debounced_state;
    uint8_t raw_state;
    TickType_t last_time_change;

    uint16_t hold_ticks;
} button_t;


static uint8_t led_state = 0;
static uint8_t button_bitmask[COUNT_BUTTONS];
static button_t buttons[COUNT_BUTTONS];

static portMUX_TYPE led_spinlock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE blink_spinlock = portMUX_INITIALIZER_UNLOCKED;

static volatile TickType_t blink_time = 0;


static void mask_init(void)
{
    for (uint8_t i = 0; i < COUNT_BUTTONS; i++)
    {
        button_bitmask[i] = (1 << i);
    }
}

static void button_init(void)
{
    for (uint8_t i = 0; i < COUNT_BUTTONS; i++)
    {
        buttons[i].prev = BUTTON_STATE_NONE;
        buttons[i].curr = BUTTON_STATE_NONE;
        buttons[i].debounced_state = 1; 
        buttons[i].raw_state = 1;    
        buttons[i].last_time_change = 0;
        buttons[i].hold_ticks = 0;
    }
}

void gpio_init(void)
{
    uint64_t output_mask = (1ULL << INDICATE_STATE_LED);
    for(uint8_t i = 0; i < COUNT_BUTTONS; i++)
    {
        output_mask |= (1ULL << led_gpio_pins[i]);
    }
    gpio_config_t io_config = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&io_config);

    gpio_set_level(INDICATE_STATE_LED, false);
    for(uint8_t i = 0; i < COUNT_BUTTONS; i++)
    {
        gpio_set_level(led_gpio_pins[i], false);
    }

    uint64_t input_mask = 0;
    for (uint8_t i = 0; i < COUNT_BUTTONS; i++) {
        input_mask |= (1ULL << button_gpio_pins[i]);
    }
    
    io_config.pin_bit_mask = input_mask;

    io_config.mode = GPIO_MODE_INPUT;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_config);

    mask_init();
    button_init();
}

void switch_led_state(const uint8_t command)
{
    taskENTER_CRITICAL(&led_spinlock);

    led_state = command;
    for (uint8_t i = 0; i < COUNT_BUTTONS; i++)
    {
        gpio_set_level(led_gpio_pins[i], (command & button_bitmask[i]) ? true : false);
    }

    taskEXIT_CRITICAL(&led_spinlock);
}


static bool button_read_debounced(uint8_t index)
{
    bool raw_gpio_level = gpio_get_level(button_gpio_pins[index]);
    
    if(buttons[index].raw_state != raw_gpio_level)
    {
        buttons[index].raw_state = raw_gpio_level;
        buttons[index].last_time_change = xTaskGetTickCount();
    }

    if(buttons[index].raw_state != buttons[index].debounced_state && 
        (xTaskGetTickCount() - buttons[index].last_time_change) > pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS))
    {
        buttons[index].debounced_state = buttons[index].raw_state;
    }
    return buttons[index].debounced_state;
}

static void button_update_state(uint8_t index)
{
    uint8_t state = button_read_debounced(index) << index;
    buttons[index].curr = (state & button_bitmask[index]) ? BUTTON_STATE_NONE : BUTTON_STATE_PRESSED;
}

static void button_handle_click(uint8_t i)
{
    taskENTER_CRITICAL(&led_spinlock);
    led_state ^= button_bitmask[i];
    taskEXIT_CRITICAL(&led_spinlock);
    buttons[i].prev = BUTTON_STATE_PRESSED;
    switch_led_state(led_state);
    mqtt_send_to_publish(led_state);
    ESP_LOGI(TAG, "Button %d clicked", i);
}

static void handle_reset_hold(uint8_t i)
{
    buttons[i].hold_ticks++;

    if (buttons[i].hold_ticks >= RESET_HOLD_TICKS)
    {
        ESP_LOGI(TAG, "Reset button held 10s => AP mode");
        change_wifi_mode(AP_MODE, NULL);
        buttons[i].hold_ticks = 0;
        buttons[i].prev = BUTTON_STATE_HOLD;
    }
}

void vTaskButtonScan(void *pvParameter)
{
    while (1)
    {
        for (uint8_t i = 0; i < COUNT_BUTTONS; i++)
        {
            button_update_state(i);

            if (buttons[i].curr == BUTTON_STATE_PRESSED && buttons[i].prev == BUTTON_STATE_NONE)
            {
                button_handle_click(i);
            }
            else if (buttons[i].curr == BUTTON_STATE_NONE && (buttons[i].prev == BUTTON_STATE_PRESSED || buttons[i].prev == BUTTON_STATE_HOLD))
            {
                buttons[i].prev = BUTTON_STATE_NONE;
                if (i == RESET_MODE_BUTTON)
                {
                    buttons[i].hold_ticks = 0;
                }
            }
            else if (i == RESET_MODE_BUTTON && buttons[RESET_MODE_BUTTON].curr == BUTTON_STATE_PRESSED && buttons[RESET_MODE_BUTTON].prev == BUTTON_STATE_PRESSED)
            {
                handle_reset_hold(i);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BUTTON_SCAN_PERIOD_MS));
    }
}

void change_blink_time(TickType_t new_time_ms)
{
    taskENTER_CRITICAL(&blink_spinlock);
    blink_time = new_time_ms;
    taskEXIT_CRITICAL(&blink_spinlock);
}

void vTaskIndicateState(void *pvParameter)
{
    TickType_t local_blink = 0;

    while (1)
    {
        taskENTER_CRITICAL(&blink_spinlock);
        local_blink = blink_time;
        taskEXIT_CRITICAL(&blink_spinlock);

        if (local_blink == portMAX_DELAY)
        {
            gpio_set_level(INDICATE_STATE_LED, true);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else if (local_blink == 0)
        {
            gpio_set_level(INDICATE_STATE_LED, false);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            gpio_set_level(INDICATE_STATE_LED, true);
            vTaskDelay(pdMS_TO_TICKS(local_blink));
            gpio_set_level(INDICATE_STATE_LED, false);
            vTaskDelay(pdMS_TO_TICKS(local_blink));
        }
    }
}