# Sound Board Quick Start Guide

## Overview

This guide provides a quick reference for building and deploying your 5-board sound system.

## Hardware Setup

### Wiring (Same for all 5 boards)

```
SD Card Breakout:
ESP32-C3 → SD Card
3V3      → 5V
GND      → GND
D0 (GPIO2) → CLK
D1 (GPIO3) → DO
D2 (GPIO4) → DI
D3 (GPIO5) → CS

MAX98357A Amplifier:
ESP32-C3 → MAX98357A
3V3        → VIN
GND        → GND
D6 (GPIO21) → DIN
D7 (GPIO20) → BCLK
D8 (GPIO8)  → LRC

Buttons (Active LOW with internal pullup):
Red:    D4 (GPIO6)
Green:  D9 (GPIO9)
Blue:   D5 (GPIO7)
Yellow: D10 (GPIO10)

Speaker:
Connect 4-8 ohm speaker to MAX98357A + and - terminals
```

## Software Setup

### 1. Configure Board Environments

Edit [`platformio.ini`](platformio.ini) and set the sound file names for each board:

```ini
[env:board1]
build_flags =
    -DBOARD_ID=1
    -DGREEN_SOUND=\"your_green_sound.wav\"
    -DBLUE_SOUND=\"your_blue_sound.wav\"
    -DYELLOW_SOUND=\"your_yellow_sound.wav\"

[env:board2]
build_flags =
    -DBOARD_ID=2
    -DGREEN_SOUND=\"another_sound.wav\"
    ...
```

### 2. Prepare Audio Files

Convert all MP3 files to WAV format:

```bash
# Single file
ffmpeg -i input.mp3 -ar 44100 -ac 2 -sample_fmt s16 output.wav

# Batch convert
for file in *.mp3; do
    ffmpeg -i "$file" -ar 44100 -ac 2 -sample_fmt s16 "${file%.mp3}.wav"
done
```

### 3. Prepare SD Cards

For each of the 5 SD cards:

1. Format as FAT32
2. Copy ALL WAV files to root directory (all cards identical)
3. Safely eject

### 4. Flash Boards

```bash
# Board 1
pio run -e board1 -t upload -t monitor

# Board 2
pio run -e board2 -t upload -t monitor

# ... repeat for boards 3-5
```

### 5. Test Each Board

For each board:

1. Insert SD card
2. Power on
3. Check serial monitor for:
   - Correct Board ID
   - Sound files discovered
   - ESP-NOW initialized
4. Test buttons:
   - Green → plays configured sound
   - Blue → plays configured sound
   - Yellow → plays configured sound
   - Green + Blue → plays random sound
   - Red → sends message (check serial)

### 6. Test System

With all 5 boards powered on:

1. Press red button on Board 1
2. Verify a random board plays a random sound
3. Repeat for all boards
4. Test multiple simultaneous red button presses

## Button Functions

| Button(s)      | Function                                          |
| -------------- | ------------------------------------------------- |
| Green          | Play static sound (configured in platformio.ini)  |
| Blue           | Play static sound (configured in platformio.ini)  |
| Yellow         | Play static sound (configured in platformio.ini)  |
| Green + Blue   | Play random sound from SD card                    |
| Green + Yellow | Play random sound from SD card                    |
| Blue + Yellow  | Play random sound from SD card                    |
| Red            | Send message to random board to play random sound |

## Communication Flow

```
Board 1 (Red Button) → Broadcast Message → All Boards Receive
                                         ↓
                                    Board 3 (Target)
                                         ↓
                                    Plays Sound

Boards 2, 4, 5 ignore (not target)
```

## Troubleshooting

### No sound on button press

- Check speaker connections
- Verify SD card has WAV files
- Check serial monitor for errors
- Verify sound file names in platformio.ini match actual files

### Red button doesn't trigger remote board

- Check ESP-NOW initialized (serial monitor)
- Verify all boards powered on
- Check target board serial monitor for received message
- Reduce distance between boards if needed

### Wrong board plays sound

- Verify each board has unique BOARD_ID (1-5)
- Check serial monitor shows correct Board ID
- Reflash with correct environment

### Buttons not responding

- Check wiring (buttons should be active LOW)
- Verify GPIO pin numbers
- Test with multimeter
- Check for stuck buttons

## File Structure

```
sound_boards/
├── platformio.ini          # Board configurations
├── src/
│   └── main.cpp           # Main application code
├── ARCHITECTURE.md        # System design document
├── IMPLEMENTATION_PLAN.md # Detailed implementation guide
├── QUICK_START.md         # This file
└── README.md              # Hardware setup guide
```

## Build Environments

| Environment | Board ID | Purpose            |
| ----------- | -------- | ------------------ |
| board1      | 1        | First sound board  |
| board2      | 2        | Second sound board |
| board3      | 3        | Third sound board  |
| board4      | 4        | Fourth sound board |
| board5      | 5        | Fifth sound board  |

## Serial Monitor Commands

Monitor output shows:

- Board ID and configuration
- Sound files discovered
- ESP-NOW status
- Button presses
- Messages sent/received
- Playback status
- Errors and warnings

## Common Serial Output

```
=== ESP32-C3 Sound Board ===
Board ID: 1
Green Sound: sound1.wav
Blue Sound: sound2.wav
Yellow Sound: sound3.wav
Buttons initialized (active LOW with pullup)
SD Card: 8192MB
Discovering sound files...
  Found: sound1.wav
  Found: sound2.wav
  ...
Total sound files: 18
ESP-NOW initialized
Board 1 MAC Address: AA:BB:CC:DD:EE:01
Broadcast peer registered
Board 1 ready to send/receive messages
Ready!
```

## Next Steps

1. Review [`ARCHITECTURE.md`](ARCHITECTURE.md) for system design details
2. Follow [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) for step-by-step coding
3. Test thoroughly before final deployment
4. Document any custom configurations or issues

## Support

For detailed information:

- **System Architecture**: [`ARCHITECTURE.md`](ARCHITECTURE.md)
- **Implementation Steps**: [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md)
- **Hardware Setup**: [`README.md`](README.md)
- **Audio Issues**: [`Audio_Fix_Summary.md`](Audio_Fix_Summary.md)
