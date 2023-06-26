#include <Arduino.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>

// WiFi credentials
const char *ssid = "SSID";
const char *passwd = "PASSWORD";

// Timezone
const int TIMEZONE = -5;

// WiFi for NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);

// the number of the LED pin
const int ledPin = 38;  // 16 corresponds to GPIO16

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

// Pins used for segment displays
const int minutes_pin[8] = {5, 6, 7, 8, 1, 2, 3, 4};
const int hours_pin[8] = {13, 14, 15, 16, 9, 10, 11, 12};


void printTime(int hours, int minutes){
  int hoursT = hours / 10;
  int hoursO = hours % 10;
  int minutesT = minutes / 10;
  int minutesO = minutes % 10;

  digitalWrite(13, (hoursT >> 0) & 1);  // A
  digitalWrite(14, (hoursT >> 1) & 1);  // B
  digitalWrite(15, (hoursT >> 2) & 1);  // C
  digitalWrite(16, (hoursT >> 3) & 1);  // D

  digitalWrite(9, (hoursO >> 0) & 1);   // A
  digitalWrite(10, (hoursO >> 1) & 1);  // B
  digitalWrite(11, (hoursO >> 2) & 1);  // C
  digitalWrite(12, (hoursO >> 3) & 1);  // D

  digitalWrite(5, (minutesT >> 0) & 1);  // A
  digitalWrite(6, (minutesT >> 1) & 1);  // B
  digitalWrite(7, (minutesT >> 2) & 1);  // C
  digitalWrite(8, (minutesT >> 3) & 1);  // D

  digitalWrite(1, (minutesO >> 0) & 1);  // A
  digitalWrite(2, (minutesO >> 1) & 1);  // B
  digitalWrite(3, (minutesO >> 2) & 1);  // C
  digitalWrite(4, (minutesO >> 3) & 1);  // D
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
  setenv("TZ", "EST5EDT", 1);
  tzset();
  ledcSetup(0, 2000, 8);
  ledcAttachPin(38, 0);
  ledcWrite(0,4);

  // Begin I2C on pins 39, 40.
  Wire.begin(40, 39);
  initLightSensor();

  // Activate the decimal point as hour indicator.
  pinMode(35, OUTPUT);
  digitalWrite(35, 0);

  // Set the hours and minutes pins to output mode
  for (int i = 0; i < 8; i++) {
    pinMode(hours_pin[i], OUTPUT);
    pinMode(minutes_pin[i], OUTPUT);
  }

  // Init Wifi. Blink Decimal point while Waiting on Connection.
  bool toggle = 0;
  WiFi.setHostname("NiceClock");
  WiFi.begin(ssid, passwd);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 100 );
    digitalWrite(35, toggle);
    toggle = !toggle;
  }
  digitalWrite(35, 0);

  // Begin Time Keeping
  timeClient.begin();
  timeClient.update();

    // Set up the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body>"
      "<form method='post' action='/change-wifi'>"
      "<label for='ssid'>WiFi SSID:</label><br>"
      "<input type='text' id='ssid' name='ssid'><br>"
      "<label for='password'>WiFi Password:</label><br>"
      "<input type='password' id='password' name='password'><br><br>"
      "<input type='submit' value='Submit'>"
      "</form>"
      "</body></html>");
  });

  // Handle a wifi change
  server.on("/change-wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = request->arg("ssid");
    String password = request->arg("password");
    WiFi.begin(ssid.c_str(), password.c_str());
    request->send(200, "text/plain", "WiFi connection changed");
  });

  server.begin();
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