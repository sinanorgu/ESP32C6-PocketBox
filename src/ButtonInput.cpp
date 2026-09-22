#include "ButtonInput.hpp"
#include "Definitions.hpp"

bool ButtonInput::begin(EventQueue<32>& queue) {
    if (taskHandle) return true;
    events = &queue;
    requests = xQueueCreateStatic(16, sizeof(Request), requestStorage, &requestControl);
    const uint8_t pins[] = {BUTTON_UP_PIN, BUTTON_DOWN_PIN, BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN};
    for (auto pin : pins) pinMode(pin, INPUT_PULLUP);
    return xTaskCreate(task, "buttons", 2048, this, 1, &taskHandle) == pdPASS;
}

bool ButtonInput::publish(void* context, const Event& event) {
    return static_cast<ButtonInput*>(context)->events->push(event);
}

bool ButtonInput::enqueue(ButtonSource source, uint8_t mask, Operation operation) {
    if (!taskHandle || source == ButtonSource::Physical || source >= ButtonSource::Count ||
        (mask & ~AllButtons)) return false;
    Request request{source, mask, operation};
    return xQueueSend(requests, &request, 0) == pdTRUE;
}

void ButtonInput::task(void* parameter) {
    auto& self = *static_cast<ButtonInput*>(parameter);
    const uint8_t pins[] = {BUTTON_UP_PIN, BUTTON_DOWN_PIN, BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN};
    while (true) {
        uint32_t now = millis();
        uint8_t mask = 0;
        for (unsigned i = 0; i < 4; ++i) if (digitalRead(pins[i]) == LOW) mask |= 1U << i;
        self.engine.samplePhysical(mask, now);
        Request request;
        // Bound work even if another task continuously injects input.
        for (unsigned i = 0; i < 16 && xQueueReceive(self.requests, &request, 0) == pdTRUE; ++i) {
            uint8_t state = self.engine.sourceState(request.source);
            if (request.operation == Operation::Set) state = request.mask;
            else if (request.operation == Operation::Press) state |= request.mask;
            else state &= ~request.mask;
            self.engine.set(request.source, state, millis());
        }
        vTaskDelay(pdMS_TO_TICKS(5) ? pdMS_TO_TICKS(5) : 1);
    }
}
