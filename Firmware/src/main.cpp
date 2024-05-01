#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <NTPClient.h>
#include <time.h>
#include <FastLED.h>

#define STROBE_PIN 38
#define LATCH_EN 39
#define CLOCK_PIN 36
#define DATA_PIN 35

// User Memory - WiFi SSID, PSK, Clock Configuration bits.
Preferences preferences;

CRGB leds[8];

// AccessPoint Credentials. Default UserMem WiFi Value.
const char* APID = "VFDClock";
const char* APSK = "VFDIV-12";
const char* GARBAGE_STRING = "C!pbujKY2#4HXbcm5dY!WJX#ns29ff#vEDWmbZ9^d!QfBW@o%Trfj&sPENuVe&sx";

// Global Variables
bool softAPActive = 0;

// Server Stuff
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);

// Define 7-segment display patterns for numbers 0-9
// BA GF ED C
const byte SEGMENT_PATTERNS[10] = {
  0b1101111, // 0
  0b1000001, // 1
  0b1110110, // 2
  0b1110011, // 3
  0b1011001, // 4
  0b0111011, // 5
  0b0111111, // 6
  0b1100001, // 7
  0b1111111, // 8
  0b1111011  // 9
};


/// @brief Prints the time to 4 segment displays
/// @param hours integer number 0-99
/// @param minutes integer number 0-99
void printTime(int hours, int minutes){
  bool timeFormat = preferences.getBool("timeFormat", 1);
  // Serial.println(timeFormat);
  if (!timeFormat) {
    hours = (hours % 12) ?: 12;
  }

  // Break up the two digit integers to single digits.
  int hoursTen = hours / 10;
  int hoursOne = hours % 10;
  int minutesTen = minutes / 10;
  int minutesOne = minutes % 10;

  uint32_t encodedTime = (SEGMENT_PATTERNS[hoursTen] << 22) + (SEGMENT_PATTERNS[hoursOne] << 15) + (SEGMENT_PATTERNS[minutesTen] << 8) + (SEGMENT_PATTERNS[minutesOne] << 1);

  // Drive strobe high. I think this is needed?
  digitalWrite(STROBE_PIN, 1);
  digitalWrite(LATCH_EN, 0);

  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer32(encodedTime);
  SPI.endTransaction();

  // Pulse latch
  digitalWrite(LATCH_EN, 1);
  delay(1);
  digitalWrite(LATCH_EN, 0);

  // Strobe pin low for normal operation.
  digitalWrite(STROBE_PIN, 0);
}


// Initial Setup Function
void setup() {
  // Serial for debug. Preferences for UserMem.
  Serial.begin(9600);
  Serial.println("Entered setup");
  preferences.begin("usermem");
  Serial.println("Begin EEPROM");
  SPIFFS.begin();

  // Config latch and strobe pins.
  pinMode(STROBE_PIN, OUTPUT);
  pinMode(LATCH_EN, OUTPUT);
  digitalWrite(STROBE_PIN, 1);
  digitalWrite(LATCH_EN, 0);

  // Config SPI on clock/data pins.
  SPI.begin(CLOCK_PIN, -1, DATA_PIN);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer32(0xFFFFFFFF);
  SPI.endTransaction();
  // Pulse latch to hold the new data
  digitalWrite(LATCH_EN, 1);
  delay(1);
  digitalWrite(LATCH_EN, 0);
  // Strobe low for normal oepration?
  digitalWrite(STROBE_PIN, 0);

  // config underglow.
  FastLED.addLeds<WS2812, 3, GRB>(leds, 8);
  fill_solid(leds, 8, CRGB::DarkOliveGreen);
  FastLED.show();

  setenv("TZ", preferences.getString("timezone", "EST5EDT").c_str(), 1);
  tzset();
  
  // ---------- WiFi Section ----------
  // Access Point init
  Serial.println("Start Access Point");
  WiFi.softAPsetHostname(APID);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APID, APSK);
  softAPActive = true;

  // Skip WiFi configuration if no credentials exist.
  if (preferences.getString("WiFiSSID", GARBAGE_STRING) == GARBAGE_STRING || preferences.getString("WiFiPSK", GARBAGE_STRING) == GARBAGE_STRING) {
    Serial.println("WiFi not configured. Skipping network connection.");
  }

  // WiFi Credentials exist. Configure WiFi and attempt to connect.
  else {
    Serial.println("WiFi Configured. Attempting Connection.");
   
    // Begin WiFi with the stored credentials.
    WiFi.setHostname("niceclock");
    WiFi.begin(preferences.getString("WiFiSSID", GARBAGE_STRING).c_str(), preferences.getString("WiFiPSK", GARBAGE_STRING).c_str());
    
    // Attempt to connect for ~5 seconds before continuing.
    while(!WiFi.isConnected() && millis() < 5000) {
      Serial.print(". ");
      delay(100);
    }
    Serial.println();

    // Connection Successful. Teardown and disable AP.
    if (WiFi.isConnected()){
      Serial.println("\nConnection Success. Tearing down AP.");
      WiFi.softAPdisconnect();
      WiFi.mode(WIFI_STA);
      softAPActive = false;
    }

    // Connection unsuccessful. Continue to main loop.
    // Your WiFi is slow, down, or you incorrectly configured your credentials if you're at this point.
    else{
      Serial.println("\nConnection Failed. Please connect to the webportal and enter valid information.");
    }
  }

  // Begin Time Keeping
  timeClient.begin();
  timeClient.update();

  // ---------- Webpage Configuration Section ----------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });

  // Route to load style.css file
  server.on("/milligram.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Loading BS CSS");
    request->send(SPIFFS, "/milligram.min.css", "text/css");
  });

  // Route to load TimeZone information.
  server.on("/zones.csv", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Loading zones");
    request->send(SPIFFS, "/zones.csv", "text/csv");
  });

  // Handle HTTP POST requests for the root page
  server.on("/updateWiFi", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Request Received");

    // Handling input data from the webpage. ALL EEPROM Values updated
    if (request->hasParam("ssid", true) && request->hasParam("psk", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String psk = request->getParam("psk", true)->value();
      if(ssid != ""){
        Serial.println("Updating WiFi Credentials to SSID: " + ssid);
        preferences.putString("WiFiSSID", ssid);
        preferences.putString("WiFiPSK", psk);

        // Begin WiFi with the stored credentials.
        WiFi.setHostname("niceclock");
        WiFi.begin(preferences.getString("WiFiSSID", GARBAGE_STRING).c_str(), preferences.getString("WiFiPSK", GARBAGE_STRING).c_str());
      }
    }

    // Done
    request->send(200);
  });

  // Toggle between 12h and 24h format.
  server.on("/updateTimeFormat", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Handling input data from the webpage
    if (request->hasArg("isChecked")) {
      String isChecked = request->arg("isChecked");
      if (isChecked == "true") {
        preferences.putBool("timeFormat", 0);
      } else {
        preferences.putBool("timeFormat", 1);
      }
    }

    request->send(200);
  });

  // Route to get configured time format (12|24)
  server.on("/getTimeFormat", HTTP_GET, [](AsyncWebServerRequest *request){
    String checkboxStateStr = preferences.getBool("timeFormat", 1) ? "false" : "true";
    request->send(200, "text/plain", checkboxStateStr);
  });

  // Route to get configured WiFi SSID
  server.on("/getWiFiSSID", HTTP_GET, [](AsyncWebServerRequest *request){
    String wiFiSSID = preferences.getString("WiFiSSID", "");
    request->send(200, "text/plain", wiFiSSID);
  });

  // Route to get configured Minimum Brightness
  server.on("/getMinBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
    String minBrightness = String(preferences.getInt("minBrightness", 10));
    request->send(200, "text/plain", minBrightness);
  });

  // Route to get configured Maximum Brightness
  server.on("/getMaxBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
    String maxBrightness = String(preferences.getInt("maxBrightness", 255));
    request->send(200, "text/plain", maxBrightness);
  });

  // Route to get update ESP-Side Brightness limits.
  server.on("/updateBrightness", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Request Received");

    // Handling input data from the webpage. ALL EEPROM Values updated
    if (request->hasParam("minBrightnessSlider", true) && request->hasParam("maxBrightnessSlider", true)) {
      int min = request->getParam("minBrightnessSlider", true)->value().toInt();
      int max = request->getParam("maxBrightnessSlider", true)->value().toInt();

      preferences.putInt("minBrightness", min);
      preferences.putInt("maxBrightness", max);
    }

    // Done
    request->send(200);
  });

  // Route to update timezone.
  server.on("/setTZ", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("setTZ Request Received");

    // Handling input data from the webpage
    if (request->hasArg("timezone")) {
      String timezone = request->arg("timezone");
      preferences.putString("timezone", timezone);
      configTzTime(timezone.c_str(), "pool.ntp.org");
      timeClient.update();
    }

    // Done
    request->send(200);
  });

  // Start the server
  server.begin();
}


void loop() {
  // Connection Successful. Teardown and disable AP.
  if(softAPActive && WiFi.isConnected()){
    Serial.println("Internet Connected. Tearing down AP.");
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_STA);
    softAPActive = false;
    timeClient.update();
  }

  // Lost connection to the internet. Re-Enable the AP to avoid getting stuck.
  if(!softAPActive && !WiFi.isConnected()) {
    Serial.println("Lost Internet. Restarting AP.");
    WiFi.softAPsetHostname("niceclock");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(APID, APSK);
    softAPActive = true;
  }

  // Reconnect to NTP To update time at midnight every night
  if(timeClient.getMinutes() == 0 && timeClient.getSeconds() == 0){
    timeClient.update();
    delay(1000);
  }

  // Base the time on epoch. This allows DST To work automatically.
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);

  // Print the timezone info to SSDs
  if (millis() % 500 == 0) {
    printTime(timeinfo->tm_hour, timeinfo->tm_min);
  }

}
