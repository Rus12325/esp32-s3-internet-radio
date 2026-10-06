# ESP32-S3 Internet Radio

Internet radio player for **ESP32-S3** with a **MAX98357A** I2S amplifier.
Press a button to cycle through a list of MP3/AAC streams.

## Wiring

| MAX98357A | ESP32-S3 |
| --- | --- |
| DIN | GPIO 6 |
| LRC | GPIO 5 |
| BCLK | GPIO 4 |
| VCC | 3V3 |
| GND | GND |

| Component | ESP32-S3 |
| --- | --- |
| Button | GPIO 21 / GND |

The button is read with the internal pull-up, so wire it between the pin and
GND — no external resistor needed.

## Setup

1. Copy the credentials template and fill it in:
   ```bash
   cp src/secrets.h.example src/secrets.h
   ```
   Then edit `src/secrets.h` with your SSID and password. This file is
   gitignored.
2. (Optional) Edit the `stationUrls[]` array in `src/main.cpp` to add your own
   MP3/AAC streams. The URL is matched case-sensitively for `"aac"` to pick the
   decoder, so any other URL is fed to the MP3 decoder.
3. Flash with PlatformIO:
   ```bash
   pio run -t upload
   ```
4. Open the Serial Monitor (115200 baud) to see connection status and the
   current station.

## Usage

Press the button to cycle through the station list.

## How it works

- All four audio objects (I2S output, HTTP source, MP3 generator, AAC
  generator) are allocated **once as statics** — the firmware never calls
  `new`/`delete`. A long-running radio with a heap-allocating stream reader
  would fragment its way to a crash; this design avoids that entirely.
- Switching stations stops the active generator, closes the HTTP stream,
  re-opens the new URL and starts the matching decoder.
- The `loop()` feeds whichever generator is running. When a stream ends or the
  network drops, the station is restarted automatically.
- A Wi-Fi watchdog reconnects and resumes the current station.
- The button is polled with a 400 ms debounce.

## Files

```
├── platformio.ini       # esp32-s3-devkitc-1, 16 MB flash, no PSRAM
├── README.md
└── src/
    ├── main.cpp         # Wi-Fi, station list, I2S player, button
    └── secrets.h.example
```

## Notes

- The board target is N16R8 (16 MB flash, **no PSRAM**); `platformio.ini`
  explicitly sets `board_build.psram_type = null`. If you use a PSRAM board you
  can drop that line, but the firmware does not need it.
- Decoding happens on the fly, so the audio quality is limited by the ESP32-S3
  CPU headroom, not by the amplifier.

## License

No license file is included. Contact the author before redistributing.
