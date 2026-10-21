# Modularized firmware

The same firmware as [`../main.c`](../main.c), split by peripheral into an
ESP-IDF project that builds directly rather than needing to be copied into one.

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

| File | Responsibility |
|---|---|
| `main/main.c` | `app_main`, and the once-per-second countdown loop |
| `main/max6951.c` | SPI bus setup and the display driver |
| `main/ds3231.c` | I2C bus setup and RTC read/write |
| `main/direct_led.c` | The ninth digit, driven straight off GPIO |
| `main/display.c` | Composition over both: prompts, numbers, blanking |
| `main/buttons.c` | Pin config and edge-triggered debounce |
| `main/buzzer.c` | LEDC PWM and the melody table |
| `main/setup_ui.c` | The two-stage setup state machine |
| `main/countdown.c` | Days-until-birthday arithmetic |

## Status

**This has not been run on hardware.** `firmware/main.c` is the version that was
built, flashed, and demonstrated. Treat this as a refactor pending verification.

## What changed, and what did not

This is a readability refactor. Behavior is intended to be identical, so the
known issues in the original are preserved rather than fixed — the birthday
re-entry path still does not accept changed values, the countdown arithmetic
still assumes a target minute of zero, `PATTERN_G` still equals `PATTERN_D` so
`G` renders as `D`, and the `SET` handler in the countdown loop still logs
"Long press detected" for what is an ordinary press.

Three changes are not pure code movement:

1. **`current_field_value()` extracted.** The eleven-branch ternary chain
   selecting the field being edited appeared verbatim three times in
   `handle_setup_mode`; it is now one function called three times.
2. **`birthday_field_value()` added.** The equivalent chain in
   `handle_birthday_setup_mode` falls back to `birthday_second` rather than
   `second`. The two are interchangeable for every state that function can
   actually reach, but it was given its own helper rather than resting on that.
3. **`birthday_values_t` removed.** It was declared in the original and never
   referenced anywhere.

Everything else is reindentation and the mechanical move of functions, defines
and constants into per-peripheral files.
