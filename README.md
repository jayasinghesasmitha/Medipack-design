MediBox - Smart Medication Reminder with Environmental Monitoring
🚀 A smart IoT device that reminds users to take medication on time and monitors room temperature/humidity for optimal storage conditions.

📌 Overview
MediBox is an ESP32-based smart medication reminder that:
✅ Alarms users at preset times to take their medication.
✅ Allows snoozing or stopping alarms.
✅ Monitors temperature & humidity to ensure proper medicine storage.
✅ Features an OLED display, buzzer, and LED indicators for alerts.
✅ Supports WiFi (NTP) for automatic time synchronization.

🛠️ Tech Stack
Hardware Components
Microcontroller: ESP32

Display: SSD1306 OLED (128x64)

Sensors: DHT22 (Temperature & Humidity)

Input: Tactile buttons (UP, DOWN, OK, CANCEL)

Output: Buzzer (for alarms), LED (status indicator)

Software & Libraries
Arduino Framework (C++)

Libraries Used:

WiFi.h (WiFi connectivity)

Adafruit_GFX & Adafruit_SSD1306 (OLED display)

DHTesp (DHT22 sensor)

Wire.h (I2C communication)

🔧 Key Features
1. Medication Alarms
Set multiple alarms (up to 5).

Snooze functionality (5-minute delay).

Visual (OLED) + audio (buzzer) + LED alerts.

2. Environmental Monitoring
Temperature & humidity tracking (DHT22).

Alerts if conditions are unsuitable for medicine storage:

High Temp (>32°C) / Low Temp (<24°C)

High Humidity (>80%) / Low Humidity (<65%)

3. User Interface
Interactive menu (navigated via buttons).

Real-time clock (NTP-synced).

Customizable timezone (UTC offset).

4. Additional Functions
View/Delete alarms.

Enable/Disable all alarms.

Manual time setting (if offline).

📂 Project Structure
📁 Medipack-design/ 
├── 📜 README.md 
├── 📁 Project01 
 ├── 📁 src 
 ├── sketch.ino ( arduino code)
 ├── diagram.json (Wokwi simulation diagram)
 
1. Hardware Connections
Component	ESP32 Pin
OLED (SSD1306)	SDA=21, SCL=22
DHT22	GPIO 12
Buzzer	GPIO 5
LED	GPIO 15
Buttons (UP/DOWN/OK/CANCEL)	GPIO 33/35/32/34
2. Software Setup
Install Arduino IDE + ESP32 Board Support.

Add required libraries (Adafruit_GFX, SSD1306, DHTesp).

Upload the sketch (Sketch.ino).

Power the device (USB or 5V supply).

3. WiFi Configuration
Modify WiFi.begin("Your_SSID", "Your_Password") in setup().

Uses NTP (pool.ntp.org) for automatic time sync.

🚀 Usage
Set Alarms:

Navigate to "Set Alarm" in the menu.

Adjust hours/minutes with UP/DOWN, confirm with OK.

Snooze/Stop Alarms:

When alarm triggers:

OK = Snooze (5 min).

CANCEL = Stop alarm.

Monitor Environment:

Device automatically checks temp/humidity.

Alerts if conditions are out of range.

