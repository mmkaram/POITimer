#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal.h>
#include <time.h>
#include "secrets.h"

// LCD pins: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(23, 22, 21, 19, 18, 5);

// WiFi / TomTom config
const bool DEBUG = true;

// NTP server for time sync
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -8 * 3600;  // PST (UTC-8)
const int daylightOffset_sec = 3600;   // +1 hour for PDT

int currentMinutes = 0;
unsigned long lastUpdate = 0;

// TomTom

int getTravelMinutes() {
  if (WiFi.status() != WL_CONNECTED) {
    if (DEBUG) Serial.println("WiFi not connected");
    return 0;
  }

  HTTPClient http;
  String url =
    "https://api.tomtom.com/routing/1/calculateRoute/" +
    homeCoords + ":" + clubCoords +
    "/json?key=" + String(tomtomApiKey) +
    "&travelMode=car&traffic=true&routeType=fastest&departAt=now";

  if (DEBUG) Serial.println("TomTom URL:");
  if (DEBUG) Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();

  if (DEBUG) Serial.printf("HTTP response code: %d\n", httpCode);

  if (httpCode != 200) {
    http.end();
    return 0;
  }

  String payload = http.getString();
  http.end();

  if (DEBUG) {
    Serial.println("Payload snippet:");
    Serial.println(payload.substring(0, 200));
  }

  int pos = payload.indexOf("\"travelTimeInSeconds\":");
  if (pos < 0) {
    if (DEBUG) Serial.println("travelTimeInSeconds not found");
    return 0;
  }

  int start = payload.indexOf(':', pos) + 1;
  int end = payload.indexOf(',', start);
  int seconds = payload.substring(start, end).toInt();

  int minutes = seconds / 60;

  if (DEBUG) {
    Serial.printf(
      "Parsed travel time: %d seconds = %d minutes\n",
      seconds,
      minutes
    );
  }

  return minutes;
}

// Time Helpers

String getETATime(int minutesToAdd) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "??:??";
  }

  // Add travel time
  time_t now = mktime(&timeinfo);
  now += minutesToAdd * 60;
  struct tm* etaTime = localtime(&now);

  // Format as 12-hour time with AM/PM
  int hour = etaTime->tm_hour;
  bool isPM = (hour >= 12);
  if (hour > 12) hour -= 12;
  if (hour == 0) hour = 12;

  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%d:%02d%s", 
           hour, etaTime->tm_min, isPM ? "p" : "a");
  
  return String(buffer);
}

// execution

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Booting ESP32 with LCD...");

  // Initialize LCD
  delay(1000);
  lcd.begin(16, 2);
  delay(500);
  lcd.clear();
  delay(500);

  // Show startup message
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for NTP time sync...");
  
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println("\nTime synchronized");

  // Show connected message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(2000);

  lcd.clear();
  
  lastUpdate = millis() - 60000UL;
}

void loop() {
  unsigned long now = millis();

  // Update every 60 seconds
  if (now - lastUpdate >= 60000UL) {
    currentMinutes = getTravelMinutes();
    Serial.printf("Updated travel time: %d minutes\n", currentMinutes);

    // Update LCD display
    lcd.clear();
    lcd.setCursor(0, 0);

    if (currentMinutes > 0) {
      String eta = getETATime(currentMinutes);
      
      // Format: "Club: ETA 3:45p"
      lcd.print("Club: ETA ");
      lcd.print(eta);
      
      // Show minutes on same line if space allows
      lcd.setCursor(0, 1);
      lcd.print(currentMinutes);
      lcd.print(" min");
    } else {
      lcd.print("Updating...");
    }

    // set the timer for next update (wait 60 seconds)
    lastUpdate = now;
  }

  delay(100); // Small delay to prevent tight looping
}
