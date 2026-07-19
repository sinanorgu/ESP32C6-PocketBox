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
            uint32_t character;
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
    EventQueue() {
        mutex = xSemaphoreCreateMutex();
    }
    bool push(Event &event);
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
    SemaphoreHandle_t mutex; 
    Event buffer_[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};



template <size_t Capacity>
bool EventQueue<Capacity>::push(Event &event)
{
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (isFull()) {
        xSemaphoreGive(mutex);
        return false;
    }

    buffer_[tail_] = event;
    tail_ = nextIndex(tail_);
    count_++;
    xSemaphoreGive(mutex);

    return true;
}

template <size_t Capacity>
bool EventQueue<Capacity>::pop(Event &event)
{
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (isEmpty()) {
        xSemaphoreGive(mutex);
        return false;
    }

    event = buffer_[head_];
    head_ = nextIndex(head_);
    count_--;
    xSemaphoreGive(mutex);
    return true;
}

template <size_t Capacity>
bool EventQueue<Capacity>::peek(Event &event) const
{
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (isEmpty()) {
        xSemaphoreGive(mutex);
        return false;
    }

    event = buffer_[head_];
    xSemaphoreGive(mutex);
    return true;
}

template <size_t Capacity>
bool EventQueue<Capacity>::isEmpty() const
{

    bool result = count_ == 0;
    return result;
}

template <size_t Capacity>
bool EventQueue<Capacity>::isFull() const
{

    bool result = count_ == Capacity;
    return result;
}

template <size_t Capacity>
size_t EventQueue<Capacity>::size() const
{
    
    size_t result = count_;
    return result;
}

template <size_t Capacity>
constexpr size_t EventQueue<Capacity>::capacity() const
{
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    size_t result = Capacity;
    xSemaphoreGive(mutex);
    return result;
}

template <size_t Capacity>
void EventQueue<Capacity>::clear()
{
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    head_ = 0;
    tail_ = 0;
    count_ = 0;
    xSemaphoreGive(mutex);
}

template <size_t Capacity>
constexpr size_t EventQueue<Capacity>::nextIndex(size_t index)
{
    
    size_t result = (index + 1) % Capacity;
    return result;
}
