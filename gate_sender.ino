#include <WiFi.h>
#include <WiFiUdp.h>

// --- CONFIGURATION ---
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const int UDP_PORT = 4210;

// --- PINS (ESP32-C3 SuperMini) ---
#define SWITCH_PIN 3  // Connect N.C. wire here
#define LED_PIN 8     // Onboard LED (Active Low)

WiFiUDP udp;
// Broadcast IP (sends to everyone on local network)
IPAddress broadcastIP(255, 255, 255, 255);

bool lastState = false;
unsigned long lastSentTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Off

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    digitalWrite(LED_PIN, LOW); // Blink
    delay(200);
    digitalWrite(LED_PIN, HIGH);
  }
  
  digitalWrite(LED_PIN, LOW); // LED On = Connected
}

void loop() {
  // Read Switch (0 = Closed/Magnet Near, 1 = Open)
  bool isOpen = (digitalRead(SWITCH_PIN) == HIGH);

  // Send packet if state changed OR every 1 second (Heartbeat)
  if (isOpen != lastState || millis() - lastSentTime > 1000) {
    sendPacket(isOpen);
    lastState = isOpen;
    lastSentTime = millis();
  }
  
  delay(100);
}

void sendPacket(bool isOpen) {
  udp.beginPacket(broadcastIP, UDP_PORT);
  if (isOpen) {
    udp.print("OPEN");
  } else {
    udp.print("CLOSED");
  }
  udp.endPacket();
  
  // Blink LED briefly to show activity
  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
}
