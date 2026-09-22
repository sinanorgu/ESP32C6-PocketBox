#include <unity.h>
#include "ButtonEngine.hpp"
#include <vector>

namespace {
constexpr uint8_t Up = buttonMask(ButtonCode::Up);
constexpr uint8_t Down = buttonMask(ButtonCode::Down);
constexpr uint8_t Left = buttonMask(ButtonCode::Left);
constexpr uint8_t Right = buttonMask(ButtonCode::Right);

struct Recorder {
    std::vector<Event> events;
    bool accepting = true;
    static bool record(void* context, const Event& event) {
        auto& self = *static_cast<Recorder*>(context);
        if (!self.accepting) return false;
        self.events.push_back(event);
        return true;
    }
    int count(EventType type) const {
        int result = 0;
        for (const auto& e : events) if (e.type == type) ++result;
        return result;
    }
};

void test_physical_bounce_produces_one_click() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.samplePhysical(Up, 0);
    engine.samplePhysical(0, 10);
    engine.samplePhysical(Up, 15);
    engine.samplePhysical(Up, 39);
    TEST_ASSERT_EQUAL_INT(0, r.events.size());
    engine.samplePhysical(Up, 40);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonDown));
    engine.samplePhysical(0, 100);
    engine.samplePhysical(Up, 110);
    engine.samplePhysical(0, 115);
    engine.samplePhysical(0, 140);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonUp));
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonClick));
    const Event& click = r.events.back();
    TEST_ASSERT_TRUE(buttonAction(click, ButtonCode::Up));
    TEST_ASSERT_EQUAL_UINT32(100, click.event.button.heldMs);
    TEST_ASSERT_EQUAL_UINT8(0, click.event.button.pressedMask);
    TEST_ASSERT_EQUAL_UINT32(140, click.timestamp);
}

void test_long_press_once_and_repeat_timing() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Test, Down, 100);
    engine.tick(549);
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonRepeat));
    engine.tick(550); engine.tick(699);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonRepeat));
    engine.tick(700); engine.tick(850);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonLongPress));
    TEST_ASSERT_EQUAL_INT(3, r.count(EventType::ButtonRepeat));
    engine.set(ButtonSource::Test, 0, 900);
    engine.tick(2000);
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonClick));
    TEST_ASSERT_EQUAL_INT(3, r.count(EventType::ButtonRepeat));
}

void test_repeat_release_does_not_add_menu_step() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Test, Right, 0);
    engine.tick(450);
    engine.set(ButtonSource::Test, 0, 500);
    const Event& click = r.events.back();
    TEST_ASSERT_EQUAL_INT(int(EventType::ButtonClick), int(click.type));
    TEST_ASSERT_TRUE(buttonAction(click, ButtonCode::Right)); // non-repeating action
    TEST_ASSERT_FALSE(buttonAction(click, ButtonCode::Right, true));
}

void test_atomic_chord_suppresses_individual_actions() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Automation, Left | Right, 0);
    TEST_ASSERT_EQUAL_INT(2, r.count(EventType::ButtonDown));
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonChord));
    TEST_ASSERT_EQUAL_UINT8(Left | Right, r.events.back().event.button.pressedMask);
    engine.tick(700);
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonRepeat));
    engine.set(ButtonSource::Automation, Left, 800);
    engine.tick(1500);
    engine.set(ButtonSource::Automation, 0, 1600);
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonClick));
    for (const auto& event : r.events) {
        TEST_ASSERT_FALSE(buttonAction(event, ButtonCode::Left, true));
        TEST_ASSERT_FALSE(buttonAction(event, ButtonCode::Right, true));
    }
}

void test_staggered_chord_and_new_gesture_after_release() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Shell, Left, 0);
    engine.set(ButtonSource::Shell, Left | Up, 50);
    engine.set(ButtonSource::Shell, Up, 100);
    engine.set(ButtonSource::Shell, 0, 150);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonChord));
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonClick));
    engine.set(ButtonSource::Shell, Up, 200);
    engine.set(ButtonSource::Shell, 0, 250);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonClick));
    TEST_ASSERT_TRUE(buttonAction(r.events.back(), ButtonCode::Up));
}

void test_sources_do_not_release_each_others_buttons() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Physical, Up, 0);
    engine.set(ButtonSource::Test, Up, 50);
    engine.set(ButtonSource::Test, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(Up, engine.mask());
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonDown));
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonUp));
    engine.set(ButtonSource::Physical, 0, 150);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonClick));
    TEST_ASSERT_FALSE(engine.set(ButtonSource::Count, Up, 200));
    TEST_ASSERT_FALSE(engine.set(ButtonSource::Test, 0x80, 200));
}

void test_cross_source_chord_metadata() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Physical, Up, 0);
    engine.set(ButtonSource::Shell, Right, 10);
    const auto& chord = r.events.back();
    TEST_ASSERT_EQUAL_INT(int(EventType::ButtonChord), int(chord.type));
    TEST_ASSERT_EQUAL_UINT8(Up | Right, chord.event.button.gestureMask);
    TEST_ASSERT_EQUAL_UINT8((1U << uint8_t(ButtonSource::Physical)) | (1U << uint8_t(ButtonSource::Shell)),
                           chord.event.button.sourceMask);
}

void test_timestamp_wrap_and_no_repeat_burst() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    uint32_t start = UINT32_MAX - 100;
    engine.set(ButtonSource::Test, Up, start);
    engine.tick(start + 450U);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonRepeat));
    engine.tick(start + 10000U);
    TEST_ASSERT_EQUAL_INT(2, r.count(EventType::ButtonRepeat));
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonLongPress));
    engine.set(ButtonSource::Test, 0, start + 10001U);
    TEST_ASSERT_EQUAL_UINT32(10001, r.events.back().event.button.heldMs);
}

void test_dropped_transition_resyncs_without_phantom_click() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    r.accepting = false;
    engine.set(ButtonSource::Test, Up, 0);
    r.accepting = true;
    engine.tick(30);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonStateSync));
    TEST_ASSERT_EQUAL_UINT8(Up, r.events.back().event.button.pressedMask);
    engine.set(ButtonSource::Test, 0, 50);
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonClick));
    engine.set(ButtonSource::Test, Up, 100);
    engine.set(ButtonSource::Test, 0, 150);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonClick));
}

void test_queue_keeps_keyboard_payload_and_order() {
    EventQueue<4> queue;
    Event text; text.type = EventType::TextInput;
    text.event.keyboard.character = 0x00e7;
    text.event.keyboard.ctrl = true;
    Event button; button.type = EventType::ButtonDown;
    TEST_ASSERT_TRUE(queue.push(text));
    TEST_ASSERT_TRUE(queue.push(button));
    TEST_ASSERT_TRUE(queue.push(text));
    Event received;
    TEST_ASSERT_EQUAL_INT(3, queue.size());
    TEST_ASSERT_TRUE(queue.peek(received));
    TEST_ASSERT_EQUAL_UINT32(0x00e7, received.event.keyboard.character);
    TEST_ASSERT_TRUE(received.event.keyboard.ctrl);
    TEST_ASSERT_TRUE(queue.pop(received));
    TEST_ASSERT_EQUAL_INT(int(EventType::TextInput), int(received.type));
    TEST_ASSERT_TRUE(queue.pop(received));
    TEST_ASSERT_EQUAL_INT(int(EventType::ButtonDown), int(received.type));
    TEST_ASSERT_TRUE(queue.pop(received));
    TEST_ASSERT_EQUAL_INT(int(EventType::TextInput), int(received.type));
    TEST_ASSERT_FALSE(queue.pop(received));
}

void test_queue_overflow_evicts_only_repeat() {
    EventQueue<3> queue;
    Event text; text.type = EventType::TextInput; text.event.keyboard.character = 'x';
    Event repeat; repeat.type = EventType::ButtonRepeat;
    Event up; up.type = EventType::ButtonUp;
    queue.push(text); queue.push(repeat); queue.push(text);
    TEST_ASSERT_TRUE(queue.push(up));
    TEST_ASSERT_TRUE(queue.isFull());
    TEST_ASSERT_FALSE(queue.push(up));
    TEST_ASSERT_EQUAL_UINT32(2, queue.droppedCount());
    Event e;
    queue.pop(e); TEST_ASSERT_EQUAL_INT(int(EventType::TextInput), int(e.type));
    queue.pop(e); TEST_ASSERT_EQUAL_INT(int(EventType::TextInput), int(e.type));
    queue.pop(e); TEST_ASSERT_EQUAL_INT(int(EventType::ButtonUp), int(e.type));
    queue.push(up); queue.clear();
    TEST_ASSERT_TRUE(queue.isEmpty());
    TEST_ASSERT_EQUAL_INT(3, queue.capacity());
}

void test_scenario_routes_keyboard_and_clicks_through_same_queue() {
    EventQueue<32> queue;
    ButtonEngine engine([](void* context, const Event& event) {
        return static_cast<EventQueue<32>*>(context)->push(event);
    }, &queue);
    engine.set(ButtonSource::Test, Up, 0);
    engine.set(ButtonSource::Test, 0, 50);
    Event text; text.type = EventType::TextInput; text.event.keyboard.character = 'a';
    queue.push(text);
    engine.set(ButtonSource::Test, Left, 100);
    engine.set(ButtonSource::Test, 0, 150);
    bool inApp = false; int launches = 0, exits = 0, characters = 0;
    Event e;
    while (queue.pop(e)) {
        if (!inApp && buttonAction(e, ButtonCode::Up)) { inApp = true; ++launches; }
        else if (inApp && e.type == EventType::TextInput) ++characters;
        else if (inApp && buttonAction(e, ButtonCode::Left)) { inApp = false; ++exits; }
    }
    TEST_ASSERT_EQUAL_INT(1, launches);
    TEST_ASSERT_EQUAL_INT(1, exits);
    TEST_ASSERT_EQUAL_INT(1, characters);
    TEST_ASSERT_FALSE(inApp);
}

void test_release_on_long_threshold_is_not_a_click() {
    Recorder r; ButtonEngine engine(Recorder::record, &r);
    engine.set(ButtonSource::Test, Right, 0);
    engine.set(ButtonSource::Test, 0, ButtonEngine::LongPressMs);
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonLongPress));
    TEST_ASSERT_EQUAL_INT(1, r.count(EventType::ButtonUp));
    TEST_ASSERT_EQUAL_INT(0, r.count(EventType::ButtonClick));
}

void test_all_button_combinations_and_queue_wrap() {
    for (uint8_t mask = 1; mask <= AllButtons; ++mask) {
        Recorder r; ButtonEngine engine(Recorder::record, &r);
        engine.set(ButtonSource::Test, mask, 0);
        engine.set(ButtonSource::Test, 0, 100);
        const bool chord = (mask & (mask - 1)) != 0;
        TEST_ASSERT_EQUAL_INT(chord ? 1 : 0, r.count(EventType::ButtonChord));
        TEST_ASSERT_EQUAL_INT(chord ? 0 : 1, r.count(EventType::ButtonClick));
    }
    EventQueue<3> queue;
    for (uint32_t i = 0; i < 20; ++i) {
        Event sent; sent.type = EventType::TextInput; sent.timestamp = i;
        TEST_ASSERT_TRUE(queue.push(sent));
        Event received;
        TEST_ASSERT_TRUE(queue.pop(received));
        TEST_ASSERT_EQUAL_UINT32(i, received.timestamp);
    }
}
} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_physical_bounce_produces_one_click);
    RUN_TEST(test_long_press_once_and_repeat_timing);
    RUN_TEST(test_repeat_release_does_not_add_menu_step);
    RUN_TEST(test_atomic_chord_suppresses_individual_actions);
    RUN_TEST(test_staggered_chord_and_new_gesture_after_release);
    RUN_TEST(test_sources_do_not_release_each_others_buttons);
    RUN_TEST(test_cross_source_chord_metadata);
    RUN_TEST(test_timestamp_wrap_and_no_repeat_burst);
    RUN_TEST(test_dropped_transition_resyncs_without_phantom_click);
    RUN_TEST(test_queue_keeps_keyboard_payload_and_order);
    RUN_TEST(test_queue_overflow_evicts_only_repeat);
    RUN_TEST(test_scenario_routes_keyboard_and_clicks_through_same_queue);
    RUN_TEST(test_release_on_long_threshold_is_not_a_click);
    RUN_TEST(test_all_button_combinations_and_queue_wrap);
    return UNITY_END();
}
