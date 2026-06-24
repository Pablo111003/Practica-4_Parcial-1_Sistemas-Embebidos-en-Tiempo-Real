#pragma once
#include <cstdint>
#include "messages.hpp"

namespace App
{
    struct ButtonTaskConfig
    {
        uint8_t gpio;
        const char *name;
        ButtonEventType event_type;
        uint32_t poll_ms;
        bool press_edge_only; // true = solo notifica al presionar (Start); false = notifica presionar y soltar (Velocidad)
    };

    class ButtonTask
    {
        public:
            static void run(void *pvParameters);
    };
}
