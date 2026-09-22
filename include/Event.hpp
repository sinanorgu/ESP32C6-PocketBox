#pragma once

#include <cstddef>
#include <cstdint>
#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <mutex>
#endif

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

    // Appended to preserve existing keyboard/system event values.
    ButtonClick,
    ButtonLongPress,
    ButtonChord,
    ButtonStateSync,

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
            uint8_t pressedMask; // Current combined state after this transition.
            uint8_t gestureMask; // All buttons participating in this press.
            uint8_t sourceMask; // Bitset of ButtonSource providers.
            uint32_t heldMs;
            bool repeated; // A click after repeats must not move a menu again.
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
    } event{};
};


constexpr uint8_t buttonMask(ButtonCode button) {
    return button >= ButtonCode::Up && button <= ButtonCode::Right
        ? uint8_t(1U << (uint8_t(button) - 1)) : 0;
}
constexpr uint8_t AllButtons = 0x0f;

inline bool isButtonEvent(const Event& e) {
    return e.type == EventType::ButtonDown || e.type == EventType::ButtonUp ||
           e.type == EventType::ButtonRepeat || e.type == EventType::ButtonClick ||
           e.type == EventType::ButtonLongPress || e.type == EventType::ButtonChord ||
           e.type == EventType::ButtonStateSync;
}

inline bool buttonAction(const Event& e, ButtonCode code, bool allowRepeat = false) {
    return ((e.type == EventType::ButtonClick && (!allowRepeat || !e.event.button.repeated)) ||
            (allowRepeat && e.type == EventType::ButtonRepeat)) &&
           e.event.button.button == code && e.event.button.gestureMask == buttonMask(code);
}

// The embedded queue uses one statically allocated mutex; no per-event allocation.
class EventMutex {
public:
#ifdef ARDUINO
    EventMutex() : handle(xSemaphoreCreateMutexStatic(&storage)) {}
    void lock() { xSemaphoreTake(handle, portMAX_DELAY); }
    void unlock() { xSemaphoreGive(handle); }
private:
    StaticSemaphore_t storage;
    SemaphoreHandle_t handle;
#else
    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }
private:
    std::mutex mutex;
#endif
};
class EventLock {
public:
    explicit EventLock(EventMutex& mutex) : mutex(mutex) { mutex.lock(); }
    ~EventLock() { mutex.unlock(); }
    EventLock(const EventLock&) = delete;
private:
    EventMutex& mutex;
};

template <size_t Capacity>
class EventQueue {
    static_assert(Capacity > 0, "EventQueue must not be empty");
public:
    EventQueue() = default;
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    bool push(const Event& event) {
        EventLock lock(mutex);
        if (count == Capacity) {
            // Repeats are disposable; never evict keyboard or transition events.
            size_t repeat = 0;
            while (repeat < count && at(repeat).type != EventType::ButtonRepeat) ++repeat;
            if (repeat == count) { ++dropped; return false; }
            remove(repeat);
            ++dropped;
        }
        buffer[(head + count) % Capacity] = event;
        ++count;
        return true;
    }
    bool pop(Event& event) {
        EventLock lock(mutex);
        if (!count) return false;
        event = at(0);
        head = (head + 1) % Capacity;
        --count;
        return true;
    }
    bool peek(Event& event) const {
        EventLock lock(mutex);
        if (!count) return false;
        event = at(0);
        return true;
    }
    bool isEmpty() const { return size() == 0; }
    bool isFull() const { return size() == Capacity; }
    size_t size() const { EventLock lock(mutex); return count; }
    constexpr size_t capacity() const { return Capacity; }
    uint32_t droppedCount() const { EventLock lock(mutex); return dropped; }
    void clear() { EventLock lock(mutex); head = count = 0; }
private:
    const Event& at(size_t i) const { return buffer[(head + i) % Capacity]; }
    Event& at(size_t i) { return buffer[(head + i) % Capacity]; }
    void remove(size_t i) {
        for (; i + 1 < count; ++i) at(i) = at(i + 1);
        --count;
    }
    mutable EventMutex mutex;
    Event buffer[Capacity]{};
    size_t head = 0, count = 0;
    uint32_t dropped = 0;
};
