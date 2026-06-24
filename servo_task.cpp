#include "servo_task.hpp"
#include "app_context.hpp"
#include "messages.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace App
{
    static const char *TAG = "SERVO";

    // Convierte el angulo a duty cycle
    static uint32_t angle_to_duty(const ServoTaskConfig *cfg, uint8_t angle)
    {
        uint32_t max_duty = (1UL << cfg->resolution) - 1UL;
        uint32_t pulse_us = cfg->min_us + ((uint32_t)angle * (cfg->max_us - cfg->min_us)) / 180UL;
        return (pulse_us * max_duty * cfg->freq_hz) / 1000000UL;
    }

    static void servo_write_angle(const ServoTaskConfig *cfg, uint8_t angle)
    {
        uint32_t duty = angle_to_duty(cfg, angle);
        ledc_set_duty(cfg->mode, cfg->channel, duty);
        ledc_update_duty(cfg->mode, cfg->channel);
    }

    static void report_status(ServoStatusType st, uint8_t cur, uint8_t tgt)
    {
        ServoStatusMsg status_msg{ st, cur, tgt, xTaskGetTickCount() };
        xQueueOverwrite(g_queues.servo_status, &status_msg);
    }

    void ServoTask::run(void *pvParameters)
    {
        auto *cfg = static_cast<ServoTaskConfig *>(pvParameters);

        ledc_timer_config_t timer_cfg = {};
        timer_cfg.speed_mode = cfg->mode;
        timer_cfg.duty_resolution = cfg->resolution;
        timer_cfg.timer_num = cfg->timer;
        timer_cfg.freq_hz = cfg->freq_hz;
        timer_cfg.clk_cfg = LEDC_USE_APB_CLK; 
        ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

        ledc_channel_config_t ch_cfg = {};
        ch_cfg.gpio_num = cfg->gpio;
        ch_cfg.speed_mode = cfg->mode;
        ch_cfg.channel = cfg->channel;
        ch_cfg.timer_sel = cfg->timer;
        ch_cfg.duty = 0;
        ch_cfg.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

        uint8_t current_angle = 0;
        servo_write_angle(cfg, current_angle);

        ESP_LOGI(TAG, "%s iniciado", cfg->name);

        while (true)
        {
            ServoCmd cmd;
            if (xQueueReceive(g_queues.servo_cmd, &cmd, portMAX_DELAY) != pdTRUE) continue;

            while (true)
            {
                int diff = (int)cmd.target_angle - (int)current_angle;

                if (diff >= -(int)cmd.tolerance_deg && diff <= (int)cmd.tolerance_deg)
                {
                    report_status(ServoStatusType::Reached, current_angle, cmd.target_angle);
                    break;
                }

                int step = (diff > 0) ? (int)cmd.step_deg : -(int)cmd.step_deg;
                int next = (int)current_angle + step;
                if (next < 0) next = 0;
                if (next > 180) next = 180;

                current_angle = static_cast<uint8_t>(next);
                servo_write_angle(cfg, current_angle);
                report_status(ServoStatusType::Moving, current_angle, cmd.target_angle);

                vTaskDelay(pdMS_TO_TICKS(cmd.step_delay_ms));

                // Lee la cola sin bloquear para poder capturar cambios de objetivo o de velocidad
                ServoCmd new_cmd;
                if (xQueueReceive(g_queues.servo_cmd, &new_cmd, 0) == pdTRUE)
                {
                    cmd = new_cmd; // actualiza objetivo y velocidad si TaskManager mando algo nuevo
                }
            }
        }
    }
}
