#pragma once
#include <cstdint>
#include "esp_adc/adc_oneshot.h"

namespace App
{
    struct SensorTaskConfig
    {
        adc_unit_t unit_id;
        adc_channel_t channel;
        uint32_t period_ms;
        uint8_t filter_window;
        const char *name;
    };

    class SensorTask
    {
    public:
        static void run(void *pvParameters);
    };
}
