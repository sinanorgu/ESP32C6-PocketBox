#pragma once
#include "Event.hpp"

enum class ButtonSource : uint8_t { Physical, Automation, Shell, Test, Count };

// Platform-independent state machine. One owner calls sample/set/tick in time order.
class ButtonEngine {
public:
    using Sink = bool (*)(void*, const Event&);
    static constexpr uint32_t DebounceMs = 25;
    static constexpr uint32_t LongPressMs = 600;
    static constexpr uint32_t RepeatDelayMs = 450;
    static constexpr uint32_t RepeatIntervalMs = 150;

    ButtonEngine(Sink sink, void* context) : sink(sink), context(context) {}
    uint8_t mask() const { return held; }
    uint8_t sourceState(ButtonSource source) const {
        return source < ButtonSource::Count ? sources[uint8_t(source)] : 0;
    }
    void samplePhysical(uint8_t raw, uint32_t now) {
        uint8_t debounced = sources[0];
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t bit = 1U << i;
            bool down = (raw & bit) != 0;
            if (down != candidate[i]) { candidate[i] = down; changedAt[i] = now; }
            if (uint32_t(now - changedAt[i]) >= DebounceMs) {
                if (down) debounced |= bit;
                else debounced &= ~bit;
            }
        }
        set(ButtonSource::Physical, debounced, now);
        tick(now);
    }
    // Synthetic state changes are already debounced and may be queued back-to-back.
    bool set(ButtonSource source, uint8_t mask, uint32_t now) {
        if (source >= ButtonSource::Count || (mask & ~AllButtons)) return false;
        sources[uint8_t(source)] = mask;
        uint8_t combined = 0;
        for (auto state : sources) combined |= state;
        if (combined == held) return true;
        const uint8_t before = held;
        held = combined;
        const uint8_t added = held & ~before, removed = before & ~held;
        const bool chord = (held & (held - 1)) != 0;
        for (unsigned i = 0; i < 4; ++i) {
            const uint8_t bit = 1U << i;
            if (added & bit) {
                presses[i] = {now, now, bit, false, false, syncPending};
            }
            if ((held & bit) && chord) {
                presses[i].gesture |= held;
                presses[i].suppressed = true;
            }
        }
        for (unsigned i = 0; i < 4; ++i) {
            const uint8_t bit = 1U << i;
            auto& press = presses[i];
            if (removed & bit) {
                if (!press.longSent && uint32_t(now - press.start) >= LongPressMs) {
                    press.longSent = true;
                    emit(EventType::ButtonLongPress, i, now, press.gesture, uint32_t(now - press.start), source);
                }
                emit(EventType::ButtonUp, i, now, press.gesture, uint32_t(now - press.start), source);
                if (!press.suppressed && !press.longSent && !syncPending)
                    emit(EventType::ButtonClick, i, now, press.gesture, uint32_t(now - press.start), source);
            }
            if (added & bit) emit(EventType::ButtonDown, i, now, press.gesture, 0, source);
        }
        if (chord && added) emit(EventType::ButtonChord, 4, now, held, 0, source);
        return true;
    }
    void tick(uint32_t now) {
        if (syncPending) {
            Event e = make(EventType::ButtonStateSync, 4, now, held, 0, ButtonSource::Physical);
            if (!sink(context, e)) return;
            syncPending = false;
        }
        for (unsigned i = 0; i < 4; ++i) {
            if (!(held & (1U << i))) continue;
            auto& press = presses[i];
            uint32_t duration = now - press.start;
            if (!press.longSent && duration >= LongPressMs) {
                press.longSent = true;
                emit(EventType::ButtonLongPress, i, now, press.gesture, duration, ButtonSource::Physical);
            }
            if (!press.suppressed && duration >= RepeatDelayMs &&
                (!press.repeated || uint32_t(now - press.lastRepeat) >= RepeatIntervalMs)) {
                press.repeated = true;
                press.lastRepeat = now;
                emit(EventType::ButtonRepeat, i, now, press.gesture, duration, ButtonSource::Physical);
            }
        }
    }
private:
    struct Press {
        uint32_t start = 0, lastRepeat = 0;
        uint8_t gesture = 0;
        bool longSent = false, repeated = false, suppressed = false;
    } presses[4];
    Sink sink;
    void* context;
    uint8_t sources[uint8_t(ButtonSource::Count)]{}, held = 0;
    bool candidate[4]{}, syncPending = false;
    uint32_t changedAt[4]{};
    Event make(EventType type, unsigned index, uint32_t now, uint8_t gesture,
               uint32_t duration, ButtonSource cause) const {
        Event e;
        e.type = type;
        e.timestamp = now;
        e.event.button.button = index < 4 ? ButtonCode(index + 1) : ButtonCode::None;
        e.event.button.pressedMask = held;
        e.event.button.gestureMask = gesture;
        e.event.button.heldMs = duration;
        e.event.button.repeated = index < 4 && presses[index].repeated;
        uint8_t providers = 0;
        for (unsigned s = 0; s < uint8_t(ButtonSource::Count); ++s)
            if (sources[s] & gesture) providers |= 1U << s;
        e.event.button.sourceMask = providers ? providers : uint8_t(1U << uint8_t(cause));
        return e;
    }
    void emit(EventType type, unsigned index, uint32_t now, uint8_t gesture,
              uint32_t duration, ButtonSource cause) {
        if (!sink(context, make(type, index, now, gesture, duration, cause)) &&
            type != EventType::ButtonRepeat) {
            syncPending = true;
            for (auto& press : presses) press.suppressed = true;
        }
    }
};
