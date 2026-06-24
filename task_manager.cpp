#include "task_manager.hpp"
#include "app_config.hpp"
#include "app_context.hpp"
#include "messages.hpp"
#include "sensor_task.hpp"
#include "button_task.hpp"
#include "servo_task.hpp"
#include "ready_led_task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

namespace App
{
    static const char *TAG = "MANAGER";

    static SensorTaskConfig sensor_cfg
    {
        .unit_id       = AppConfig::LDR_ADC_UNIT,
        .channel       = AppConfig::LDR_ADC_CHANNEL,
        .period_ms     = AppConfig::SENSOR_PERIOD_MS,
        .filter_window = AppConfig::FILTER_WINDOW_SIZE,
        .name          = "SensorTask"
    };

    static ServoTaskConfig servo_cfg
    {
        .gpio       = AppConfig::SERVO_GPIO,
        .channel    = AppConfig::SERVO_PWM_CHANNEL,
        .timer      = AppConfig::SERVO_PWM_TIMER,
        .mode       = AppConfig::SERVO_PWM_MODE,
        .resolution = AppConfig::SERVO_PWM_RES_BITS,
        .freq_hz    = AppConfig::SERVO_PWM_FREQ_HZ,
        .min_us     = static_cast<uint16_t>(AppConfig::SERVO_MIN_US),
        .max_us     = static_cast<uint16_t>(AppConfig::SERVO_MAX_US),
        .name       = "ServoTask"
    };

    static ButtonTaskConfig start_btn_cfg
    {
        .gpio            = AppConfig::START_BUTTON_GPIO,
        .name            = "BtnStart",
        .event_type      = ButtonEventType::Start,
        .poll_ms         = AppConfig::BUTTON_POLL_MS,
        .press_edge_only = true
    };

    static ButtonTaskConfig speed_btn_cfg
    {
        .gpio            = AppConfig::SPEED_BUTTON_GPIO,
        .name            = "BtnSpeed",
        .event_type      = ButtonEventType::SpeedState,
        .poll_ms         = AppConfig::BUTTON_POLL_MS,
        .press_edge_only = false
    };

    static ReadyLedTaskConfig ready_led_cfg
    {
        .gpio    = AppConfig::READY_LED_GPIO,
        .on_ms   = AppConfig::READY_LED_ON_MS,
        .off_ms  = AppConfig::READY_LED_OFF_MS,
        .name    = "LedReady"
    };

    static ManagerTaskConfig manager_cfg
    {
        .hold_target_ms = AppConfig::HOLD_TARGET_MS,
        .tolerance_deg  = static_cast<uint8_t>(AppConfig::SERVO_TOLERANCE_DEG),
        .slow_delay_ms  = static_cast<uint16_t>(AppConfig::SERVO_DELAY_SLOW_MS),
        .fast_delay_ms  = static_cast<uint16_t>(AppConfig::SERVO_DELAY_FAST_MS),
        .step_deg       = static_cast<uint8_t>(AppConfig::SERVO_STEP_DEG),
        .name           = "TaskManager"
    };

    static const char *state_text(eTaskState st)
    {
        switch (st)
        {
            case eRunning:   return "RUNNING";
            case eReady:     return "READY";
            case eBlocked:   return "BLOCKED";
            case eSuspended: return "SUSPENDED";
            case eDeleted:   return "DELETED";
            default:         return "UNKNOWN";
        }
    }

    static void print_states()
    {
        ESP_LOGI(TAG, "STATES sensor=%s servo=%s start=%s speed=%s ready=%s",
                 state_text(eTaskGetState(g_handles.sensor)),
                 state_text(eTaskGetState(g_handles.servo)),
                 state_text(eTaskGetState(g_handles.start_button)),
                 state_text(eTaskGetState(g_handles.speed_button)),
                 state_text(eTaskGetState(g_handles.ready_led)));
    }

    static void send_servo_cmd(uint8_t target, bool fast, const ManagerTaskConfig *cfg)
    {
        ServoCmd cmd
        {
            target,
            cfg->tolerance_deg,
            fast ? cfg->fast_delay_ms : cfg->slow_delay_ms,
            cfg->step_deg
        };
        xQueueOverwrite(g_queues.servo_cmd, &cmd);
    }

    enum class SysState { IDLE, OPERATING, HOLDING };

    // Suspende el sensor, servo, y el boton de velocidad, y reanuda el boton start y led
    static void enter_idle()
    {
        gpio_set_level(static_cast<gpio_num_t>(AppConfig::READY_LED_GPIO), 0);
        vTaskSuspend(g_handles.sensor);
        vTaskSuspend(g_handles.servo);
        vTaskSuspend(g_handles.speed_button);
        vTaskResume(g_handles.start_button);
        vTaskResume(g_handles.ready_led);
    }

    // Vacia las colas para no procesar datos o estados anteriores
    static uint8_t enter_operating(const ManagerTaskConfig *cfg, bool fast)
    {
        vTaskSuspend(g_handles.start_button);
        vTaskSuspend(g_handles.ready_led);
        gpio_set_level(static_cast<gpio_num_t>(AppConfig::READY_LED_GPIO), 0);

        SensorMsg stale_sensor;
        xQueueReceive(g_queues.sensor, &stale_sensor, 0);
        ServoStatusMsg stale_status;
        xQueueReceive(g_queues.servo_status, &stale_status, 0);

        vTaskResume(g_handles.sensor);
        // Se notifica al sensor que haga un reset de histeresis
        xTaskNotify(g_handles.sensor, 1, eSetValueWithOverwrite);
        vTaskResume(g_handles.speed_button);

        // esperar primera lectura fresca del sensor
        SensorMsg first_reading;
        xQueueReceive(g_queues.sensor, &first_reading, portMAX_DELAY);

        vTaskResume(g_handles.servo);
        send_servo_cmd(first_reading.target_angle, fast, cfg);

        return first_reading.target_angle;
    }

    void TaskManager::run(void *pvParameters)
    {
        auto *cfg = static_cast<ManagerTaskConfig *>(pvParameters);

        SysState   state        = SysState::IDLE;
        bool       fast_mode    = false;
        uint8_t    cur_target   = AppConfig::SERVO_ANGLE_DARK;
        TickType_t reached_tick = 0;

        vTaskSuspend(g_handles.sensor);
        vTaskSuspend(g_handles.servo);
        vTaskSuspend(g_handles.speed_button);
        vTaskResume(g_handles.ready_led);
        vTaskResume(g_handles.start_button);

        TickType_t last_log_tick = xTaskGetTickCount();

        while (true)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_log_tick) >= pdMS_TO_TICKS(AppConfig::LOG_PERIOD_MS))
            {
                print_states();
                last_log_tick = now;
            }

            ButtonMsg btn_msg;
            while (xQueueReceive(g_queues.buttons, &btn_msg, 0) == pdTRUE)
            {
                if (btn_msg.type == ButtonEventType::Start && state == SysState::IDLE)
                {
                    state      = SysState::OPERATING;
                    cur_target = enter_operating(cfg, fast_mode);
                }
                else if (btn_msg.type == ButtonEventType::SpeedState && state == SysState::OPERATING)
                {
                    fast_mode = btn_msg.pressed;
                    send_servo_cmd(cur_target, fast_mode, cfg);
                }
            }

            if (state == SysState::OPERATING)
            {
                SensorMsg sensor_msg;
                if (xQueueReceive(g_queues.sensor, &sensor_msg, 0) == pdTRUE)
                {
                    if (sensor_msg.target_angle != cur_target)
                    {
                        cur_target = sensor_msg.target_angle;
                        send_servo_cmd(cur_target, fast_mode, cfg);
                    }
                }

                ServoStatusMsg srv_status;
                if (xQueueReceive(g_queues.servo_status, &srv_status, 0) == pdTRUE)
                {
                    if (srv_status.status == ServoStatusType::Reached)
                    {
                        state        = SysState::HOLDING;
                        reached_tick = xTaskGetTickCount();
                        // suspender sensor y velocidad durante los 8 segundos
                        vTaskSuspend(g_handles.sensor);
                        vTaskSuspend(g_handles.speed_button);
                    }
                }
            }
            else if (state == SysState::HOLDING)
            {
                if ((xTaskGetTickCount() - reached_tick) >= pdMS_TO_TICKS(cfg->hold_target_ms))
                {
                    // reanudar para que enter_idle los suspenda limpiamente
                    vTaskResume(g_handles.sensor);
                    vTaskResume(g_handles.speed_button);
                    state = SysState::IDLE;
                    enter_idle();
                }
            }

            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void app_tasks_create()
    {
        g_queues.sensor       = xQueueCreate(1, sizeof(SensorMsg));
        g_queues.buttons      = xQueueCreate(AppConfig::BUTTON_QUEUE_LEN, sizeof(ButtonMsg));
        g_queues.servo_cmd    = xQueueCreate(1, sizeof(ServoCmd));
        g_queues.servo_status = xQueueCreate(1, sizeof(ServoStatusMsg));

        xTaskCreate(SensorTask::run,   sensor_cfg.name,    4096, &sensor_cfg,    3, &g_handles.sensor);
        xTaskCreate(ServoTask::run,    servo_cfg.name,     4096, &servo_cfg,     3, &g_handles.servo);
        xTaskCreate(ButtonTask::run,   start_btn_cfg.name, 2048, &start_btn_cfg, 4, &g_handles.start_button);
        xTaskCreate(ButtonTask::run,   speed_btn_cfg.name, 2048, &speed_btn_cfg, 4, &g_handles.speed_button);
        xTaskCreate(ReadyLedTask::run, ready_led_cfg.name, 2048, &ready_led_cfg, 1, &g_handles.ready_led);
        xTaskCreate(TaskManager::run,  manager_cfg.name,   4096, &manager_cfg,   5, &g_handles.manager);
    }
}