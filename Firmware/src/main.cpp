#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <NTPClient.h>
#include <time.h>
#include <movingAvg.h>
#include <FastLED.h>

// User Memory - WiFi SSID, PSK, Clock Configuration bits.
Preferences preferences;

// AccessPoint Credentials. Default UserMem WiFi Value.
const char* APID = "NixieClock";
const char* APSK = "MinesBigger";
const char* GARBAGE_STRING = "C!pbujKY2#4HXbcm5dY!WJX#ns29ff#vEDWmbZ9^d!QfBW@o%Trfj&sPENuVe&sx";

// Global Variables
bool softAPActive = 0;
bool showIP = 0;

// Server Stuff
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);

// setting PWM properties
#define freq 2000
#define ledChannel 0
#define ledPWMResolution 8

// Pins used for segment displays
const int hours_pin[8] = {9, 11, 12, 10, 18, 16, 15, 17};
const int minutes_pin[8] = {13, 21, 33, 14, 38, 36, 35, 37};

// Additional Control Pins
const int ledPin = 41;       // Power pin for segment displays. Common Anode
#define NUM_LEDS 4
#define DATA_PIN 1
// const int decimalPin = 35;  // Decimal point which separates hours and minutes.
// Other Decimal Points on 36, 37, 38.

CRGB leds[NUM_LEDS];

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
  int hoursT = hours / 10;
  int hoursO = hours % 10;
  int minutesT = minutes / 10;
  int minutesO = minutes % 10;

  // Write binary coded decimal to each display.
  // Tens Place
  digitalWrite(hours_pin[4], (hoursT >> 0) & 1);  // A4
  digitalWrite(hours_pin[5], (hoursT >> 1) & 1);  // B4
  digitalWrite(hours_pin[6], (hoursT >> 2) & 1);  // C4
  digitalWrite(hours_pin[7], (hoursT >> 3) & 1);  // D4

  // Ones Place
  digitalWrite(hours_pin[0], (hoursO >> 0) & 1);   // A3
  digitalWrite(hours_pin[1], (hoursO >> 1) & 1);  // B3
  digitalWrite(hours_pin[2], (hoursO >> 2) & 1);  // C3
  digitalWrite(hours_pin[3], (hoursO >> 3) & 1);  // D3

  // Tens Place
  digitalWrite(minutes_pin[4], (minutesT >> 0) & 1);  // A2
  digitalWrite(minutes_pin[5], (minutesT >> 1) & 1);  // B2
  digitalWrite(minutes_pin[6], (minutesT >> 2) & 1);  // C2
  digitalWrite(minutes_pin[7], (minutesT >> 3) & 1);  // D2

  // Ones Place
  digitalWrite(minutes_pin[0], (minutesO >> 0) & 1);  // A1
  digitalWrite(minutes_pin[1], (minutesO >> 1) & 1);  // B1
  digitalWrite(minutes_pin[2], (minutesO >> 2) & 1);  // C1
  digitalWrite(minutes_pin[3], (minutesO >> 3) & 1);  // D1
}

// Flag the main loop to show the IP Address of the clock.
// static void IRAM_ATTR showIPFlag(){
//   showIP = 1;
// }

/// @brief Flashes the IP Address of the device on the main display.
/// @param localIP IPAddres object. Basically an array of 8 bit integers.
// void displayIP(IPAddress localIP) {
//   pinMode(38, OUTPUT);
//   // digitalWrite(decimalPin, 1);
//   digitalWrite(38, 0);

//   int octet0 = localIP[0];
//   int octet1 = localIP[1];
//   int octet2 = localIP[2];
//   int octet3 = localIP[3];

//   printTime(octet0 / 100, octet0 % 100);
//   delay(1500);
//   printTime(octet1 / 100, octet1 % 100);
//   delay(1500);
//   printTime(octet2 / 100, octet2 % 100);
//   delay(1500);
//   printTime(octet3 / 100, octet3 % 100);
//   delay(1500);

//   digitalWrite(decimalPin, 0);
//   pinMode(38, INPUT);
// }

/// @brief Initializes BH1730 Ambient Light Sensor over I2C.
void initLightSensor() {
  Wire.beginTransmission(0x29);
  Wire.write(0x80);
  Wire.write(0x3);
  Wire.endTransmission();
}

/// @brief Reads the ambient light bits inside the ambient light sensor
/// @return 8 bit integer mapped between the user setable min and max brightness values.
uint8_t readAmbientLightData(){
  Wire.beginTransmission(0x29);
  Wire.write(0b10010100);
  Wire.endTransmission();
  Wire.requestFrom(0x29, 2);

  // Read the data from the BH1730
  byte highByte = Wire.read();
  byte lowByte = Wire.read();

  // Combine the high and low bytes to get the ambient light level
  int lightLevel = (highByte << 8) | lowByte;

  // return the ambient light level
  return( map(lightLevel, 0, 65535, preferences.getInt("minBrightness", 50), preferences.getInt("maxBrightness", 255)) );
}

// Moving average to smooth out changes in the ambient light.
movingAvg ambientLight(10);

// Initial Setup Function
void setup() {
  // Serial for debug. Preferences for UserMem.
  Serial.begin(9600);
  Serial.println("Entered setup");
  preferences.begin("usermem");
  Serial.println("Begin EEPROM");
  SPIFFS.begin();

  // Begin LEDs
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  // leds[0] = CRGB::Purple;
  FastLED.show();

  setenv("TZ", preferences.getString("timezone", "UTC").c_str(), 1);
  tzset();
  ledcSetup(ledChannel, freq, ledPWMResolution);
  ledcAttachPin(ledPin, ledChannel);
  ledcWrite(ledChannel, 50);

  // Begin I2C on pins 34, 33.
  Wire.begin(5, 6);
  initLightSensor();
  ambientLight.begin();

  // Fill moving average with real data.
  // Prevents the clock from bouncing around in brightness.
  for (int i = 0; i < 10; i++) {
    ambientLight.reading(readAmbientLightData());
  }

  // Activate the decimal point as hour indicator.
  // pinMode(decimalPin, OUTPUT);
  // digitalWrite(decimalPin, 0);

  // Set the hours and minutes pins to output mode
  for (int i = 0; i < 8; i++) {
    pinMode(hours_pin[i], OUTPUT);
    pinMode(minutes_pin[i], OUTPUT);
  }
  
  // ---------- WiFi Section ----------
  // Access Point init
  Serial.println("Start Access Point");
  WiFi.softAPsetHostname("nixieclock");
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
    WiFi.setHostname("nixieclock");
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
      // displayIP(WiFi.localIP());
    }

    // Connection unsuccessful. Continue to main loop.
    // Your WiFi is slow, down, or you incorrectly configured your credentials if you're at this point.
    else{
      Serial.println("\nConnection Failed. Please connect to the webportal and enter valid information.");
    }
  }

  // Shows the center point for the clock. Splitter between hour and minute.
  // digitalWrite(decimalPin, 0);

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
        WiFi.setHostname("nixieclock");
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
    Serial.println("Request Received");

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

  // Use an interrupt to flag when the boot button has been pressed.
  // attachInterrupt(0, showIPFlag, FALLING);
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
    WiFi.softAPsetHostname("nixieclock");
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
  printTime(timeinfo->tm_hour, timeinfo->tm_min);

  // Adjust light intensity using a moving average
  if ((millis() % 25) == 0) {
    uint8_t ambiReading = ambientLight.reading(readAmbientLightData());
    // Serial.println(ambiReading);
    ledcWrite(ledChannel, ambiReading);
  }

  // Display IP if button flagged
  // if (showIP){
  //   displayIP(WiFi.localIP());
  //   showIP = 0;
  // }
}
