#pragma once
#include <cstdint>

namespace App
{
    struct ManagerTaskConfig
    {
        uint32_t hold_target_ms;
        uint8_t tolerance_deg;
        uint16_t slow_delay_ms;
        uint16_t fast_delay_ms;
        uint8_t step_deg;
        const char *name;
    };

    class TaskManager
    {
    public:
        static void run(void *pvParameters);
    };

    void app_tasks_create();
}
