#include "ready_led_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

namespace App
{
    void ReadyLedTask::run(void *pvParameters)
    {
        auto *cfg = static_cast<ReadyLedTaskConfig *>(pvParameters);

        gpio_reset_pin(static_cast<gpio_num_t>(cfg->gpio));
        gpio_set_direction(static_cast<gpio_num_t>(cfg->gpio), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(cfg->gpio), 0);

        while (true)
        {
            gpio_set_level(static_cast<gpio_num_t>(cfg->gpio), 1);
            vTaskDelay(pdMS_TO_TICKS(cfg->on_ms));

            gpio_set_level(static_cast<gpio_num_t>(cfg->gpio), 0);
            vTaskDelay(pdMS_TO_TICKS(cfg->off_ms));
        }
    }
}
