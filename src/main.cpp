#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <esp_now.h>
#include <WiFi.h>

// SD card pin definitions for ESP32-C3
#define SD_CS_PIN 5   // D3 -> CS
#define SD_MOSI_PIN 4 // D2 -> DI
#define SD_MISO_PIN 3 // D1 -> DO
#define SD_SCK_PIN 2  // D0 -> CLK

// I2S pins for MAX98357A (correct GPIO mapping for XIAO ESP32-C3)
#define I2S_DOUT 21 // D6 -> DIN (GPIO21, not GPIO6)
#define I2S_BCLK 20 // D7 -> BCLK (GPIO20, not GPIO7)
#define I2S_LRC 8   // D8 -> LRC (GPIO8)

// Button pin definitions
#define BUTTON_RED 6     // GPIO6 (D4)
#define BUTTON_GREEN 9   // GPIO9 (D9)
#define BUTTON_BLUE 7    // GPIO7 (D5)
#define BUTTON_YELLOW 10 // GPIO10 (D10)

// Button timing constants
#define DEBOUNCE_DELAY 50
#define DUAL_PRESS_WINDOW 100
#define BUTTON_TIMEOUT 5000

// I2S configuration
#define I2S_NUM I2S_NUM_0
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define CHANNEL_FORMAT I2S_CHANNEL_FMT_RIGHT_LEFT
#define BUFFER_SIZE 1024

// Software gain control for MAX98357A with 3W @ 4Ω speakers
// At 3.3V supply: Theoretical max ~2.7W (limited by supply voltage)
// Software gain set to 1.0 (100%) to maximize loudness
// Safe: 3.3V supply limits output well below 3W speaker rating
#define SOFTWARE_GAIN 1.0

// ESP-NOW broadcast address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ESP-NOW message structure
struct ESPNowMessage
{
  uint8_t senderBoardId;
  uint8_t targetBoardId;
  char soundFile[64];
  uint32_t timestamp;
  uint8_t checksum;
};

// Button state structure
struct ButtonState
{
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

String soundFiles[30]; // Array to store sound file names
int soundFileCount = 0;
uint8_t boardId = 0; // Board ID loaded from SD card

// Sound file assignments (loaded from SD card by index)
String greenSound = "";
String blueSound = "";
String yellowSound = "";

bool isPlaying = false;

uint8_t audioBuffer[BUFFER_SIZE];
int16_t processedBuffer[BUFFER_SIZE / 2]; // For 16-bit audio processing

// Function declarations
void setupI2S();
void setupESPNow();
void initButtons();
bool loadBoardId();
void discoverSoundFiles();
void assignSoundsByIndex();
String getRandomSound();
uint8_t getRandomBoardId();
void updateButton(ButtonState *btn);
void updateAllButtons();
bool isButtonPressed(ButtonState *btn);
int countPressedButtons();
void handleButtons();
void handleSingleButtonPress();
void handleDualButtonPress();
void handleRedButtonPress();
void sendSoundCommand(uint8_t targetBoard, const char *soundFile);
uint8_t calculateChecksum(const ESPNowMessage *msg);
bool validateMessage(const ESPNowMessage *msg);
void onDataReceive(const uint8_t *mac, const uint8_t *data, int len);
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void playWAVFile(const char *filename);
bool initializeSDCard();

// Audio processing functions
int16_t applyVolumeControl(int16_t sample, float volume)
{
  // Apply volume scaling with soft limiting to prevent crackling
  int32_t scaled = (int32_t)(sample * volume);

  // Soft limiting to prevent harsh clipping that causes crackling
  if (scaled > 28000)
    scaled = 28000 + (scaled - 28000) / 4;
  if (scaled < -28000)
    scaled = -28000 + (scaled + 28000) / 4;

  // Final hard clamp
  if (scaled > 32767)
    scaled = 32767;
  if (scaled < -32768)
    scaled = -32768;

  return (int16_t)scaled;
}

void processAudioBuffer(uint8_t *rawBuffer, int16_t *processedBuffer, size_t bytesRead)
{
  // Convert bytes to 16-bit samples and apply volume control
  size_t sampleCount = bytesRead / 2; // 16-bit = 2 bytes per sample

  for (size_t i = 0; i < sampleCount; i++)
  {
    // Convert little-endian bytes to 16-bit sample
    int16_t sample = (int16_t)(rawBuffer[i * 2] | (rawBuffer[i * 2 + 1] << 8));

    // Apply software gain and limiting
    processedBuffer[i] = applyVolumeControl(sample, SOFTWARE_GAIN);
  }
}

void setupI2S()
{
  // Uninstall any existing I2S driver
  i2s_driver_uninstall(I2S_NUM);

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = BITS_PER_SAMPLE,
      .channel_format = CHANNEL_FORMAT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 16,
      .dma_buf_len = 128,
      .use_apll = true,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_LRC,
      .data_out_num = I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE};

  esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  if (err != ESP_OK)
  {
    Serial.printf("I2S driver install failed: %s\n", esp_err_to_name(err));
    return;
  }

  err = i2s_set_pin(I2S_NUM, &pin_config);
  if (err != ESP_OK)
  {
    Serial.printf("I2S pin config failed: %s\n", esp_err_to_name(err));
    return;
  }

  i2s_zero_dma_buffer(I2S_NUM);
  Serial.println("I2S initialized successfully");
}

bool initializeSDCard()
{
  Serial.println("Initializing SD card...");

  // End any previous SD card session and SPI
  SD.end();
  SPI.end();
  delay(200);

  // Configure CS pin as output and set HIGH (deselect) BEFORE SPI.begin
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(100);

  // Initialize SPI with custom pins
  // Note: SPI.begin() parameter order is (SCK, MISO, MOSI, SS)
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(300);

  // Send some clock pulses to wake up the SD card
  digitalWrite(SD_CS_PIN, HIGH);
  for (int i = 0; i < 10; i++)
  {
    SPI.transfer(0xFF);
  }
  delay(100);

  // Try initialization with explicit SPI bus and lower speed for reliability
  Serial.println("Attempting SD.begin() with 4MHz clock...");
  if (!SD.begin(SD_CS_PIN, SPI, 4000000))
  {
    Serial.println("SD card initialization failed at 4MHz");

    // Try with 1MHz
    delay(500);
    Serial.println("Retrying with 1MHz clock...");
    if (!SD.begin(SD_CS_PIN, SPI, 1000000))
    {
      Serial.println("SD card initialization failed at 1MHz");

      // Try one more time with even slower speed
      delay(500);
      Serial.println("Retrying with 400kHz clock...");
      if (!SD.begin(SD_CS_PIN, SPI, 400000))
      {
        Serial.println("SD card initialization failed at 400kHz");
        Serial.println("Please check:");
        Serial.println("  - SD card is properly inserted");
        Serial.println("  - SD card is formatted as FAT32");
        Serial.println("  - Wiring connections are correct");
        Serial.println("  - SD card pins:");
        Serial.printf("    CS:   GPIO%d\n", SD_CS_PIN);
        Serial.printf("    MOSI: GPIO%d\n", SD_MOSI_PIN);
        Serial.printf("    MISO: GPIO%d\n", SD_MISO_PIN);
        Serial.printf("    SCK:  GPIO%d\n", SD_SCK_PIN);
        return false;
      }
    }
  }

  Serial.println("SD card initialized successfully");

  // Verify we can access the card
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card detected");
    return false;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
    Serial.println("MMC");
  else if (cardType == CARD_SD)
    Serial.println("SDSC");
  else if (cardType == CARD_SDHC)
    Serial.println("SDHC");
  else
    Serial.println("UNKNOWN");

  return true;
}

// Load board ID from SD card
bool loadBoardId()
{
  Serial.println("Loading board ID from SD card...");

  // Check for id files 1.txt through 5.txt
  for (uint8_t id = 1; id <= 5; id++)
  {
    String filename = "/" + String(id) + ".txt";
    if (SD.exists(filename))
    {
      boardId = id;
      Serial.printf("Found %s - Board ID set to %d\n", filename.c_str(), boardId);
      return true;
    }
  }

  Serial.println("ERROR: No board ID file found (1.txt, 2.txt, 3.txt, 4.txt, or 5.txt)");
  return false;
}

// Sound file discovery
void discoverSoundFiles()
{
  Serial.println("Discovering sound files...");
  soundFileCount = 0;

  File root = SD.open("/");
  if (!root)
  {
    Serial.println("Failed to open root directory");
    return;
  }

  while (true)
  {
    File entry = root.openNextFile();
    if (!entry)
      break;

    if (!entry.isDirectory())
    {
      String filename = entry.name();
      if (filename.endsWith(".wav") || filename.endsWith(".WAV"))
      {
        if (soundFileCount < 30)
        {
          soundFiles[soundFileCount] = filename;
          soundFileCount++;
          Serial.printf("  Found: %s\n", filename.c_str());
        }
      }
    }
    entry.close();
  }
  root.close();

  Serial.printf("Total sound files found: %d\n", soundFileCount);

  // Sort sound files alphabetically
  if (soundFileCount > 1)
  {
    Serial.println("Sorting sound files alphabetically...");
    for (int i = 0; i < soundFileCount - 1; i++)
    {
      for (int j = i + 1; j < soundFileCount; j++)
      {
        if (soundFiles[i].compareTo(soundFiles[j]) > 0)
        {
          String temp = soundFiles[i];
          soundFiles[i] = soundFiles[j];
          soundFiles[j] = temp;
        }
      }
    }
    Serial.println("Sorted sound files:");
    for (int i = 0; i < soundFileCount; i++)
    {
      Serial.printf("  %d: %s\n", i + 1, soundFiles[i].c_str());
    }
  }
}

// Assign sounds by index (first 3 WAV files)
void assignSoundsByIndex()
{
  Serial.println("Assigning sounds by index...");

  if (soundFileCount < 3)
  {
    Serial.printf("ERROR: Need at least 3 WAV files, found %d\n", soundFileCount);
    return;
  }

  greenSound = soundFiles[0];
  blueSound = soundFiles[1];
  yellowSound = soundFiles[2];

  Serial.printf("  Green button: %s\n", greenSound.c_str());
  Serial.printf("  Blue button: %s\n", blueSound.c_str());
  Serial.printf("  Yellow button: %s\n", yellowSound.c_str());
}

String getRandomSound()
{
  if (soundFileCount == 0)
  {
    Serial.println("No sound files available");
    return "";
  }
  uint32_t randomIndex = esp_random() % soundFileCount;
  return soundFiles[randomIndex];
}

uint8_t getRandomBoardId()
{
  uint8_t targetId;
  do
  {
    targetId = (esp_random() % 5) + 1;
  } while (targetId == boardId);
  return targetId;
}

// Button management
void initButtons()
{
  pinMode(BUTTON_RED, INPUT_PULLUP);
  pinMode(BUTTON_GREEN, INPUT_PULLUP);
  pinMode(BUTTON_BLUE, INPUT_PULLUP);
  pinMode(BUTTON_YELLOW, INPUT_PULLUP);

  Serial.println("Buttons initialized (active LOW with pullup)");
}

void updateButton(ButtonState *btn)
{
  bool reading = digitalRead(btn->pin) == LOW; // Active LOW

  if (reading != btn->lastState)
  {
    btn->lastDebounceTime = millis();
  }

  if ((millis() - btn->lastDebounceTime) > DEBOUNCE_DELAY)
  {
    if (reading != btn->currentState)
    {
      btn->currentState = reading;
      if (reading)
      {
        btn->pressed = true;
      }
    }
  }

  btn->lastState = reading;
}

void updateAllButtons()
{
  updateButton(&redButton);
  updateButton(&greenButton);
  updateButton(&blueButton);
  updateButton(&yellowButton);
}

bool isButtonPressed(ButtonState *btn)
{
  if (btn->pressed)
  {
    btn->pressed = false;
    return true;
  }
  return false;
}

int countPressedButtons()
{
  int count = 0;
  if (greenButton.currentState)
    count++;
  if (blueButton.currentState)
    count++;
  if (yellowButton.currentState)
    count++;
  return count;
}

// ESP-NOW communication
uint8_t calculateChecksum(const ESPNowMessage *msg)
{
  uint8_t sum = 0;
  sum += msg->senderBoardId;
  sum += msg->targetBoardId;
  for (int i = 0; i < sizeof(msg->soundFile) && msg->soundFile[i]; i++)
  {
    sum += msg->soundFile[i];
  }
  return sum;
}

bool validateMessage(const ESPNowMessage *msg)
{
  // Check board ID range
  if (msg->senderBoardId < 1 || msg->senderBoardId > 5)
  {
    Serial.println("Invalid sender board ID");
    return false;
  }
  if (msg->targetBoardId < 1 || msg->targetBoardId > 5)
  {
    Serial.println("Invalid target board ID");
    return false;
  }

  // Verify checksum
  uint8_t calculatedChecksum = calculateChecksum(msg);
  if (msg->checksum != calculatedChecksum)
  {
    Serial.println("Checksum mismatch");
    return false;
  }

  // Check if file exists
  String filePath = "/" + String(msg->soundFile);
  if (!SD.exists(filePath))
  {
    Serial.printf("File not found: %s\n", msg->soundFile);
    return false;
  }

  return true;
}

void onDataReceive(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len != sizeof(ESPNowMessage))
  {
    Serial.printf("Invalid message size: %d (expected %d)\n", len, sizeof(ESPNowMessage));
    return;
  }

  ESPNowMessage msg;
  memcpy(&msg, data, sizeof(msg));

  // Filter by target board ID
  if (msg.targetBoardId != boardId)
  {
    return; // Not for us, ignore silently
  }

  Serial.printf("Received from Board %d: %s\n", msg.senderBoardId, msg.soundFile);

  // Validate and play
  if (validateMessage(&msg))
  {
    String filePath = "/" + String(msg.soundFile);
    playWAVFile(filePath.c_str());
  }
  else
  {
    Serial.println("Message validation failed");
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.printf("Send status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setupESPNow()
{
  Serial.println("Initializing ESP-NOW...");

  // Set device as WiFi Station
  WiFi.mode(WIFI_STA);

  // Print MAC address for reference
  Serial.printf("Board %d MAC Address: %s\n", boardId, WiFi.macAddress().c_str());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
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

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add broadcast peer");
    return;
  }

  Serial.println("Broadcast peer registered");
  Serial.printf("Board %d ready to send/receive messages\n", boardId);
}

void sendSoundCommand(uint8_t targetBoard, const char *soundFile)
{
  ESPNowMessage msg;
  msg.senderBoardId = boardId;
  msg.targetBoardId = targetBoard;
  strncpy(msg.soundFile, soundFile, sizeof(msg.soundFile) - 1);
  msg.soundFile[sizeof(msg.soundFile) - 1] = '\0';
  msg.timestamp = millis();
  msg.checksum = calculateChecksum(&msg);

  Serial.printf("Sending to Board %d: %s\n", targetBoard, soundFile);

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));

  if (result != ESP_OK)
  {
    Serial.printf("Send error: %s\n", esp_err_to_name(result));
  }
}

// Button handlers
void handleSingleButtonPress()
{
  if (isButtonPressed(&greenButton))
  {
    Serial.println("Green button pressed - playing static sound");
    String filePath = "/" + greenSound;
    playWAVFile(filePath.c_str());
  }
  else if (isButtonPressed(&blueButton))
  {
    Serial.println("Blue button pressed - playing static sound");
    String filePath = "/" + blueSound;
    playWAVFile(filePath.c_str());
  }
  else if (isButtonPressed(&yellowButton))
  {
    Serial.println("Yellow button pressed - playing static sound");
    String filePath = "/" + yellowSound;
    playWAVFile(filePath.c_str());
  }
}

void handleDualButtonPress()
{
  // Check if exactly 2 of the 3 buttons are pressed
  if (countPressedButtons() == 2)
  {
    Serial.println("Dual button press detected - playing random sound");
    String randomSound = getRandomSound();
    if (randomSound.length() > 0)
    {
      String filePath = "/" + randomSound;
      playWAVFile(filePath.c_str());
    }

    // Clear button states to prevent repeated triggers
    greenButton.pressed = false;
    blueButton.pressed = false;
    yellowButton.pressed = false;
  }
}

void handleRedButtonPress()
{
  if (isButtonPressed(&redButton))
  {
    Serial.println("Red button pressed - sending remote command");

    uint8_t targetBoard = getRandomBoardId();
    String randomSound = getRandomSound();

    if (randomSound.length() > 0)
    {
      sendSoundCommand(targetBoard, randomSound.c_str());
    }
    else
    {
      Serial.println("No sounds available to send");
    }
  }
}

void handleButtons()
{
  updateAllButtons();

  // Check for dual button press first (higher priority)
  if (countPressedButtons() >= 2)
  {
    handleDualButtonPress();
  }
  // Then check for red button (remote trigger)
  else if (redButton.currentState)
  {
    handleRedButtonPress();
  }
  // Finally check for single button presses
  else
  {
    handleSingleButtonPress();
  }
}

void playWAVFile(const char *filename)
{
  isPlaying = true;

  File audioFile = SD.open(filename);
  if (!audioFile)
  {
    Serial.printf("Failed to open: %s\n", filename);
    isPlaying = false;
    return;
  }

  Serial.printf("Playing: %s (%d bytes)\n", filename, audioFile.size());

  // Skip WAV header (44 bytes for standard WAV)
  if (String(filename).endsWith(".wav") || String(filename).endsWith(".WAV"))
  {
    audioFile.seek(44);
  }

  size_t bytesRead, bytesWritten;

  while (audioFile.available())
  {
    bytesRead = audioFile.read(audioBuffer, BUFFER_SIZE);

    if (bytesRead > 0)
    {
      processAudioBuffer(audioBuffer, processedBuffer, bytesRead);

      esp_err_t result = i2s_write(I2S_NUM, processedBuffer, bytesRead,
                                   &bytesWritten, pdMS_TO_TICKS(100));
      if (result != ESP_OK)
      {
        Serial.printf("I2S write error: %s\n", esp_err_to_name(result));
        break;
      }

      if (bytesWritten < bytesRead)
      {
        taskYIELD();
      }
    }
  }

  audioFile.close();
  isPlaying = false;
  Serial.println("Playback completed");
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== ESP32-C3 Sound Board ===");

  // Initialize buttons first (they don't conflict with SPI)
  initButtons();

  // Give the system time to stabilize before initializing SPI peripherals
  delay(500);

  // Initialize SD card (must be before I2S to avoid SPI conflicts)
  if (!initializeSDCard())
  {
    Serial.println("Cannot continue without SD card");
    Serial.println("Halting. Please fix SD card and reset board.");
    while (1)
      delay(1000);
  }

  // Show SD card info
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card: %lluMB\n", cardSize);

  // Load board ID from SD card
  if (!loadBoardId())
  {
    Serial.println("Cannot continue without board ID file");
    Serial.println("Please create 1.txt, 2.txt, 3.txt, 4.txt, or 5.txt on SD card");
    Serial.println("Halting. Please fix and reset board.");
    while (1)
      delay(1000);
  }

  Serial.printf("Board ID: %d\n", boardId);

  // Discover sound files
  discoverSoundFiles();

  // Assign sounds by index
  assignSoundsByIndex();

  if (greenSound.length() == 0 || blueSound.length() == 0 || yellowSound.length() == 0)
  {
    Serial.println("Cannot continue without at least 3 WAV files");
    Serial.println("Halting. Please add WAV files to SD card and reset board.");
    while (1)
      delay(1000);
  }

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

void loop()
{
  handleButtons();
  delay(10); // Small delay to prevent excessive CPU usage
}