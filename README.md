# ESP32-C3 Audio Player with MAX98357A

A complete audio playback system using the Seeed Studio XIAO ESP32-C3, MAX98357A I2S amplifier, and SD card storage.

## Hardware Setup

### Components Required

- Seeed Studio XIAO ESP32-C3
- Adafruit MAX98357A I2S 3W Class D Amplifier Breakout
- Micro SD Card Breakout Board
- 4-8 ohm speaker
- MicroSD card (FAT32 formatted)

### Wiring Connections

**SD Card Breakout:**

```
ESP32-C3 → SD Card Breakout
3V3      → 5V
GND      → GND
D0 (GPIO2) → CLK
D1 (GPIO3) → DO
D2 (GPIO4) → DI
D3 (GPIO5) → CS
```

**MAX98357A I2S Amplifier:**

```
ESP32-C3 → MAX98357A
3V3        → VIN
GND        → GND
D6 (GPIO21) → DIN
D7 (GPIO20) → BCLK
D8 (GPIO8)  → LRC
```

**Speaker:**

- Connect 4-8 ohm speaker to MAX98357A + and - terminals

## Key Learnings

### 1. GPIO Pin Mapping

**Critical Discovery:** The Seeed Studio XIAO ESP32-C3 pin labels don't match GPIO numbers:

- D6 = GPIO21 (not GPIO6)
- D7 = GPIO20 (not GPIO7)
- D8 = GPIO8 (correct)

### 2. SD Card Initialization

**Issue:** SD card mounting can fail after I2S initialization due to SPI timing.
**Solution:** Use multiple initialization methods with different SPI speeds:

- Try 4MHz first (most reliable)
- Fall back to default speed
- Fall back to 1MHz (slowest but most compatible)

### 3. I2S Configuration

**Working settings for MAX98357A:**

- Sample Rate: 44.1kHz
- Bit Depth: 16-bit
- Channel Format: Stereo (I2S_CHANNEL_FMT_RIGHT_LEFT)
- Communication Format: Standard I2S
- DMA Buffer: 8 buffers × 64 samples

## Audio File Support

### Supported Formats

- **WAV files**: Direct I2S playback (16-bit, 44.1kHz stereo recommended)

### Unsupported Formats

- **M4A/MP4**: Requires decoding (not supported by raw I2S)

### File Conversion

Convert M4A to WAV using FFmpeg:

```bash
ffmpeg -i input.m4a -ar 44100 -ac 2 -f wav output.wav
```

## Code Structure

### Main Functions

- `setupI2S()`: Configures I2S interface with proper error handling
- `initializeSDCard()`: Robust SD card initialization with multiple fallback methods
- `playWAVFile()`: Streams WAV files directly to I2S output
- `listFiles()`: Shows all files on SD card for debugging

### Boot Sequence

1. Initialize serial communication
2. Initialize SD card with fallback methods
3. List files on SD card
4. Initialize I2S audio interface
5. Search for and play first WAV file found
6. Provide conversion instructions for M4A files

## Troubleshooting

### No Audio Output

- Verify correct GPIO pin connections (D6=GPIO21, D7=GPIO20, D8=GPIO8)
- Check speaker impedance (4-8 ohms)
- Ensure MAX98357A has proper power (3.3V)

### SD Card Mount Failed

- Check all 4 SD card connections
- Verify SD card is FAT32 formatted
- Try different SD card
- Check power supply to SD breakout (3V3 → 5V)

### File Not Found

- Ensure WAV files are in root directory of SD card
- Check file naming (case sensitive)
- Convert M4A files to WAV format

## Technical Specifications

- **Platform**: ESP32-C3 (RISC-V architecture)
- **Audio Output**: I2S to MAX98357A Class D amplifier
- **Power Output**: Up to 3W @ 4 ohms
- **Sample Rate**: 44.1kHz
- **Bit Depth**: 16-bit stereo
- **Storage**: MicroSD card (FAT32)
- **Power**: 3.3V operation

## Project Files

- `src/main.cpp`: Main application code
- `platformio.ini`: PlatformIO configuration
- `MAX98357A_Setup.md`: Detailed setup guide
- `Audio_Troubleshooting.md`: Comprehensive troubleshooting guide

## Usage

1. Wire components according to connection diagrams
2. Format SD card as FAT32
3. Copy WAV files to SD card root directory
4. Upload code to ESP32-C3
5. Power on - audio will play automatically on boot

The system will automatically find and play the first WAV file on the SD card when powered on.
