# MAX98357A I2S Amplifier Setup for Seeed Studio XIAO ESP32-C3

## Overview

This guide shows how to connect and use the Adafruit MAX98357A I2S 3W Class D Amplifier Breakout with your ESP32-C3 to play audio files from an SD card.

## Hardware Requirements

- Seeed Studio XIAO ESP32-C3
- Adafruit MAX98357A I2S Amplifier Breakout
- Micro SD Card Breakout (already connected)
- Speaker (4-8 ohm, up to 3W)
- Jumper wires

## Wiring Connections

### SD Card (Already Connected)

```
ESP32-C3 → SD Card Breakout
3V3      → 5V
GND      → GND
D0 (GPIO2) → CLK
D1 (GPIO3) → DO
D2 (GPIO4) → DI
D3 (GPIO5) → CS
```

### MAX98357A I2S Amplifier

```
ESP32-C3 → MAX98357A
3V3        → VIN (3.3V power)
GND        → GND (Ground)
D6 (GPIO21) → DIN (I2S Data)
D7 (GPIO20) → BCLK (I2S Bit Clock)
D8 (GPIO8)  → LRC (I2S Left/Right Clock - Word Select)
```

### MAX98357A Optional Connections

- **GAIN**: Leave unconnected (defaults to 9dB gain)
  - Connect to GND for 6dB gain
  - Connect to VIN for 12dB gain
  - Connect to 100K resistor to GND for 15dB gain
- **SD**: Leave unconnected or tie to VIN (shutdown control, high = enabled)

### Speaker Connection

Connect your 4-8 ohm speaker to the **+** and **-** terminals on the MAX98357A.

## Pin Usage Summary

After connecting both the SD card and MAX98357A:

- **Used pins**: D0, D1, D2, D3 (SD card), D6, D7, D8 (I2S audio)
- **Available pins**: D9, D10 (for other peripherals)

## Audio File Format Support

The current implementation supports:

- **WAV files**: Direct I2S playback (recommended)
- **Test tones**: Generated programmatically

**Note**: M4A files require decoding and are not directly supported. Convert your `lovekills_30sec.m4a` to WAV format for playback.

## Converting M4A to WAV

To convert your M4A file to WAV format:

### Using FFmpeg (recommended):

```bash
ffmpeg -i lovekills_30sec.m4a -ar 44100 -ac 2 -f wav lovekills_30sec.wav
```

### Using online converters:

- CloudConvert.com
- Online-Audio-Converter.com
- Zamzar.com

## Code Features

The provided code includes:

1. **SD Card initialization and file listing**
2. **I2S audio setup** with proper pin configuration
3. **Test tone generation** to verify audio output
4. **WAV file playback** from SD card
5. **Automatic audio playback on boot**

## Audio Playback Sequence

When the ESP32-C3 boots up:

1. Initializes SD card and lists files
2. Sets up I2S audio interface
3. Plays a 440Hz test tone for 2 seconds
4. Looks for and plays any WAV files found on SD card
5. If no WAV files found, plays a musical sequence of test tones

## Troubleshooting

### No Audio Output

- Check all wiring connections
- Verify speaker is connected properly
- Ensure 3.3V power is supplied to MAX98357A
- Check serial monitor for error messages

### Distorted Audio

- Reduce volume in code (lower the amplitude in `generateTestTone()`)
- Check speaker impedance (should be 4-8 ohms)
- Verify power supply can handle the current draw

### File Not Found

- Ensure WAV files are in the root directory of SD card
- Check file naming (case sensitive)
- Verify SD card is properly formatted (FAT32)

## Technical Specifications

- **Sample Rate**: 44.1 kHz
- **Bit Depth**: 16-bit
- **Channels**: Stereo
- **I2S Format**: Standard I2S
- **Amplifier Power**: Up to 3W into 4 ohms
- **Supply Voltage**: 3.3V

## Safety Notes

- Start with low volume settings
- Use appropriate speaker impedance (4-8 ohms)
- Ensure adequate power supply capacity
- Avoid short circuits on speaker terminals
