#pragma once
#include <cstdint>
#include "driver/ledc.h"

namespace App
{
    struct ServoTaskConfig
    {
        uint8_t gpio;
        ledc_channel_t channel;
        ledc_timer_t timer;
        ledc_mode_t mode;
        ledc_timer_bit_t resolution;
        uint32_t freq_hz;
        uint16_t min_us;
        uint16_t max_us;
        const char *name;
    };

    class ServoTask
    {
    public:
        static void run(void *pvParameters);
    };
}
