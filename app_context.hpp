#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace App
{
    struct Handles
    {
        TaskHandle_t sensor{};
        TaskHandle_t servo{};
        TaskHandle_t start_button{};
        TaskHandle_t speed_button{};
        TaskHandle_t ready_led{};
        TaskHandle_t manager{};
    };

    struct Queues
    {
        QueueHandle_t sensor{};
        QueueHandle_t buttons{};
        QueueHandle_t servo_cmd{};
        QueueHandle_t servo_status{};
    };

    extern Handles g_handles;
    extern Queues g_queues;
}
