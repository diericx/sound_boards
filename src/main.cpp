#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

// SD card pin definitions for ESP32-C3
#define SD_CS_PIN 5   // D3 -> CS
#define SD_MOSI_PIN 4 // D2 -> DI
#define SD_MISO_PIN 3 // D1 -> DO
#define SD_SCK_PIN 2  // D0 -> CLK

// I2S pins for MAX98357A (correct GPIO mapping for XIAO ESP32-C3)
#define I2S_DOUT 21 // D6 -> DIN (GPIO21, not GPIO6)
#define I2S_BCLK 20 // D7 -> BCLK (GPIO20, not GPIO7)
#define I2S_LRC 8   // D8 -> LRC (GPIO8)

// I2S configuration
#define I2S_NUM I2S_NUM_0
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define CHANNEL_FORMAT I2S_CHANNEL_FMT_RIGHT_LEFT
#define BUFFER_SIZE 1024

// Volume control (0.0 to 1.0, where 0.5 = 50% volume)
#define VOLUME_LEVEL 0.9 // Start with 30% volume to prevent clipping

uint8_t audioBuffer[BUFFER_SIZE];
int16_t processedBuffer[BUFFER_SIZE / 2]; // For 16-bit audio processing

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
      .dma_buf_count = 16, // Increased from 8 to 16 for better buffering
      .dma_buf_len = 128,  // Increased from 64 to 128 for smoother playback
      .use_apll = true,    // Use APLL for better clock accuracy
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

    // Apply volume control and limiting
    processedBuffer[i] = applyVolumeControl(sample, VOLUME_LEVEL);
  }
}

bool initializeSDCard()
{
  Serial.println("Initializing SD card...");

  // Initialize SPI with custom pins
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(100);

  // Try multiple initialization methods for reliability
  if (SD.begin(SD_CS_PIN, SPI, 4000000))
  {
    Serial.println("SD card initialized at 4MHz");
    return true;
  }
  else if (SD.begin(SD_CS_PIN))
  {
    Serial.println("SD card initialized at default speed");
    return true;
  }
  else if (SD.begin(SD_CS_PIN, SPI, 1000000))
  {
    Serial.println("SD card initialized at 1MHz");
    return true;
  }

  Serial.println("SD card initialization failed");
  return false;
}

void playWAVFile(const char *filename)
{
  File audioFile = SD.open(filename);
  if (!audioFile)
  {
    Serial.printf("Failed to open: %s\n", filename);
    return;
  }

  Serial.printf("Playing: %s (%d bytes)\n", filename, audioFile.size());
  Serial.printf("Volume level: %.1f%% (%.2f)\n", VOLUME_LEVEL * 100, VOLUME_LEVEL);
  Serial.println("Anti-crackling measures enabled:");
  Serial.println("- Soft limiting at ±28000 range");
  Serial.println("- Improved I2S buffering (16 buffers, 128 samples each)");
  Serial.println("- APLL clock for better timing accuracy");

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
      // Process audio data with volume control and limiting
      processAudioBuffer(audioBuffer, processedBuffer, bytesRead);

      // Write processed audio data with timeout to prevent blocking
      esp_err_t result = i2s_write(I2S_NUM, processedBuffer, bytesRead, &bytesWritten, pdMS_TO_TICKS(100));
      if (result != ESP_OK)
      {
        Serial.printf("I2S write error: %s\n", esp_err_to_name(result));
        break;
      }

      // Only yield if we have time, don't add unnecessary delays
      if (bytesWritten < bytesRead)
      {
        taskYIELD();
      }
    }
  }

  audioFile.close();
  Serial.println("Playback completed");
  Serial.println("\nIf you still hear crackling:");
  Serial.println("1. Check power supply - use USB wall adapter, not computer USB");
  Serial.println("2. Verify speaker impedance (4-8 ohms work best)");
  Serial.println("3. Check all wiring connections are secure");
  Serial.println("4. Try connecting MAX98357A GAIN pin to GND for lower gain");
}

void listFiles()
{
  Serial.println("\nFiles on SD card:");
  File root = SD.open("/");

  while (true)
  {
    File entry = root.openNextFile();
    if (!entry)
      break;

    if (!entry.isDirectory())
    {
      Serial.printf("  %s (%d bytes)\n", entry.name(), entry.size());
    }
    entry.close();
  }
  root.close();
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== ESP32-C3 Audio Player ===");

  // Initialize SD card
  if (!initializeSDCard())
  {
    Serial.println("Cannot continue without SD card");
    return;
  }

  // Show SD card info
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card: %lluMB\n", cardSize);

  listFiles();

  // Initialize I2S audio
  Serial.println("\nInitializing audio...");
  setupI2S();

  // Look for and play audio files
  Serial.println("Searching for audio files...");

  bool audioPlayed = false;
  File root = SD.open("/");

  while (true)
  {
    File entry = root.openNextFile();
    if (!entry)
      break;

    String filename = entry.name();
    if (filename.endsWith(".wav") || filename.endsWith(".WAV"))
    {
      entry.close();
      playWAVFile(("/" + filename).c_str());
      audioPlayed = true;
      break;
    }
    entry.close();
  }
  root.close();

  if (!audioPlayed)
  {
    if (SD.exists("/lovekills_30sec.m4a"))
    {
      Serial.println("Found M4A file - convert to WAV for playback:");
      Serial.println("ffmpeg -i lovekills_30sec.m4a -ar 44100 -ac 2 -f wav lovekills_30sec.wav");
    }
    else
    {
      Serial.println("No audio files found");
    }
  }

  Serial.println("Setup complete!");
}

void loop()
{
  delay(1000);
}