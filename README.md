Wireless Gate Monitor (UDP Edition)

This system uses two boards communicating over your home Wi-Fi:

Sender (Gate): An ESP32-C3 SuperMini wired to the magnetic switch.

Receiver (House): The ESP32-CYD (Cheap Yellow Display) showing status & weather.



Hardware & Wiring


1. The Sender (At the Gate)

Board: ESP32-C3 SuperMini

Power: USB Power Bank or Hardwired 5V.

Switch Wiring:

COM (White): GND

N.C. (Red): GPIO 3

N.O.: Disconnected


2. The Receiver (In the House)

Board: ESP32-3248S035 (CYD)

Wiring: None! (Just plug in USB-C power).


Installation Steps

Configure Libraries:

Install TFT_eSPI and ArduinoJson libraries in Arduino IDE.

CRITICAL: Copy the code from User_Setup.h (below) into your computer's Documents/Arduino/libraries/TFT_eSPI/User_Setup.h file.

Upload Receiver Code:

Open Receiver_CYD_Display.ino.

Edit the ssid and password lines.

Select Board: ESP32 Dev Module.

Upload to the Screen Board.

Upload Sender Code:

Open Sender_ESP32C3.ino.

Edit the ssid and password lines (Must be same network as Receiver).

Select Board: ESP32C3 Dev Module.

Upload to the Small C3 Board.

How it Works

The Sender connects to Wi-Fi and broadcasts a "Heartbeat" packet every second.

The Receiver listens for these packets.

Green Screen: Gate Closed.

Red Screen: Gate Open.

Orange Screen ("NO SIGNAL"): If the Receiver hasn't heard from the Gate in 5 seconds (Power loss or Wi-Fi dro
