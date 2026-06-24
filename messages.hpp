#pragma once
#include <cstdint>

namespace App
{
    enum class ButtonEventType
    {
        Start,
        SpeedState
    };

    enum class ServoStatusType
    {
        Moving,
        Reached
    };

    struct ButtonMsg
    {
        ButtonEventType type;
        bool pressed;
        uint32_t tick;
    };

    struct SensorMsg
    {
        uint16_t raw;
        uint16_t filtered;
        uint8_t target_angle;
        uint32_t tick;
    };

    struct ServoCmd
    {
        uint8_t target_angle;
        uint8_t tolerance_deg;
        uint16_t step_delay_ms;
        uint8_t step_deg;
    };

    struct ServoStatusMsg
    {
        ServoStatusType status;
        uint8_t current_angle;
        uint8_t target_angle;
        uint32_t tick;
    };
}
