#pragma once

#include <Arduino.h>

enum class EventType : uint8_t
{
    None,

    // Button events
    ButtonDown,
    ButtonUp,
    ButtonRepeat,

    // Keyboard events
    KeyDown,
    KeyUp,
    TextInput,

    // Mouse events
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel,

    // System events
    AppOpened,
    AppClosed,
    BatteryLow,
    WiFiConnected,
    WiFiDisconnected,
    BluetoothConnected,
    BluetoothDisconnected,

    // User-defined event
    Custom
};

enum class ButtonCode : uint8_t
{
    None,
    Up,
    Down,
    Left,
    Right
};

struct Event
{
    EventType type = EventType::None;
    uint32_t timestamp = 0;
    union
    {
        struct
        {
            ButtonCode button;
        } button;

        struct
        {
            uint16_t keyCode;
            char character;
            bool ctrl;
            bool alt;
            bool shift;
        } keyboard;

        struct
        {
            int16_t x;
            int16_t y;
            int16_t deltaX;
            int16_t deltaY;
            int8_t wheel;
            uint8_t button;
        } mouse;

        struct
        {
            int32_t code;
            int32_t value;
        } custom;
    } event;
};


template <size_t Capacity>
class EventQueue
{
public:
    bool push(const Event &event);
    bool pop(Event &event);
    bool peek(Event &event) const;
    bool isEmpty() const;
    bool isFull() const;
    size_t size() const;
    constexpr size_t capacity() const;
    void clear();

private:
    static constexpr size_t nextIndex(size_t index);
private:
    Event buffer_[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};