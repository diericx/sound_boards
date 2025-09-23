#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// SD card pin definitions for ESP32-C3
// Based on your connections:
// CLK -> D0 (GPIO2)
// DO -> D1 (GPIO3)
// DI -> D2 (GPIO4)
// CS -> D3 (GPIO5)
#define SD_CS_PIN 5   // D3
#define SD_MOSI_PIN 4 // D2 (DI)
#define SD_MISO_PIN 3 // D1 (DO)
#define SD_SCK_PIN 2  // D0 (CLK)

void listDirectory(File dir, int numTabs)
{
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // No more files
      break;
    }

    // Print tabs for directory structure
    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t');
    }

    Serial.print(entry.name());
    if (entry.isDirectory())
    {
      Serial.println("/");
      listDirectory(entry, numTabs + 1);
    }
    else
    {
      // Print file size
      Serial.print("\t\t");
      Serial.print(entry.size(), DEC);
      Serial.println(" bytes");
    }
    entry.close();
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10); // Wait for serial port to connect
  }

  Serial.println("Initializing SD card...");

  // Initialize SPI with custom pins
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  Serial.printf("Total space: %lluMB\n", totalBytes / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", usedBytes / (1024 * 1024));
  Serial.printf("Free space: %lluMB\n", (totalBytes - usedBytes) / (1024 * 1024));

  Serial.println("\nFiles found on the card:");
  Serial.println("Name\t\tSize");
  Serial.println("----\t\t----");

  File root = SD.open("/");
  listDirectory(root, 0);
  root.close();

  Serial.println("\nSD card listing complete!");
}

void loop()
{
  // Nothing to do here - we only list files once in setup()
  delay(1000);
}