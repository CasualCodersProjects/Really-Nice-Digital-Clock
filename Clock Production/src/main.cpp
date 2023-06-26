#include <Arduino.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
// #include <WebServer.h>
#include "index.h"

// WiFi credentials
const char *ssid = "Znet";
const char *passwd = "Znet2017";

// Timezone
const int TIMEZONE = -5;

// WiFi for NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);

// the number of the LED pin
const int ledPin = 1;  // 16 corresponds to GPIO16
const int decimalPin = 35;

// setting PWM properties
const int freq = 2000;
const int ledChannel = 0;
const int resolution = 8;

// Pins used for segment displays
const int minutes_pin[8] = {9, 8, 3, 14, 13, 10, 11, 12};
const int hours_pin[8] = {7, 4, 5, 6, 18, 15, 16, 17};


void printTime(int hours, int minutes){
  int hoursT = hours / 10;
  int hoursO = hours % 10;
  int minutesT = minutes / 10;
  int minutesO = minutes % 10;

  digitalWrite(13, (hoursT >> 0) & 1);  // A4
  digitalWrite(10, (hoursT >> 1) & 1);  // B4
  digitalWrite(11, (hoursT >> 2) & 1);  // C4
  digitalWrite(12, (hoursT >> 3) & 1);  // D4

  digitalWrite(9, (hoursO >> 0) & 1);   // A3
  digitalWrite(8, (hoursO >> 1) & 1);  // B3
  digitalWrite(3, (hoursO >> 2) & 1);  // C3
  digitalWrite(14, (hoursO >> 3) & 1);  // D3

  digitalWrite(18, (minutesT >> 0) & 1);  // A2
  digitalWrite(15, (minutesT >> 1) & 1);  // B2
  digitalWrite(16, (minutesT >> 2) & 1);  // C2
  digitalWrite(17, (minutesT >> 3) & 1);  // D2

  digitalWrite(7, (minutesO >> 0) & 1);  // A1
  digitalWrite(4, (minutesO >> 1) & 1);  // B1
  digitalWrite(5, (minutesO >> 2) & 1);  // C1
  digitalWrite(6, (minutesO >> 3) & 1);  // D1
}

void initLightSensor() {
  Wire.beginTransmission(0x29);
  Wire.write(0x80);
  Wire.write(0x3);
  Wire.endTransmission();
}

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
  return( map(lightLevel, 10, 65535, 1, 255) );
}

void setup() {
  setenv("TZ", "UTC", 1);
  tzset();
  ledcSetup(0, 2000, 8);
  ledcAttachPin(ledPin, 0);
  ledcWrite(0,4);

  // Begin I2C on pins 39, 40.
  Wire.begin(33, 21);
  initLightSensor();

  // Activate the decimal point as hour indicator.
  pinMode(decimalPin, OUTPUT);
  digitalWrite(decimalPin, 0);

  // Set the hours and minutes pins to output mode
  for (int i = 0; i < 8; i++) {
    pinMode(hours_pin[i], OUTPUT);
    pinMode(minutes_pin[i], OUTPUT);
  }

  // Init Wifi. Blink Decimal point while Waiting on Connection.
  bool toggle = 0;
  WiFi.setHostname("niceclock");
  WiFi.begin(ssid, passwd);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 100 );
    digitalWrite(decimalPin, toggle);
    toggle = !toggle;
  }
  WiFi.setHostname("niceclock");

  digitalWrite(decimalPin, 0);

  // Begin Time Keeping
  timeClient.begin();
  timeClient.update();

  // Set up the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", HTML);
  });

  // Handle a wifi change
  server.on("/change-wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = request->arg("ssid");
    String password = request->arg("password");
    WiFi.begin(ssid.c_str(), password.c_str());
    request->send(200, "text/plain", "WiFi connection changed");
  });

  server.on("/setTZ", HTTP_POST, [](AsyncWebServerRequest *request){
    String timezone = request->arg("timezone");
    setenv("TZ", timezone.c_str(), 1);
    tzset();
  });
  server.begin();
  Serial.println("Webserver Started");
}

void loop() {
  // Reconnect to NTP To update time at midnight every night
  if(timeClient.getHours() == 0 && timeClient.getMinutes() == 0 && timeClient.getSeconds() == 0){
    timeClient.update();
    delay(1000);
  }
  // Base the time on epoch. This allows DST To work automatically.
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);

  // Print the timezone info to SSDs
  printTime(timeinfo->tm_hour, timeinfo->tm_min);

  // Adjust light intensity.
  ledcWrite(0, readAmbientLightData());
}