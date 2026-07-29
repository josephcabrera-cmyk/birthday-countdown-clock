# Birthday Countdown Clock

A custom PCB that counts down to a chosen birthday in `days:hours:minutes:seconds` across nine 7-segment displays, and plays a tune through a piezo buzzer at midnight on the date.

<!-- TODO: replace with your demo GIF once uploaded (drag the file into a GitHub issue to get a hosted URL, then paste it here) -->
![Demo](docs/demo.gif)

▶ **[Full demo video](TODO_YOUTUBE_UNLISTED_LINK)**

---

## Overview

The clock takes a target date entered through three onboard pushbuttons, computes the remaining interval against a battery-backed real-time clock, and refreshes the display once per second. At midnight on the target date the countdown reaches zero and a buzzer routine plays.

Designed, laid out, assembled, and programmed as a personal hardware project.

| | |
|---|---|
| **MCU** | ESP32-DEVKIT-32E |
| **Timekeeping** | DS3231MZ+TRL TCXO real-time clock |
| **Display driver** | MAX6951CEE+ |
| **Display** | 9 × LDQ-N516RI 7-segment |
| **Audio** | Passive piezo buzzer |
| **Power** | USB-A, 5 V |
| **Firmware** | C <!-- TODO: confirm — ESP-IDF or Arduino core? --> |
| **PCB** | <!-- TODO: layer count --> -layer, designed in Autodesk EAGLE / Fusion Electronics |

---

## Hardware

### Display driving

The countdown needs nine digits — three for days (up to 365), then two each for hours, minutes, and seconds. The MAX6951 drives eight, so the ninth is handled separately.

<!-- TODO: describe how digit 9 is driven — direct ESP32 GPIO? second driver? Worth a sentence, it's the most interesting part of the display design. -->

**Why the MAX6951 over discrete shift registers:** it handles multiplexing, per-digit brightness, and BCD decoding internally over SPI, which keeps the ESP32 free of refresh timing and cuts the GPIO count dramatically versus a 74HC595 chain. <!-- TODO: adjust if your actual reasoning differed — cost, availability, familiarity are all legitimate -->

### Timekeeping

The DS3231MZ+TRL was chosen over the ESP32's internal RTC because the internal oscillator drifts badly and loses state on power cycles. The DS3231 is temperature-compensated, specified to ±2 ppm from 0 °C to +40 °C (roughly one minute per year), and holds time across power loss on its backup cell — necessary for a countdown that may run for months.

<!-- TODO: confirm whether you populated a backup battery, and which -->

### Power

USB-A input at 5 V. <!-- TODO: how is the 3.3 V rail generated — onboard regulator, or the DevKit's? Note any current budget you worked to. -->

---

## User Interface

Three pushbuttons set the target date.

<!-- TODO: document the actual button behavior. Something like:
     - BTN1: cycle field (month / day / year)
     - BTN2: increment
     - BTN3: confirm and store
     Include how the setting is persisted (NVS? DS3231 registers?) -->

---

## Firmware

`firmware/main.c` — <!-- TODO: one line on structure. Main loop? Timer ISR? How the buzzer routine is triggered? -->

### Building

<!-- TODO: fill in whichever applies

**ESP-IDF**
```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**Arduino IDE** — board "ESP32 Dev Module", required libraries: ...
-->

---

## Repository structure

```
.
├── firmware/
│   └── main.c                    # Application firmware
├── hardware/
│   ├── schematic.pdf             # Schematic
│   ├── board-layout.pdf          # PCB layout
│   ├── bill-of-materials.pdf     # BOM
│   └── source/                   # EAGLE/Fusion source (.sch, .brd)
└── docs/
    └── demo.gif
```

---

## Manufacturing

<!-- TODO: if you had these fabricated, note the vendor, board specs (layers, thickness, finish),
     and whether you hand-assembled or used assembly service. Real fab experience is worth stating. -->

---

## Possible improvements

<!-- TODO: 2-3 honest items. This section is worth more than it looks — interviewers read it as
     evidence you evaluate your own work. Candidates: USB-C instead of USB-A, RTC alarm interrupt
     instead of polling, driving all nine digits from one controller, enclosure design. -->

---

## License

<!-- TODO: MIT is the usual default for a project like this. Add a LICENSE file if you want one. -->
