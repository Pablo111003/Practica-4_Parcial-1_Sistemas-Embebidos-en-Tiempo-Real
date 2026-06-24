#include "sensor_task.hpp"
#include "app_config.hpp"
#include "app_context.hpp"
#include "messages.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <climits>

namespace App
{
    static const char *TAG = "SENSOR";
    static constexpr uint8_t MAX_WINDOW = 32;

    static uint16_t median_u16(uint16_t *v, uint8_t n)
    {
        uint16_t sorted[MAX_WINDOW];
        uint8_t count = (n > MAX_WINDOW) ? MAX_WINDOW : n;

        for (uint8_t i = 0; i < count; i++) sorted[i] = v[i];

        for (uint8_t i = 1; i < count; i++)
        {
            uint16_t key = sorted[i];
            int8_t j = (int8_t)(i - 1);
            while (j >= 0 && sorted[j] > key)
            {
                sorted[j + 1] = sorted[j];
                j--;
            }
            sorted[j + 1] = key;
        }

        return sorted[count / 2];
    }

    // Determina el angulo objetivo segun la lectura filtrada del LDR
    static uint8_t target_from_ldr(uint16_t filtered, bool reset)
    {
        static uint8_t last_target = AppConfig::SERVO_ANGLE_DARK;

        if (reset)
        {
            constexpr uint16_t mid = (AppConfig::LDR_THRESHOLD_LOW + AppConfig::LDR_THRESHOLD_HIGH) / 2;
            last_target = (filtered >= mid)
                          ? static_cast<uint8_t>(AppConfig::SERVO_ANGLE_LIGHT)
                          : static_cast<uint8_t>(AppConfig::SERVO_ANGLE_DARK);
        }
        else if (filtered >= AppConfig::LDR_THRESHOLD_HIGH)
            last_target = AppConfig::SERVO_ANGLE_LIGHT;
        else if (filtered <= AppConfig::LDR_THRESHOLD_LOW)
            last_target = AppConfig::SERVO_ANGLE_DARK;

        return last_target;
    }

    void SensorTask::run(void *pvParameters)
    {
        auto *cfg = static_cast<SensorTaskConfig *>(pvParameters);

        uint8_t window = (cfg->filter_window == 0) ? 1
                        : (cfg->filter_window > MAX_WINDOW) ? MAX_WINDOW
                        : cfg->filter_window;

        adc_oneshot_unit_handle_t adc_handle;
        adc_oneshot_unit_init_cfg_t unit_cfg = {};
        unit_cfg.unit_id  = cfg->unit_id;
        unit_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

        adc_oneshot_chan_cfg_t chan_cfg = {};
        chan_cfg.atten    = ADC_ATTEN_DB_12;
        chan_cfg.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, cfg->channel, &chan_cfg));

        ESP_LOGI(TAG, "%s iniciado", cfg->name);

        uint16_t samples[MAX_WINDOW];

        while (true)
        {
            // TaskManager notifica al reanudar para resetear histeresis
            uint32_t notif = 0;
            xTaskNotifyWait(0, ULONG_MAX, &notif, 0);
            bool do_reset = (notif > 0);

            for (uint8_t i = 0; i < window; i++)
            {
                int raw_val = 0;
                ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, cfg->channel, &raw_val));
                samples[i] = static_cast<uint16_t>(raw_val);
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            uint16_t filtered = median_u16(samples, window);
            uint8_t  target   = target_from_ldr(filtered, do_reset);

            
            SensorMsg msg{ samples[window - 1], filtered, target, xTaskGetTickCount() };
            xQueueOverwrite(g_queues.sensor, &msg);

            TickType_t sample_time_ms = (TickType_t)window * 10;
            if (cfg->period_ms > sample_time_ms)
                vTaskDelay(pdMS_TO_TICKS(cfg->period_ms - sample_time_ms));
        }
    }
}