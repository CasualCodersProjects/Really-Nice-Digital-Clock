#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// User Memory - WiFi SSID, PSK, Clock Configuration bits.
Preferences preferences;

// AccessPoint Credentials. Default UserMem WiFi Value.
const char* APID = "studioCounter";
const char* APSK = "MinesBigger";
const char* GARBAGE_STRING = "C!pbujKY2#4HXbcm5dY!WJX#ns29ff#vEDWmbZ9^d!QfBW@o%Trfj&sPENuVe&sx";

// Global Variables
bool softAPActive = 0;
bool showIP = 0;

// Server Stuff
AsyncWebServer server(80);

// Pins used for segment displays
const int hours_pin[8] = {9, 8, 3, 14, 13, 10, 11, 12};
const int minutes_pin[8] = {7, 4, 5, 6, 18, 15, 16, 17};

// Additional Control Pins
const int ledPin = 1;       // Power pin for segment displays. Common Anode
const int decimalPin = 35;  // Decimal point which separates hours and minutes.
// Other Decimal Points on 36, 37, 38.

// TimeKeeping
int hours = 0;
int minutes = 0;

extern volatile unsigned long timer0_millis;

// // to reset millis:
// noInterrupts ();
// timer0_millis = 0;
// interrupts ();

/// @brief Prints the time to 4 segment displays
/// @param hours integer number 0-99
/// @param minutes integer number 0-99
void printTime(int hours, int minutes){
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
void displayIP(IPAddress localIP) {
  pinMode(38, OUTPUT);
  digitalWrite(decimalPin, 1);
  digitalWrite(38, 0);

  int octet0 = localIP[0];
  int octet1 = localIP[1];
  int octet2 = localIP[2];
  int octet3 = localIP[3];

  printTime(octet0 / 100, octet0 % 100);
  delay(1500);
  printTime(octet1 / 100, octet1 % 100);
  delay(1500);
  printTime(octet2 / 100, octet2 % 100);
  delay(1500);
  printTime(octet3 / 100, octet3 % 100);
  delay(1500);

  digitalWrite(decimalPin, 0);
  pinMode(38, INPUT);
}

// Initial Setup Function
void setup() {
  // Serial for debug. Preferences for UserMem.
  Serial.begin(9600);
  Serial.println("Entered setup");
  preferences.begin("usermem");
  Serial.println("Begin EEPROM");
  SPIFFS.begin();

  setenv("TZ", preferences.getString("timezone", "UTC").c_str(), 1);
  tzset();

  // Set Backlight on fullbright. For studio use. No need for ambient light.
  pinMode(OUTPUT, ledPin);
  digitalWrite(ledPin, 1);

  // Activate the decimal point as hour indicator.
  pinMode(decimalPin, OUTPUT);
  digitalWrite(decimalPin, 0);

  // Set the hours and minutes pins to output mode
  for (int i = 0; i < 8; i++) {
    pinMode(hours_pin[i], OUTPUT);
    pinMode(minutes_pin[i], OUTPUT);
  }
  
  // ---------- WiFi Section ----------
  // Access Point init
  Serial.println("Start Access Point");
  WiFi.softAPsetHostname("studioCounter");
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
    WiFi.setHostname("studioCounter");
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
      displayIP(WiFi.localIP());
    }

    // Connection unsuccessful. Continue to main loop.
    // Your WiFi is slow, down, or you incorrectly configured your credentials if you're at this point.
    else{
      Serial.println("\nConnection Failed. Please connect to the webportal and enter valid information.");
    }
  }

  // Shows the center point for the clock. Splitter between hour and minute.
  digitalWrite(decimalPin, 0);

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
        WiFi.setHostname("studioCounter");
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
  }

  // Lost connection to the internet. Re-Enable the AP to avoid getting stuck.
  if(!softAPActive && !WiFi.isConnected()) {
    Serial.println("Lost Internet. Restarting AP.");
    WiFi.softAPsetHostname("studioCounter");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(APID, APSK);
    softAPActive = true;
  }

  // Print the timezone info to SSDs
  long timeBaseSeconds = millis() / 1000;
  printTime(timeBaseSeconds / 60 , timeBaseSeconds % 60);

  // Display IP if button flagged
  if (showIP){
    displayIP(WiFi.localIP());
    showIP = 0;
  }
}
