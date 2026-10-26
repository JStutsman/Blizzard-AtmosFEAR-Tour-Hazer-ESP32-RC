# Hazer Remote (transmitter)

Wireless touchscreen remote for the **Blizzard AtmosFEAR Tour HZ** haze machine. The screen copies the hazer's front panel, and the remote talks to a small receiver at the hazer over ESP-NOW. There's no router, app or pairing step.

This repo is the **remote**. The receiver that plugs into the hazer's DMX input is in
[Blizzard-AtmosFEAR-Tour-Hazer-ESP32-RC-Receiver](https://github.com/JStutsman/Blizzard-AtmosFEAR-Tour-Hazer-ESP32-RC-Receiver).

**📖 [User Guide (PDF)](docs/Hazer-Remote-User-Guide.pdf)** covers setup, every screen, DMX addressing, using several remotes and troubleshooting.

Firmware **v1.1**. Built and running on hardware: 2 remotes and 2 receivers.

## Features

- **Front-panel mimic:** a blue display plus FUNCTION, UP, DOWN, TIMER, START/STOP and MANUAL, with the hazer's own Interval, Duration, Volume and FanSpeed pages. The interval/duration timer runs on the remote.
- **One or two hazers:** control RX 1, RX 2 or both, each on its own DMX address, e.g. 89 and 109.
- **Live link status:** a bottom line per receiver shows the DMX address, signal strength and a signal icon. A red X means no link.
- **DMX addresses set from the remote:** the remote remembers each receiver's address and re-sends it when a receiver that was offline comes back.
- **Several remotes:** each receiver follows one remote at a time. Other remotes show *In use* and can take over RX 1, RX 2 or both.
- **Settings behind a gear icon:** receivers, DMX addresses, Normal/Dark display and backlight.
- **Number pad** for typing any value.
- **Fail-safe:** a receiver stops the hazer after 3 s without data.
- **Debug screen** (PIN 1234): firmware versions and build date, MACs, signal at both ends, echoed DMX values and status flags.
- **Receiver setup mode** (PIN 4817): sends the receivers into their WiFi setup page.
- **Serial command line:** everything the touchscreen does, plus `status`, `screen`, `debug` and `watch`, over USB at 115200 baud. Type `help`.

## Hardware

| Part | Notes |
|---|---|
| ESP32-2432S028R ("CYD") | 2.8" 320x240 ILI9341 touchscreen with XPT2046 touch |

## Building and flashing

Open the workspace in VS Code with PlatformIO. Clone both repos side by side:

```
git clone https://github.com/JStutsman/Blizzard-AtmosFEAR-Tour-Hazer-ESP32-RC.git remote-controller
git clone https://github.com/JStutsman/Blizzard-AtmosFEAR-Tour-Hazer-ESP32-RC-Receiver.git receiver
```

Then build and upload (env `cyd`; LovyanGFX is pulled in automatically):

```
pio run -d remote-controller -t upload --upload-port COMxx
```

`platformio.ini` points `src_dir` at `Hazer-TX/`, so the sketch also opens unchanged in the Arduino IDE. For the Arduino IDE, use board *ESP32 Dev Module*, ESP32 core 3.x, and install the LovyanGFX library.

### Serial commands

Connect by USB and open a terminal at 115200 baud, e.g. `pio device monitor -d remote-controller`. A few examples:

```
start / stop                 manual on|off|<seconds>      timer on|off
volume 60   fan 80           interval 30   duration 5     page interval
rx both     dmx 1 89         display dark  backlight 30   take 2
status      screen           debug         watch 2        help
```

The full list is in `help` and in section 14 of the user guide.

### Configuration (`Hazer-TX/Config.h`)

| Setting | Default | Notes |
|---|---|---|
| `RX_MAC_RECEIVER_1` / `_2` | your receivers | Each receiver prints its MAC on the serial monitor at boot |
| `RADIO_CHANNEL` | 6 | Must match the receivers |
| `HZ_CONFIG_PIN` | 4817 | Sends receivers into setup mode |
| `HZ_DEBUG_PIN` | 1234 | Opens the debug screen |
| `DISPLAY_INVERT`, `DISPLAY_RGB_ORDER` | false | For panels with inverted or swapped colors |
| `TOUCH_CAL_ON_BOOT` | false | Set true once to print touch calibration values |

If the display stays blank, change `Panel_ILI9341` to `Panel_ST7789` in `Display.h`.

## Code layout

```
Hazer-TX/
  Hazer-TX.ino     main loop: touch -> screens, 20 Hz data send, DMX address upkeep
  Config.h         compile-time settings (receiver MACs, PINs, radio channel, display)
  FixtureTable.h   receivers and fixtures this remote knows
  HazerProtocol.h  wire protocol - must stay identical to the receiver's copy
  Radio.*          ESP-NOW sends and per-receiver status
  HazerState.*     panel logic (pages, timer, output) and saved settings
  UiPanel.*        main panel screen: blue display, info line, gear, six buttons
  UiSettings.*     gear screen
  UiNumpad.*       number pad
  UiConfig.*       PIN entry, receiver setup mode, take control, debug screen
  SerialCli.*      serial command line (type "help")
  Theme.*          Normal / Dark colors and backlight
  Display.*        LovyanGFX setup for the CYD
  BuildStamp.cpp   build date (touched before every build by touch_build_stamp.py)
docs/
  Hazer-Remote-User-Guide.pdf
  make_user_guide.py   rebuilds the PDF (needs reportlab)
```

## Protocol summary

The remote sends `HzPacket` messages: DATA (emit, fan, volume) at 20 Hz, plus SAFE, ENTER_CONFIG, SET_BASE, IDENTIFY and TAKE_CONTROL. Each receiver replies with `HzStatus` 4 times a second: firmware version, RSSI, the values it is outputting, its DMX base address, and flags (FAILSAFE, CONFIG, IDENTIFY, BUSY). A receiver follows one remote at a time. It answers other remotes with BUSY until the owner has been silent for 3 s, releases with SAFE, or another remote sends TAKE_CONTROL.
