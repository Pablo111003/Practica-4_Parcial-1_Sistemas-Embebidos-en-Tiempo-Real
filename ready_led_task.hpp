#pragma once
#include <cstdint>

namespace App
{
    struct ReadyLedTaskConfig
    {
        uint8_t gpio;
        uint32_t on_ms;
        uint32_t off_ms;
        const char *name;
    };

    class ReadyLedTask
    {
    public:
        static void run(void *pvParameters);
    };
}
