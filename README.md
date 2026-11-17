# Smart Energy Management System

An Arduino Nano project that switches a light (or other load) **only when someone is in the room and it is dark**, then turns it off automatically a few seconds after the room empties. A buzzer warns shortly before shutdown, and a push button lets you override the automatic logic.

## Features

- **Sensor fusion:** an IR sensor at the doorway flags someone entering, and an HLK-LD2410B mmWave radar keeps the room "occupied" while a person is present (even when sitting still).
- **Daylight awareness:** an LDR module stops the light from switching on when the room is already bright.
- **Automatic shutdown:** after the room is vacant, a countdown runs and a buzzer beeps during the last 2 seconds before the relay switches off.
- **Manual override:** one button toggles between AUTO and MANUAL (load forced ON).
- **Status LEDs:** auto mode, manual mode and load power.
- **Non-blocking code:** everything runs on `millis()` timers, with no `delay()` calls.
- **Serial debug output** at 115200 baud to help calibrate the sensors.

## Hardware

| Part | Notes |
|---|---|
| Arduino Nano (ATmega328P) | Powered over USB (5 V) |
| FC-51 IR obstacle sensor | 3 pins, output is active LOW |
| HLK-LD2410B radar | 5 pins, only 5V, GND and OUT are used; OUT is active HIGH |
| LDR module | 3 pins, analog output |
| Relay module (1 channel) | 3 pins, **active LOW** in this build |
| Buzzer module | 3 pins, passive type in this build |
| 3 x LEDs + 220-330 ohm resistors | One resistor per LED |
| Push button | Two pins, connects D5 to GND |

## Wiring

| Component | Component pin | Nano pin |
|---|---|---|
| FC-51 IR | OUT | D2 |
| LD2410B radar | OUT | D4 |
| Override push button | one side | D5 (other side to GND) |
| Relay module | IN | D6 |
| Buzzer module | I/O | D7 |
| LED: auto mode | anode (via resistor) | D8 |
| LED: manual override | anode (via resistor) | D11 |
| LED: load power | anode (via resistor) | D10 |
| LDR module | AO | A0 |

All module `VCC` / `5V` pins go to the Nano's **5V**, and all `GND` pins go to **GND**. LEDs need a 220-330 ohm resistor in series, with the long leg toward the pin and the short leg to GND. The radar's TX and RX pins are not used. Do not connect anything to D0/D1, which are used for USB uploads.

### Relay (load) side

The relay's screw terminals are the switching side:

- **COM**: common terminal
- **NO** (normally open): connected to COM only while the relay is energized. Use this for the load.
- **NC**: do not use for this project.

Break only the live wire of the appliance: one end to COM, the other to NO.

> **Warning: mains voltage is dangerous.** Unplug everything from the wall before touching the relay terminals, and keep them enclosed. If you are not experienced with mains wiring, test with a low-voltage DC lamp on a separate supply first.

## How it works

### Auto mode (default at power-up)

1. **IR trigger:** when the IR sensor goes LOW, the system assumes someone has entered and treats the room as occupied for a 10-second *entry grace* window, giving the radar time to lock on.
2. **Occupancy:** the room is occupied if the entry grace window is active **or** the radar output is HIGH.
3. **Occupied and dark:** the relay turns ON. Once on, it stays on while the room is occupied (this avoids a feedback loop where the lamp's own light makes the LDR think it is bright).
4. **Occupied and bright:** the relay stays OFF.
5. **Vacant:** if the relay is ON and neither sensor sees anyone, a 5-second countdown starts. In the last 2 seconds the buzzer sounds, then the relay turns OFF. If someone is detected during the countdown, it is cancelled.

### Manual override

Press the push button to toggle. In **manual mode** the relay is forced ON and all automatic logic is skipped. Press again to return to auto mode, where the normal vacancy countdown takes over.

### Indicator LEDs

| LED | Pin | Meaning |
|---|---|---|
| Auto mode | D8 | ON in auto mode |
| Manual override | D11 | ON in manual mode |
| Load power | D10 | ON whenever the relay is ON |

## Configuration

Settings are constants at the top of the sketch:

| Setting | Default | Purpose |
|---|---|---|
| `RELAY_ACTIVE_LOW` | `true` | Set to match your relay module. `true` if the relay switches ON when IN is LOW |
| `BUZZER_ACTIVE_LOW` | `false` | `true` for "low level trigger" buzzer modules |
| `BUZZER_PASSIVE` | `true` | `true` if the buzzer needs a tone signal, `false` for a self-oscillating active buzzer |
| `BUZZER_FREQ` | `2500` | Tone frequency in Hz (passive buzzers only) |
| `LDR_DARK_IS_HIGH` | `false` | `true` if your LDR reading goes **up** in the dark |
| `LDR_THRESHOLD` | `400` | Dark/bright cut-off on the 0-1023 scale |
| `TIMEOUT_OFF` | `5000` | Vacancy countdown in ms (5 s is for testing; use minutes in real use) |
| `BEEP_DUR` | `2000` | Warning beep length in ms |
| `ENTRY_GRACE` | `10000` | Time in ms the room counts as occupied after an IR trigger |
| `DEBUG_PRINT` | `true` | Print sensor values once per second |

## Getting started

1. Wire the components as described above.
2. Put the sketch in a folder with the same name as the file (for example `home_automation/home_automation.ino`). The folder must contain **only one** `.ino` file.
3. In the Arduino IDE choose **Board: Arduino Nano** and **Processor: ATmega328P**. If upload fails with an "out of sync" error, choose **ATmega328P (Old Bootloader)**.
4. Upload with the mains side of the relay **disconnected**.
5. Open the Serial Monitor at **115200 baud** and watch the output while you test each sensor.
6. Adjust the configuration flags, then connect your load.

### Project structure

```
home_automation/
  home_automation.ino
README.md
```

## Calibration and tuning

**Relay polarity.** If the relay clicks ON at power-up or the load behaves the wrong way round, flip `RELAY_ACTIVE_LOW`. In this build the module is active LOW.

**LDR.** Watch the `LDR=` value in the Serial Monitor in light and in dark. If the number is low in the dark (as with this build), keep `LDR_DARK_IS_HIGH = false`; otherwise set it to `true`. Adjust `LDR_THRESHOLD` to a value between your dark and bright readings. Keep the LDR out of direct view of the lamp it controls and away from the indicator LEDs.

**Radar.** The LD2410B is very sensitive by default. It detects breathing, fans, curtains and people through thin walls out to about 6 m, so its output may never drop to 0. Use the free **HLKRadarTool** app (Bluetooth, default password `HiLink`) to:

- limit the maximum moving and static gates (each gate is about 0.75 m), for example to gate 2-3
- set the unoccupied delay to about 5-10 s
- raise the thresholds of any gate that stays high in an empty room
- use auto-threshold / background learning with the room empty, if your firmware offers it

Mount the radar firmly, away from the relay, buzzer and loose wires, and aim it at the doorway rather than along a corridor.

**Buzzer.** A passive buzzer needs `BUZZER_PASSIVE = true`. If a module gets warm but makes no sound, it is usually a passive buzzer being fed steady DC, or an active-low module being held on. Test with `tone(7, 2500); delay(1000); noTone(7);`.

## Serial monitor output

| Message | Meaning |
|---|---|
| `[DBG] IR=1 RADAR=0 LDR=8 RELAY=0` | Live values, once per second (IR is `0` when triggered) |
| `[MODE] Switched to: ...` | Auto / manual toggled |
| `[TRIGGER] IR triggered...` | Entry grace window started |
| `[AUTO] Occupied but room is bright...` | Someone detected, but the room is bright so the relay stays off |
| `[TIMER] Room vacant...` | Shutdown countdown started |
| `[OUTPUT] Relay changed to: ON/OFF` | Relay switched |
| `[POWER] Shutdown complete...` | Countdown finished, relay off |

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| "redefinition of ..." compile errors | More than one `.ino` in the sketch folder | Keep a single `.ino` file in the folder |
| Relay on at power-up, inverted behaviour, voltage on NC | Relay module is active LOW | Set `RELAY_ACTIVE_LOW = true` |
| Relay works in manual mode but not in auto | Room considered bright, or LDR direction wrong | Check the `LDR=` value, `LDR_DARK_IS_HIGH` and `LDR_THRESHOLD` |
| `RADAR=1` all the time, relay never turns off | Radar detects something in range | Tune it with HLKRadarTool (see above) |
| Buzzer warm but silent | Passive buzzer fed DC, or active-low module held on | Set `BUZZER_PASSIVE` / `BUZZER_ACTIVE_LOW` correctly |
| LED does not light | Reversed polarity, missing resistor, bad ground or damaged pin | Check polarity, add a 220-330 ohm resistor, or move it to a spare pin and update the pin constant |
| Nano resets or radar flickers when the relay clicks | Power noise on the shared 5V rail | Add 100-470 uF and 100 nF capacitors near the relay and radar, or use a separate 5V supply for the relay |
| Upload fails with "out of sync" | Clone Nano with old bootloader | Choose **ATmega328P (Old Bootloader)** |

## Known limitations

- The FC-51 is a **reflective** sensor with roughly 2-30 cm range, not a true beam-break. It is affected by sunlight and dark surfaces, and it cannot count people or tell entry from exit.
- Once the relay is ON, bright light does not switch it off while the room is occupied. This is deliberate, to avoid flicker caused by the lamp lighting up the LDR.
- Manual mode can only force the load ON.
- The 5 s vacancy timeout is meant for testing.

## Possible improvements

- Configure the LD2410B over UART instead of using only its OUT pin.
- Use two IR sensors to detect direction and count people.
- Add a delayed daylight cut-off with the LDR shielded from the lamp.
- Store settings in EEPROM.
- Put the electronics in an enclosure with the mains side properly isolated.
