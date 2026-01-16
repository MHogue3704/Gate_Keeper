#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <SD.h>
#include <WebServer.h>
#include <time.h>

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
#define BAT_ADC 34
#define TFT_BL 27
// SD Card Pins
#define SD_CS 5
#define SD_MOSI 23
#define SD_SCLK 18
#define SD_MISO 19

TFT_eSPI tft = TFT_eSPI(); 
WiFiUDP udp;
char packetBuffer[255];
WebServer server(80);
SPIClass sdSPI(VSPI); // Use VSPI for SD Card (Distinct from HSPI used by TFT)
bool sdAvailable = false;

String weatherStr = "Waiting for Weather...";
unsigned long lastWeatherTime = 0;
unsigned long lastPacketTime = 0;
unsigned long lastBatCheck = 0;
unsigned long stateEntryTime = 0;

// States: 0=Closed, 1=Open, 2=Lost Signal
int currentState = 2; 
int lastDrawnState = -1;

void fetchWeather();
void drawScreen(int state);
void drawBatteryHeader(uint16_t bgColor, uint16_t txtColor);
void setupSD();
void logChange(int newState);
void handleRoot();
void handleClearLog();
void handleStatus();
void handleLogRaw();

void setup() {
  Serial.begin(115200);
  
  // Pins
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(BAT_ADC, INPUT);

  // Turn LEDs OFF (Active High on some boards, Low on others. Assuming Active Low here)
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(BLUE_LED, HIGH);

  // Screen
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // Configure Backlight PWM (Channel 0, 5kHz, 8-bit)
  // Must be done AFTER tft.init() so the library doesn't override pin mode
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255); // Start Full Brightness

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
  
  // Setup Time for Logging
  configTime(-6 * 3600, 3600, "pool.ntp.org"); // CST/CDT approx
  
  setupSD();
  
  udp.begin(UDP_PORT);
  server.on("/", handleRoot);
  server.on("/clear", HTTP_POST, handleClearLog);
  server.on("/status", handleStatus);   // JSON API for Home Assistant
  server.on("/log.txt", handleLogRaw);  // Raw Log for Home Assistant
  server.begin();
  
  fetchWeather();
}

void loop() {
  server.handleClient();

  // 1. Check for UDP Packets from Gate
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    
    String msg = String(packetBuffer);
    msg.trim(); // Remove any whitespace/newlines
    Serial.println("Received: " + msg); // Debug print

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
    logChange(currentState);
    drawScreen(currentState);
    lastDrawnState = currentState;
    stateEntryTime = millis();
  }

  // Dimming Logic: Only dim if Closed (Green/State 0) and > 3 mins (180000ms)
  if (currentState == 0 && (millis() - stateEntryTime > 180000)) {
    ledcWrite(0, 10); // Dim to ~4% brightness
  } else {
    ledcWrite(0, 255); // Full brightness
  }

  // 4. Update Weather every 15 mins
  if (millis() - lastWeatherTime > 900000) {
    fetchWeather();
    // Force redraw to update weather text
    drawScreen(currentState); 
  }

  // 5. Redraw battery every 30s
  if (millis() - lastBatCheck > 30000) {
    drawScreen(currentState);
    lastBatCheck = millis();
  }
  
  delay(100);
}

void drawBatteryHeader(uint16_t bgColor, uint16_t txtColor) {
  uint16_t v = analogRead(BAT_ADC);
  // Calibration: CYD usually has 1/2 divider. 
  int pct = map(v, 1860, 2600, 0, 100);
  if (pct > 100) pct = 100;
  if (pct < 0) pct = 0;
  
  // Charging logic: if > 4.25V approx (ADC > 2630)
  bool charging = (v > 2630); 

  int w = 40;
  int h = 15;
  int x = (tft.width() / 2) - (w / 2); // Center the battery icon
  int y = 10;
  
  // Colors
  uint16_t barColor = TFT_GREEN;
  if (pct < 40) barColor = TFT_ORANGE;
  if (pct < 20) barColor = TFT_RED;
  
  // Changed Charging color to BLUE for better contrast on Green screens
  if (charging) barColor = TFT_BLUE; 

  // Draw Battery Background (Gray) so it stands out against any screen color
  tft.fillRect(x, y, w, h, TFT_LIGHTGREY);

  // Draw Outline
  tft.drawRect(x, y, w, h, txtColor);
  tft.fillRect(x + w, y + 4, 3, 7, txtColor); // Tip

  // Fill Bar
  float fillRatio = (float)pct / 100.0;
  int fillW = (int)((w - 4) * fillRatio);
  if (fillW > 0) tft.fillRect(x + 2, y + 2, fillW, h - 4, barColor);

  // Text pct
  tft.setTextColor(txtColor, bgColor);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(pct) + "%", x - 5, y + h/2 + 2, 2);

  // Status
  // IP Address at Bottom
  tft.setTextColor(txtColor, bgColor);
  tft.setTextDatum(BC_DATUM);
  tft.drawString(WiFi.localIP().toString(), x + w/2, tft.height() - 5, 2);

  if (charging) {
     tft.setTextDatum(ML_DATUM);
     tft.drawString("CHG", x + w + 8, y + h/2 + 2, 2);
  }
}

void drawScreen(int state) {
  Serial.println("Drawing State: " + String(state));
  
  int midX = tft.width() / 2;
  int midY = tft.height() / 2;

  // State 0: Closed (Green)
  // State 1: Open (Red)
  // State 2: Lost Signal (Orange/Blue)

  if (state == 1) { 
    // --- OPEN ---
    tft.fillScreen(TFT_RED);
    
    // Battery
    drawBatteryHeader(TFT_RED, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    // Use Font 4 if Font 6 isn't showing up
    tft.drawString("GATE OPEN!", midX, midY, 4); 
    
    digitalWrite(RED_LED, LOW);   // On
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);
    
  } else if (state == 0) {
    // --- CLOSED ---
    tft.fillScreen(TFT_GREEN);
    
    // Battery
    drawBatteryHeader(TFT_GREEN, TFT_BLACK);

    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("GATE CLOSED", midX, midY - 30, 4);
    
    tft.setTextFont(4);
    tft.drawString(weatherStr, midX, midY + 30);

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW); // On
    digitalWrite(BLUE_LED, HIGH);
    
  } else {
    // --- LOST SIGNAL ---
    tft.fillScreen(TFT_ORANGE);
    
    // Battery
    drawBatteryHeader(TFT_ORANGE, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_ORANGE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("NO SIGNAL", midX, midY - 30, 4);
    
    tft.setTextFont(4);
    tft.drawString("Check Gate Power", midX, midY + 30);

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, LOW); // Blue On
  }
}

void setupSD() {
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD Card Mount Failed");
    sdAvailable = false;
    return;
  }
  Serial.println("SD Card Initialized");
  sdAvailable = true;
}

void logChange(int newState) {
  if (!sdAvailable) return;

  File file = SD.open("/status_log.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open log file");
    return;
  }

  // Time
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    file.print("[NoTime] ");
  } else {
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S ", &timeinfo);
    file.print(strftime_buf);
  }

  String stateStr;
  if (newState == 0) stateStr = "CLOSED";
  else if (newState == 1) stateStr = "OPEN";
  else stateStr = "LOST SIGNAL";

  file.println("State: " + stateStr);
  file.close();
}

void handleClearLog() {
  if (sdAvailable) {
    SD.remove("/status_log.txt");
    // specific file recreation not strictly needed as append will create it
    File file = SD.open("/status_log.txt", FILE_WRITE);
    if(file) file.close();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStatus() {
  JsonDocument doc;
  
  // State
  String stateStr = "LOST SIGNAL";
  if (currentState == 0) stateStr = "CLOSED";
  else if (currentState == 1) stateStr = "OPEN";
  
  doc["state"] = stateStr;
  doc["state_code"] = currentState;
  
  // Battery
  uint16_t v = analogRead(BAT_ADC);
  int pct = map(v, 1860, 2600, 0, 100); 
  if(pct>100) pct=100; if(pct<0) pct=0;
  bool charging = (v > 2630);
  
  doc["battery_percent"] = pct;
  doc["battery_voltage_raw"] = v;
  doc["charging"] = charging;
  
  // Other
  doc["weather"] = weatherStr;
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime_ms"] = millis();
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleLogRaw() {
  if (!sdAvailable) {
    server.send(500, "text/plain", "SD Card not working");
    return;
  }
  File file = SD.open("/status_log.txt", "r");
  if (file) {
    server.streamFile(file, "text/plain");
    file.close();
  } else {
    server.send(404, "text/plain", "Log Empty");
  }
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  String html = "<!DOCTYPE html><html><head><title>Gate Keeper</title>";
  html += "<meta http-equiv='refresh' content='10'>"; 
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;text-align:center;background:#222;color:#fff;}";
  html += ".state{font-size:3em;padding:20px;border-radius:10px;margin:20px auto;width:80%;}";
  html += ".OPEN{background-color:red;color:white;} .CLOSED{background-color:green;color:black;} .LOST{background-color:orange;color:white;}";
  html += "button{padding:10px;font-size:1.2em;cursor:pointer;background:#444;color:white;border:none;border-radius:5px;}";
  html += "pre{text-align:left;background:#333;padding:10px;overflow:auto;height:400px;border:1px solid #555;}";
  html += "</style></head><body>";
  
  html += "<h1>Gate Keeper Status</h1>";
  
  String stateClass = "LOST";
  String stateText = "NO SIGNAL";
  if (currentState == 1) { stateClass = "OPEN"; stateText = "OPEN"; }
  else if (currentState == 0) { stateClass = "CLOSED"; stateText = "CLOSED"; }
  
  html += "<div class='state " + stateClass + "'>" + stateText + "</div>";
  html += "<p>Weather: " + weatherStr + "</p>";
  
  uint16_t v = analogRead(BAT_ADC);
  int pct = map(v, 1860, 2600, 0, 100); 
  if(pct>100) pct=100; if(pct<0) pct=0;
  bool charging = (v > 2630);
  html += "<p>Battery: " + String(pct) + "%" + (charging ? " (Charging)" : "") + "</p>";

  html += "<hr><h2>Event Log</h2>";
  html += "<form action='/clear' method='POST'><button onclick=\"return confirm('Clear log?')\">Clear Log</button></form>";
  html += "<pre>";
  
  server.sendContent(html);
  
  if (sdAvailable) {
    File file = SD.open("/status_log.txt", "r");
    if (file) {
      while (file.available()) {
        String line = file.readStringUntil('\n');
        server.sendContent(line + "\n");
      }
      file.close();
    } else {
       server.sendContent("Log empty.");
    }
  } else {
    server.sendContent("SD Card not mounted.");
  }
  
  server.sendContent("</pre></body></html>");
}

void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE) + "&longitude=" + String(LONGITUDE) + "&current_weather=true&temperature_unit=fahrenheit";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
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
