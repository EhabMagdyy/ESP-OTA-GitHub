/********************************************************************************************************************
*****************************    Author  : Ehab Magdy Abdullah                      *********************************
*****************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *********************************
*****************************    Youtube : https://www.youtube.com/@EhabMagdyy      *********************************
*********************************************************************************************************************/

#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Updater.h>
#include <LittleFS.h>
// File path to save the downloaded file
#define SPIFFS_FilePath           "/update.bin"
#endif
#include <ArduinoJson.h>

// Wi-Fi credentials
#define SSID                      "SSID"
#define PASSWORD                  "PASSWORD"

// GitHub credentials
#define GitHubJsonFile            "GitHubJsonFile"
#define GitHubPAT                 "GitHubPAT"

// Current firmware version
const String CURRENT_FIRMWARE_VERSION = "1.0";

// Variables to store JSON data
String firmwareVersion;
String updateFileUrl;

void setup()
{
  Serial.begin(115200);

#ifdef ESP8266
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed. Attempting to format...");
    if (LittleFS.format()) {
      Serial.println("LittleFS formatted successfully.");
      if (!LittleFS.begin()) {
        Serial.println("Failed to mount after formatting. Halt.");
        while (1); // Stop here if formatting fails
      }
    } else {
      Serial.println("Formatting failed. Halt.");
      while (1);
    }
  } else {
    Serial.println("LittleFS mounted successfully");
  }
#endif

  ConnectToWiFi();
}

void loop()
{
  Serial.println("Current Version: " + CURRENT_FIRMWARE_VERSION);
  CheckForNewUpdate();
  delay(5000);
}

void ConnectToWiFi()
{
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

// Download the json file from GitHub & Extracts update data (version,updateFileURL)
void CheckForNewUpdate()
{
  HTTPClient http;

#ifdef ESP32
  http.begin(GitHubJsonFile);
  http.addHeader("Authorization", "Bearer " GitHubPAT);
#elif ESP8266
  WiFiClientSecure client;
  client.setInsecure();
  http.begin(client, GitHubJsonFile);
  http.addHeader("Authorization", "Bearer " GitHubPAT);
#endif

  Serial.println("Downloading JSON file...");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("JSON payload:");
      Serial.println(payload);

      // Parse JSON
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        return;
      }

      // Extract data from JSON
      firmwareVersion = doc["version"].as<String>();
      updateFileUrl = doc["update_file_url"].as<String>();

      Serial.println("Parsed data:");
      Serial.printf("Firmware Version: %s\n", firmwareVersion.c_str());
      Serial.printf("Update File URL: %s\n", updateFileUrl.c_str());

      double currentVersionNum = CURRENT_FIRMWARE_VERSION.toDouble();
      double newVersionNum = firmwareVersion.toDouble();

      if(newVersionNum > currentVersionNum)
      {
        #ifdef ESP32
        ESP32UpdateFirwmare(updateFileUrl);
        #elif ESP8266
        ESP8266UpdateFirwmare(updateFileUrl);
        #endif
      }
      else{ Serial.print("Application is up to data!"); }

    }
    else {
      Serial.printf("Failed to download JSON file, HTTP error code: %d\n", httpCode);
      Serial.printf("Error message: %s\n", http.errorToString(httpCode).c_str());
    }

  http.end();
}

#ifdef ESP32
void ESP32UpdateFirwmare(String updateFileUrl)
{
  HTTPClient http;

  http.begin(updateFileUrl);
  http.addHeader("Authorization", "Bearer " GitHubPAT);

  Serial.println("\nDownloading update file...");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    // reference to the data stream of the http response
    WiFiClient& client = http.getStream();
    uint32_t firmwareSize = http.getSize();
    Serial.print("Firmware Size: ");
    Serial.println(firmwareSize);

    if (Update.begin(firmwareSize))
    {
      Serial.println("Performing OTA Update...");
      size_t written = Update.writeStream(client);

      if (Update.size() == written)
      {
        Serial.println("Update successfully completed. Rebooting...");

        if (Update.end())
        {
          Serial.println("Rebooting...");
          ESP.restart();
        } 
        else 
        {
          Serial.print("Update failed: ");
          Serial.println(Update.errorString());
        }
      }
      else{ Serial.println("Not enough space for OTA."); }
    } 
    else{ Serial.println("Failed to begin OTA update."); }
  }

  http.end();
}
#endif

#ifdef ESP8266
void ESP8266UpdateFirwmare(String updateFileUrl)
{
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.begin(client, updateFileUrl);
  http.addHeader("Authorization", "Bearer " GitHubPAT);

  Serial.println("\nDownloading update file...");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    File file = LittleFS.open(SPIFFS_FilePath, "w");
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }

    http.writeToStream(&file);
    file.close();
    Serial.println("File downloaded and saved to File System");
    
  } else {
    Serial.printf("Failed to download file, HTTP error code: %d\n", httpCode);
    Serial.printf("Error message: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  File file = LittleFS.open(SPIFFS_FilePath, "r");
  if (!file) {
    Serial.println("Failed to open file for update");
    return;
  }

  size_t fileSize = file.size();
  Serial.printf("Starting OTA update from file: %s, size: %d bytes\n", SPIFFS_FilePath, fileSize);

  if (Update.begin(fileSize)) {
    size_t written = Update.writeStream(file);
    if (written == fileSize) {
      Serial.println("Update successfully written.");
    } else {
      Serial.printf("Update failed. Written only %d/%d bytes\n", written, fileSize);
    }

    if (Update.end()) {
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting...");
        ESP.restart();
      } else {
        Serial.println("Update not finished. Something went wrong.");
      }
    } else {
      Serial.println("Update failed");
    }
  } else {
    Serial.println("Not enough space to begin OTA");
  }

  file.close();
}
#endif