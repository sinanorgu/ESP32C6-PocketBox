#pragma once
#include "ButtonEngine.hpp"
#include <freertos/queue.h>
#include <freertos/task.h>

// Task-context API, not ISR-safe. Return false means the request was not accepted.
class ButtonInput {
public:
    bool begin(EventQueue<32>& queue);
    bool setButtons(ButtonSource source, uint8_t mask) { return enqueue(source, mask, Operation::Set); }
    bool press(ButtonSource source, ButtonCode button) { return change(source, button, Operation::Press); }
    bool release(ButtonSource source, ButtonCode button) { return change(source, button, Operation::Release); }
    bool releaseAll(ButtonSource source) { return setButtons(source, 0); }
private:
    enum class Operation : uint8_t { Set, Press, Release };
    struct Request { ButtonSource source; uint8_t mask; Operation operation; };
    static void task(void* parameter);
    static bool publish(void* context, const Event& event);
    bool enqueue(ButtonSource source, uint8_t mask, Operation operation);
    bool change(ButtonSource source, ButtonCode button, Operation operation) {
        return buttonMask(button) && enqueue(source, buttonMask(button), operation);
    }
    EventQueue<32>* events = nullptr;
    QueueHandle_t requests = nullptr;
    StaticQueue_t requestControl{};
    uint8_t requestStorage[16 * sizeof(Request)]{};
    TaskHandle_t taskHandle = nullptr;
    ButtonEngine engine{publish, this};
};
