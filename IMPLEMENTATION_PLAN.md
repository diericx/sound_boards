# Sound Board Implementation Plan

## Overview

This document provides a step-by-step implementation guide for the sound board system. Follow these steps in order to build the complete system.

## Prerequisites

- PlatformIO installed and configured
- ESP32-C3 boards wired according to [`ARCHITECTURE.md`](ARCHITECTURE.md)
- SD cards formatted as FAT32
- Audio files converted to WAV format (16-bit, 44.1kHz stereo)

## Implementation Steps

### Step 1: Update PlatformIO Configuration

**File**: [`platformio.ini`](platformio.ini)

**Task**: Create 5 build environments with board-specific configurations

**Details**:

- Each environment defines a unique `BOARD_ID` (1-5)
- Each environment defines 3 sound file mappings for Green, Blue, Yellow buttons
- All environments share the same base configuration

**Example Configuration**:

```ini
[env:board1]
platform = espressif32
board = seeed_xiao_esp32c3
framework = arduino
monitor_speed = 115200
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DBOARD_ID=1
    -DGREEN_SOUND=\"green_sound.wav\"
    -DBLUE_SOUND=\"blue_sound.wav\"
    -DYELLOW_SOUND=\"yellow_sound.wav\"
```

**Validation**:

- Compile each environment successfully
- Verify build flags are correctly defined

---

### Step 2: Define Data Structures and Constants

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Add ESP-NOW message structure and global constants

**Code to Add**:

```cpp
#include <esp_now.h>
#include <WiFi.h>

// Button pin definitions
#define BUTTON_RED 6     // GPIO6 (D4)
#define BUTTON_GREEN 9   // GPIO9 (D9)
#define BUTTON_BLUE 7    // GPIO7 (D5)
#define BUTTON_YELLOW 10 // GPIO10 (D10)

// Button timing constants
#define DEBOUNCE_DELAY 50
#define DUAL_PRESS_WINDOW 100
#define BUTTON_TIMEOUT 5000

// ESP-NOW broadcast address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ESP-NOW message structure
struct ESPNowMessage {
    uint8_t senderBoardId;
    uint8_t targetBoardId;
    char soundFile[64];
    uint32_t timestamp;
    uint8_t checksum;
};

// Button state structure
struct ButtonState {
    uint8_t pin;
    bool currentState;
    bool lastState;
    unsigned long lastDebounceTime;
    bool pressed;
};

// Global variables
ButtonState redButton = {BUTTON_RED, false, false, 0, false};
ButtonState greenButton = {BUTTON_GREEN, false, false, 0, false};
ButtonState blueButton = {BUTTON_BLUE, false, false, 0, false};
ButtonState yellowButton = {BUTTON_YELLOW, false, false, 0, false};

String soundFiles[30];  // Array to store sound file names
int soundFileCount = 0;

bool isPlaying = false;
```

**Validation**:

- Code compiles without errors
- All constants are properly defined

---

### Step 3: Implement Sound File Discovery

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Create function to scan SD card and build list of WAV files

**Function to Add**:

```cpp
void discoverSoundFiles() {
    Serial.println("Discovering sound files...");
    soundFileCount = 0;

    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open root directory");
        return;
    }

    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;

        if (!entry.isDirectory()) {
            String filename = entry.name();
            if (filename.endsWith(".wav") || filename.endsWith(".WAV")) {
                if (soundFileCount < 30) {
                    soundFiles[soundFileCount] = filename;
                    soundFileCount++;
                    Serial.printf("  Found: %s\n", filename.c_str());
                }
            }
        }
        entry.close();
    }
    root.close();

    Serial.printf("Total sound files: %d\n", soundFileCount);
}

String getRandomSound() {
    if (soundFileCount == 0) {
        Serial.println("No sound files available");
        return "";
    }
    uint32_t randomIndex = esp_random() % soundFileCount;
    return soundFiles[randomIndex];
}

uint8_t getRandomBoardId() {
    uint8_t targetId;
    do {
        targetId = (esp_random() % 5) + 1;
    } while (targetId == BOARD_ID);
    return targetId;
}
```

**Validation**:

- Function correctly lists all WAV files on SD card
- Random selection returns valid filenames
- Random board ID excludes current board

---

### Step 4: Implement Button Debouncing

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Create button state management and debouncing logic

**Functions to Add**:

```cpp
void initButtons() {
    pinMode(BUTTON_RED, INPUT_PULLUP);
    pinMode(BUTTON_GREEN, INPUT_PULLUP);
    pinMode(BUTTON_BLUE, INPUT_PULLUP);
    pinMode(BUTTON_YELLOW, INPUT_PULLUP);

    Serial.println("Buttons initialized (active LOW with pullup)");
}

void updateButton(ButtonState* btn) {
    bool reading = digitalRead(btn->pin) == LOW; // Active LOW

    if (reading != btn->lastState) {
        btn->lastDebounceTime = millis();
    }

    if ((millis() - btn->lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != btn->currentState) {
            btn->currentState = reading;
            if (reading) {
                btn->pressed = true;
            }
        }
    }

    btn->lastState = reading;
}

void updateAllButtons() {
    updateButton(&redButton);
    updateButton(&greenButton);
    updateButton(&blueButton);
    updateButton(&yellowButton);
}

bool isButtonPressed(ButtonState* btn) {
    if (btn->pressed) {
        btn->pressed = false;
        return true;
    }
    return false;
}

int countPressedButtons() {
    int count = 0;
    if (greenButton.currentState) count++;
    if (blueButton.currentState) count++;
    if (yellowButton.currentState) count++;
    return count;
}
```

**Validation**:

- Buttons respond reliably without bouncing
- Multiple button presses are detected correctly
- Button state is tracked accurately

---

### Step 5: Implement ESP-NOW Communication

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Initialize ESP-NOW and implement message handling

**Functions to Add**:

```cpp
uint8_t calculateChecksum(const ESPNowMessage* msg) {
    uint8_t sum = 0;
    sum += msg->senderBoardId;
    sum += msg->targetBoardId;
    for (int i = 0; i < sizeof(msg->soundFile) && msg->soundFile[i]; i++) {
        sum += msg->soundFile[i];
    }
    return sum;
}

bool validateMessage(const ESPNowMessage* msg) {
    // Check board ID range
    if (msg->senderBoardId < 1 || msg->senderBoardId > 5) {
        Serial.println("Invalid sender board ID");
        return false;
    }
    if (msg->targetBoardId < 1 || msg->targetBoardId > 5) {
        Serial.println("Invalid target board ID");
        return false;
    }

    // Verify checksum
    uint8_t calculatedChecksum = calculateChecksum(msg);
    if (msg->checksum != calculatedChecksum) {
        Serial.println("Checksum mismatch");
        return false;
    }

    // Check if file exists
    String filePath = "/" + String(msg->soundFile);
    if (!SD.exists(filePath)) {
        Serial.printf("File not found: %s\n", msg->soundFile);
        return false;
    }

    return true;
}

void onDataReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(ESPNowMessage)) {
        Serial.printf("Invalid message size: %d (expected %d)\n", len, sizeof(ESPNowMessage));
        return;
    }

    ESPNowMessage msg;
    memcpy(&msg, data, sizeof(msg));

    // Filter by target board ID
    if (msg.targetBoardId != BOARD_ID) {
        return; // Not for us, ignore silently
    }

    Serial.printf("Received from Board %d: %s\n", msg.senderBoardId, msg.soundFile);

    // Validate and play
    if (validateMessage(&msg)) {
        String filePath = "/" + String(msg.soundFile);
        playWAVFile(filePath.c_str());
    } else {
        Serial.println("Message validation failed");
    }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.printf("Send status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setupESPNow() {
    Serial.println("Initializing ESP-NOW...");

    // Set device as WiFi Station
    WiFi.mode(WIFI_STA);

    // Print MAC address for reference
    Serial.printf("Board %d MAC Address: %s\n", BOARD_ID, WiFi.macAddress().c_str());

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    Serial.println("ESP-NOW initialized");

    // Register callbacks
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceive);

    // Register broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
        return;
    }

    Serial.println("Broadcast peer registered");
    Serial.printf("Board %d ready to send/receive messages\n", BOARD_ID);
}

void sendSoundCommand(uint8_t targetBoard, const char* soundFile) {
    ESPNowMessage msg;
    msg.senderBoardId = BOARD_ID;
    msg.targetBoardId = targetBoard;
    strncpy(msg.soundFile, soundFile, sizeof(msg.soundFile) - 1);
    msg.soundFile[sizeof(msg.soundFile) - 1] = '\0';
    msg.timestamp = millis();
    msg.checksum = calculateChecksum(&msg);

    Serial.printf("Sending to Board %d: %s\n", targetBoard, soundFile);

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&msg, sizeof(msg));

    if (result != ESP_OK) {
        Serial.printf("Send error: %s\n", esp_err_to_name(result));
    }
}
```

**Validation**:

- ESP-NOW initializes successfully
- Broadcast peer is registered
- Messages are sent and received correctly
- Only target board processes messages

---

### Step 6: Implement Button Handlers

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Create handlers for single, dual, and red button presses

**Functions to Add**:

```cpp
void handleSingleButtonPress() {
    if (isButtonPressed(&greenButton)) {
        Serial.println("Green button pressed - playing static sound");
        playWAVFile("/" GREEN_SOUND);
    }
    else if (isButtonPressed(&blueButton)) {
        Serial.println("Blue button pressed - playing static sound");
        playWAVFile("/" BLUE_SOUND);
    }
    else if (isButtonPressed(&yellowButton)) {
        Serial.println("Yellow button pressed - playing static sound");
        playWAVFile("/" YELLOW_SOUND);
    }
}

void handleDualButtonPress() {
    // Check if exactly 2 of the 3 buttons are pressed
    if (countPressedButtons() == 2) {
        Serial.println("Dual button press detected - playing random sound");
        String randomSound = getRandomSound();
        if (randomSound.length() > 0) {
            String filePath = "/" + randomSound;
            playWAVFile(filePath.c_str());
        }

        // Clear button states to prevent repeated triggers
        greenButton.pressed = false;
        blueButton.pressed = false;
        yellowButton.pressed = false;
    }
}

void handleRedButtonPress() {
    if (isButtonPressed(&redButton)) {
        Serial.println("Red button pressed - sending remote command");

        uint8_t targetBoard = getRandomBoardId();
        String randomSound = getRandomSound();

        if (randomSound.length() > 0) {
            sendSoundCommand(targetBoard, randomSound.c_str());
        } else {
            Serial.println("No sounds available to send");
        }
    }
}

void handleButtons() {
    updateAllButtons();

    // Check for dual button press first (higher priority)
    if (countPressedButtons() >= 2) {
        handleDualButtonPress();
    }
    // Then check for red button (remote trigger)
    else if (redButton.currentState) {
        handleRedButtonPress();
    }
    // Finally check for single button presses
    else {
        handleSingleButtonPress();
    }
}
```

**Validation**:

- Single button presses play correct static sounds
- Dual button presses play random sounds
- Red button sends messages to random boards
- Button priority is handled correctly

---

### Step 7: Update Setup Function

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Integrate all components in setup()

**Updated Setup Function**:

```cpp
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("=== ESP32-C3 Sound Board ===");
    Serial.printf("Board ID: %d\n", BOARD_ID);
    Serial.printf("Green Sound: %s\n", GREEN_SOUND);
    Serial.printf("Blue Sound: %s\n", BLUE_SOUND);
    Serial.printf("Yellow Sound: %s\n", YELLOW_SOUND);

    // Initialize buttons
    initButtons();

    // Initialize SD card
    if (!initializeSDCard()) {
        Serial.println("Cannot continue without SD card");
        while (1) delay(1000);
    }

    // Show SD card info
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card: %lluMB\n", cardSize);

    // Discover sound files
    discoverSoundFiles();

    // Initialize I2S audio
    Serial.println("\nInitializing audio...");
    setupI2S();

    // Initialize ESP-NOW
    setupESPNow();

    Serial.println("\n=== Setup Complete ===");
    Serial.println("Button Functions:");
    Serial.println("  Green/Blue/Yellow: Play static sound");
    Serial.println("  Any 2 together: Play random sound");
    Serial.println("  Red: Send random sound to random board");
    Serial.println("Ready!");
}
```

**Validation**:

- All components initialize in correct order
- Serial output shows configuration
- System is ready to receive button presses

---

### Step 8: Update Loop Function

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Implement main loop with button handling

**Updated Loop Function**:

```cpp
void loop() {
    handleButtons();
    delay(10); // Small delay to prevent excessive CPU usage
}
```

**Validation**:

- Loop runs smoothly without blocking
- Buttons are responsive
- ESP-NOW messages are received asynchronously

---

### Step 9: Update playWAVFile Function

**File**: [`src/main.cpp`](src/main.cpp)

**Task**: Add playback state tracking

**Updated Function**:

```cpp
void playWAVFile(const char *filename) {
    isPlaying = true;

    File audioFile = SD.open(filename);
    if (!audioFile) {
        Serial.printf("Failed to open: %s\n", filename);
        isPlaying = false;
        return;
    }

    Serial.printf("Playing: %s (%d bytes)\n", filename, audioFile.size());

    // Skip WAV header (44 bytes for standard WAV)
    if (String(filename).endsWith(".wav") || String(filename).endsWith(".WAV")) {
        audioFile.seek(44);
    }

    size_t bytesRead, bytesWritten;

    while (audioFile.available()) {
        bytesRead = audioFile.read(audioBuffer, BUFFER_SIZE);

        if (bytesRead > 0) {
            processAudioBuffer(audioBuffer, processedBuffer, bytesRead);

            esp_err_t result = i2s_write(I2S_NUM, processedBuffer, bytesRead,
                                         &bytesWritten, pdMS_TO_TICKS(100));
            if (result != ESP_OK) {
                Serial.printf("I2S write error: %s\n", esp_err_to_name(result));
                break;
            }

            if (bytesWritten < bytesRead) {
                taskYIELD();
            }
        }
    }

    audioFile.close();
    isPlaying = false;
    Serial.println("Playback completed");
}
```

**Validation**:

- Playback state is tracked correctly
- Audio plays without interruption
- State is cleared after playback

---

## Testing Plan

### Phase 1: Individual Board Testing

For each board (1-5):

1. **Flash and Verify**

   - Flash with correct environment (board1-board5)
   - Verify serial output shows correct Board ID
   - Verify sound file mappings are correct

2. **SD Card Test**

   - Insert SD card with WAV files
   - Verify all files are discovered
   - Check file count matches expected

3. **Button Test - Single Press**

   - Press Green button → verify correct static sound plays
   - Press Blue button → verify correct static sound plays
   - Press Yellow button → verify correct static sound plays

4. **Button Test - Dual Press**

   - Press Green + Blue → verify random sound plays
   - Press Green + Yellow → verify random sound plays
   - Press Blue + Yellow → verify random sound plays

5. **ESP-NOW Test**
   - Verify ESP-NOW initializes
   - Verify MAC address is displayed
   - Verify broadcast peer is registered

### Phase 2: Multi-Board Testing

With all 5 boards powered on:

1. **Red Button Test**

   - Press red button on Board 1
   - Verify message is sent (check serial)
   - Verify target board receives and plays sound
   - Verify other boards ignore message

2. **Multiple Simultaneous Broadcasts**

   - Press red buttons on multiple boards simultaneously
   - Verify all messages are sent
   - Verify target boards receive and play sounds
   - Check for message collisions or drops

3. **Stress Test**

   - Rapid button presses on multiple boards
   - Verify system remains responsive
   - Check for memory leaks or crashes
   - Monitor for audio glitches

4. **Long Duration Test**
   - Run system for 1+ hours
   - Periodically trigger all button types
   - Monitor for stability issues
   - Check for thermal problems

### Phase 3: Error Scenario Testing

1. **Missing Sound File**

   - Send message with non-existent filename
   - Verify error is logged
   - Verify system continues operating

2. **Invalid Board ID**

   - Manually send message with invalid board ID
   - Verify message is rejected
   - Verify error is logged

3. **SD Card Removal**

   - Remove SD card during operation
   - Verify error handling
   - Re-insert card and verify recovery

4. **Power Cycle**
   - Power cycle individual boards
   - Verify they rejoin network
   - Verify communication resumes

## Deployment Steps

### 1. Prepare Audio Files

```bash
# Convert all MP3 files to WAV
for file in *.mp3; do
    ffmpeg -i "$file" -ar 44100 -ac 2 -sample_fmt s16 "${file%.mp3}.wav"
done

# Verify file format
for file in *.wav; do
    ffprobe "$file" 2>&1 | grep "Audio:"
done
```

### 2. Configure Sound Mappings

Edit [`platformio.ini`](platformio.ini) for each board:

- Set appropriate sound file names for GREEN_SOUND, BLUE_SOUND, YELLOW_SOUND
- Ensure filenames match actual files on SD card

### 3. Flash Boards

```bash
# Flash Board 1
pio run -e board1 -t upload

# Flash Board 2
pio run -e board2 -t upload

# ... repeat for boards 3-5
```

### 4. Prepare SD Cards

For each SD card:

1. Format as FAT32
2. Copy all WAV files to root directory
3. Verify files are readable
4. Insert into board

### 5. System Verification

1. Power on all boards
2. Monitor serial output from each board
3. Record MAC addresses
4. Test each button type on each board
5. Verify inter-board communication
6. Document any issues

## Troubleshooting Guide

### Issue: Board not receiving messages

**Symptoms**: Red button pressed but target board doesn't play sound

**Checks**:

- Verify ESP-NOW initialized successfully
- Check broadcast peer is registered
- Verify target board ID is correct (1-5)
- Check serial output for received messages
- Verify WiFi is in STA mode

**Solutions**:

- Power cycle both boards
- Verify both boards are on same WiFi channel
- Check for WiFi interference
- Reduce distance between boards

### Issue: Wrong sound plays

**Symptoms**: Button press plays unexpected sound

**Checks**:

- Verify build flags in platformio.ini
- Check BOARD_ID is correct
- Verify sound file names match configuration
- Check SD card has correct files

**Solutions**:

- Reflash with correct environment
- Verify sound file mappings
- Check for typos in filenames

### Issue: Buttons not responding

**Symptoms**: Button presses don't trigger actions

**Checks**:

- Verify button wiring (active LOW with pullup)
- Check button pin definitions
- Monitor serial output for button state changes
- Test with multimeter

**Solutions**:

- Check physical connections
- Verify GPIO pins are correct
- Test buttons individually
- Check for stuck buttons

### Issue: Audio quality problems

**Symptoms**: Crackling, distortion, or no sound

**Checks**:

- Verify WAV file format (16-bit, 44.1kHz stereo)
- Check I2S wiring
- Verify speaker impedance (4-8 ohms)
- Check power supply

**Solutions**:

- Re-convert audio files with correct format
- Verify all I2S connections
- Use wall adapter instead of USB power
- Adjust VOLUME_LEVEL if needed

## Success Criteria

The implementation is complete when:

- [ ] All 5 boards flash successfully with unique IDs
- [ ] Each board discovers and lists sound files correctly
- [ ] Single button presses play correct static sounds
- [ ] Dual button presses play random sounds
- [ ] Red button sends messages to random boards
- [ ] Target boards receive and play requested sounds
- [ ] Non-target boards ignore messages
- [ ] System operates reliably for extended periods
- [ ] Error handling works as expected
- [ ] Serial output provides useful debugging information

## Next Steps

After successful implementation:

1. **Documentation**: Create user manual with button functions
2. **Enclosure**: Design and build cases for boards
3. **Power**: Add battery operation if needed
4. **Enhancements**: Consider adding LED feedback or volume control
5. **Maintenance**: Establish procedure for updating sound files

## Support Resources

- **Architecture**: See [`ARCHITECTURE.md`](ARCHITECTURE.md) for system design
- **Hardware**: See [`README.md`](README.md) for wiring details
- **Audio**: See [`Audio_Fix_Summary.md`](Audio_Fix_Summary.md) for audio troubleshooting
- **ESP-NOW**: [Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/network/esp_now.html)
