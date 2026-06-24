#include "button_task.hpp"
#include "app_context.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

namespace App
{
    static const char *TAG = "BUTTON";

    struct Debounce // Guarda el estado interno del anti rebote para un boton
    {
        int last_raw{1};
        int stable{1};
        uint8_t count{0};
    };

    static constexpr uint8_t DEBOUNCE_CONFIRM_COUNT = 3;

    // Actualiza el estado anti rebote y detecta si el nivel estable cambio
    static bool debounce(uint8_t gpio, Debounce &db, bool &level_pressed)
    {
        int raw = gpio_get_level(static_cast<gpio_num_t>(gpio));
        bool changed = false;

        if (raw != db.last_raw)
        {
            db.last_raw = raw;
            db.count = 0;
        }
        else if (db.count < DEBOUNCE_CONFIRM_COUNT)
        {
            db.count++;
        }

        if (db.count >= DEBOUNCE_CONFIRM_COUNT && raw != db.stable)
        {
            db.stable = raw;
            changed = true;
        }

        level_pressed = (db.stable == 0); // activo-bajo (pull-up)
        return changed;
    }

    void ButtonTask::run(void *pvParameters)
    {
        auto *cfg = static_cast<ButtonTaskConfig *>(pvParameters);

        gpio_reset_pin(static_cast<gpio_num_t>(cfg->gpio));
        gpio_set_direction(static_cast<gpio_num_t>(cfg->gpio), GPIO_MODE_INPUT);
        gpio_set_pull_mode(static_cast<gpio_num_t>(cfg->gpio), GPIO_PULLUP_ONLY);

        Debounce db;
        bool pressed = false;

        ESP_LOGI(TAG, "%s iniciado", cfg->name);

        while (true)
        {
            bool changed = debounce(cfg->gpio, db, pressed);

            if (changed)
            {
                bool should_send = pressed || !cfg->press_edge_only; 
                                            // Notifica si se presiono el boton start
                                            // o si se presiona y mantiene presionado el boton velocidad

                if (should_send)
                {
                    ButtonMsg msg{ cfg->event_type, pressed, xTaskGetTickCount() };
                    xQueueSend(g_queues.buttons, &msg, 0);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(cfg->poll_ms));
        }
    }
}
