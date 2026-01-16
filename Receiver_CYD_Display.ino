#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h> 

// --- CONFIGURATION ---
const char* ssid = "FHogue";
const char* password = "fdigger1";
const int UDP_PORT = 4210;

// Location for Weather
const float LATITUDE = 39.24;
const float LONGITUDE = -94.42;

// --- PINS (CYD) ---
#define RED_LED 22
#define GREEN_LED 16
#define BLUE_LED 17

TFT_eSPI tft = TFT_eSPI(); 
WiFiUDP udp;
char packetBuffer[255];

String weatherStr = "Waiting for Weather...";
unsigned long lastWeatherTime = 0;
unsigned long lastPacketTime = 0;

// States: 0=Closed, 1=Open, 2=Lost Signal
int currentState = 2; 
int lastDrawnState = -1;

void setup() {
  Serial.begin(115200);
  
  // Pins
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  // Turn LEDs OFF (Active High on some boards, Low on others. Assuming Active Low here)
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(BLUE_LED, HIGH);

  // Screen
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  tft.println("Connected!");
  tft.println("Listening for Gate...");
  
  udp.begin(UDP_PORT);
  fetchWeather();
}

void loop() {
  // 1. Check for UDP Packets from Gate
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    
    String msg = String(packetBuffer);
    lastPacketTime = millis(); // Reset heartbeat timer
    
    if (msg == "OPEN") currentState = 1;
    else if (msg == "CLOSED") currentState = 0;
  }

  // 2. Check for Signal Loss (> 5 seconds silence)
  if (millis() - lastPacketTime > 5000) {
    currentState = 2; // Lost Signal
  }

  // 3. Update Screen if State Changed
  if (currentState != lastDrawnState) {
    drawScreen(currentState);
    lastDrawnState = currentState;
  }

  // 4. Update Weather every 15 mins
  if (millis() - lastWeatherTime > 900000) {
    fetchWeather();
    // Force redraw to update weather text
    drawScreen(currentState); 
  }
  
  delay(100);
}

void drawScreen(int state) {
  // State 0: Closed (Green)
  // State 1: Open (Red)
  // State 2: Lost Signal (Orange/Blue)

  if (state == 1) { 
    // --- OPEN ---
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("GATE OPEN!", 160, 120, 6);
    
    digitalWrite(RED_LED, LOW);   // On
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);
    
  } else if (state == 0) {
    // --- CLOSED ---
    tft.fillScreen(TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("GATE CLOSED", 160, 100, 6);
    
    tft.setTextFont(4);
    tft.drawString(weatherStr, 160, 180);

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW); // On
    digitalWrite(BLUE_LED, HIGH);
    
  } else {
    // --- LOST SIGNAL ---
    tft.fillScreen(TFT_ORANGE);
    tft.setTextColor(TFT_WHITE, TFT_ORANGE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("NO SIGNAL", 160, 100, 6);
    tft.setTextFont(4);
    tft.drawString("Check Gate Power", 160, 160);

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, LOW); // Blue On
  }
}

void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE) + "&longitude=" + String(LONGITUDE) + "&current_weather=true&temperature_unit=fahrenheit";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      float temp = doc["current_weather"]["temperature"];
      int code = doc["current_weather"]["weathercode"];
      
      String cond = "Clear";
      if (code > 3) cond = "Cloudy";
      if (code > 50) cond = "Rain";
      if (code > 70) cond = "Snow";
      
      weatherStr = String(temp, 1) + "F " + cond;
    }
    http.end();
    lastWeatherTime = millis();
  }
}
