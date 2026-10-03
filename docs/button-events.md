# Button events and automation

Input flows through `GPIO / virtual requests -> ButtonEngine -> EventQueue ->
System::pollEvent -> global button handler -> active screen`. Only
`src/ButtonInput.cpp` reads button GPIOs. The launcher, Settings, Keyboard Test,
Shell and Gellery consume events. BLE still publishes `TextInput` with the same
keyboard payload; existing event enum values are preserved.

## Timing and event contract

One input task starts after hardware initialization and polls approximately every
5 ms (at least one FreeRTOS tick), including while the UI is busy. Its stack is
2048 bytes plus FreeRTOS task overhead. Engine state and the 16-entry virtual
request queue have fixed storage; producing events does not allocate memory.
`ButtonEngine.hpp` runs on the host too, with an explicit millisecond clock.

| Event | Meaning |
| --- | --- |
| `ButtonDown` | Press after 25 ms debounce, independently per physical button. |
| `ButtonUp` | Debounced release. |
| `ButtonClick` | Single-button release before 600 ms, without a combination/lost transition. |
| `ButtonLongPress` | Once per press at 600 ms, including release exactly at the threshold. |
| `ButtonRepeat` | At 450 ms, then every 150 ms; no catch-up burst. |
| `ButtonChord` | At least two buttons held and another button joins. |
| `ButtonStateSync` | Current state after a transition could not be queued. |

The payload contains `button`, `pressedMask` (state after transition),
`gestureMask` (all buttons participating in this press), `sourceMask`, `heldMs`
and `repeated`. `timestamp` is production time, with uint32 wraparound supported.
Use `buttonMask(ButtonCode::Left)`; button enum values are not masks.

All buttons in a `setButtons` request change together. Staggered overlapping
presses also form chords. Participants produce no clicks or further repeats,
even when only one remains held. Their long-press events carry the combination
mask. A repeat delivered before another button joins cannot be undone.

## Consumption and keyboard compatibility

`System::nextEvent()` returns one event or `None`; `pollEvent(Event&)` returns a
boolean. Both invoke the optional global button handler on the UI task. There is
one foreground consumer: do not start another task draining the shared queue.

```cpp
Event event = System::getInstance().nextEvent();
if (event.type == EventType::TextInput) {
    textBox.draw(gfx, event);
} else if (buttonAction(event, ButtonCode::Down, true)) {
    menu.incrementIndex();
} else if (buttonAction(event, ButtonCode::Right)) {
    menu.runSelectedItem();
}
```

Default actions use short clicks on release. Menus/launcher opt into navigation
repeats; release after repeats adds no extra step. Opening, closing, fullscreen
and photo switching never repeat. Long presses have no built-in shortcut assignment. The Left+Right chord saves a
screenshot; other combinations remain available for future shortcuts. Use a short Left press to go back; a long press does
not emit a click on release. This prevents held keys/chords from activating
nested screens accidentally.

Keyboard Test, Shell and the Wi-Fi password box still process text. Non-text
screens ignore text rather than accumulating it until the queue fills. During
Gellery decoding/directory scanning, input is drained: short Left cancels, other
navigation/text is ignored until loading completes. Global handlers still run.
Synchronous Wi-Fi calls still block UI consumption; collecting events does not
make those operations asynchronous.

## Virtual input API

`Physical`, `Automation`, `Shell` and `Test` are separate state owners. Their
masks are ORed: one owner cannot release a key held by another. Only the input
task updates the engine; other tasks enqueue requests:

```cpp
auto& buttons = System::getInstance().buttons;
bool accepted = buttons.press(ButtonSource::Test, ButtonCode::Right);
// At a later scenario step (e.g. after 80 ms), check acceptance again:
bool released = buttons.release(ButtonSource::Test, ButtonCode::Right);

bool chordAccepted = buttons.setButtons(ButtonSource::Automation,
    buttonMask(ButtonCode::Left) | buttonMask(ButtonCode::Right));
bool cleared = buttons.releaseAll(ButtonSource::Automation);
```

Calls do not wait for consumption. Accepted back-to-back press/release requests
remain ordered, not collapsed. Holds produce the same long/repeat events as
physical buttons. `false` means invalid source/mask, input not started or request
queue full. Callers must retry/report failure, especially releases. An owner
should `releaseAll` when its scenario/session ends. These are task-context, not
ISR, APIs. Scenarios should wait for screen readiness before their next action.
This change provides the C++ API; shell commands, ClumsyPL bindings, USB test
protocols can be connected later. Left+Right now invokes screenshot capture through
the global handler installed in `setup()`; see the README for storage and commands.

## Global shortcuts

Register one handler on the UI task, for example in setup:

```cpp
System::getInstance().setButtonHandler([](const Event& event, void*) {
    const uint8_t shortcut = buttonMask(ButtonCode::Left) | buttonMask(ButtonCode::Right);
    if (event.type == EventType::ButtonChord && event.event.button.pressedMask == shortcut) {
        // Schedule a screenshot or another short operation here.
        return true; // Consume this event before it reaches the active screen.
    }
    return false;
});
```

Keep the callback short; do not recursively poll input. It never receives keyboard
events. Registration changes belong to the UI task. A shortcut router can occupy
this single handler slot later.

## Queue pressure and verification

The shared 32-entry queue uses a mutex. At capacity it can evict an older repeat,
preserving other events' order. Otherwise `push` returns false and increments
`droppedCount`; it never overwrites keyboard input to make room. Lost button
transitions suppress in-flight clicks and trigger a retried `ButtonStateSync`.
Consumers tracking held state must honor this snapshot. Under saturation delivery
is not guaranteed; scenarios should check dropped counts and fail/retry.

`pio test -e native` includes `test_buttons`: debounce, long/repeat timing, all 15
nonempty masks, staggered chords, source isolation, clock rollover, overflow,
keyboard payload/FIFO order, and a queued launch/text/back scenario.
`python3 scripts/coverage.py` automatically includes it. Real GPIO timing, BLE
input and task scheduling still need board tests; native tests do not execute
the actual LCD application loops.
