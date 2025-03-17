// Include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

// Define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
int utc_offset = 0;

// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
int n_alarms = 2;
int alarm_hours[] = {0, 1};
int alarm_minutes[] = {1, 10};
bool alarm_triggered[] = {false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Disable Alarm", "5 - Set Time Zone"};


void setup() {
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(PB_CANCEL, INPUT);
    pinMode(PB_OK, INPUT);
    pinMode(PB_DOWN, INPUT);
    pinMode(PB_UP, INPUT);

    dhtSensor.setup(DHTPIN, DHTesp::DHT22);
    Serial.begin(9600);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    display.display();
    delay(500);

    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        display.clearDisplay();
        print_line("Connecting to WiFi", 0, 0, 2);
    }

    display.clearDisplay();
    print_line("Connected to WiFi", 0, 0, 2);

    configTime(utc_offset * 3600, 0, NTP_SERVER);  // Apply the offset dynamically

    display.clearDisplay();
    print_line("Welcome to Medibox!", 10, 20, 2);
    delay(500);
    display.clearDisplay();
}


void loop() {
  // put your main code here, to run repeatedly:
  update_time_with_check_alarm();
  if(digitalRead(PB_OK) == LOW) {
    delay(500);
    go_to_menu();
  }
  check_temp();
}

void print_line(String text, int column, int row, int text_size){

  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row); // (column, row)
  display.println(text);

  display.display();
}

void print_time_now(void) {
    display.clearDisplay();
    print_line(String(days), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(hours), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(minutes), 60, 0, 2);
    print_line(":", 80, 0, 2);
    print_line(String(seconds), 90, 0, 2);
}

void update_time() {
    struct tm timeinfo;
    getLocalTime(&timeinfo);

    char timeHour[3];
    strftime(timeHour, 3, "%H", &timeinfo);
    hours = atoi(timeHour);

    char timeMinute[3];
    strftime(timeMinute, 3, "%M", &timeinfo);
    minutes = atoi(timeMinute);

    char timeSecond[3];
    strftime(timeSecond, 3, "%S", &timeinfo);
    seconds = atoi(timeSecond);

    char timeDay[3];
    strftime(timeDay, 3, "%d", &timeinfo);
    days = atoi(timeDay);

    // Display time on OLED
    print_time_now();
}

void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);

  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  // Ring the buzzer
  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
      for (int i = 0; i < n_notes; i++) {
          if (digitalRead(PB_CANCEL) == LOW) {
              delay(200);
              break_happened = true;
              break;
          }
          tone(BUZZER, notes[i]);
          delay(500);
          noTone(BUZZER);
          delay(2);
      }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
      for (int i = 0; i < n_alarms; i++) {
          if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
              ring_alarm();
              alarm_triggered[i] = true;
          }
      }
  }
}

int wait_for_button_press() {
    while (true) {
        if (digitalRead(PB_UP) == LOW) {
            delay(200);
            return PB_UP;
        }

        else if (digitalRead(PB_DOWN) == LOW) {
            delay(200);
            return PB_DOWN;
        }

        else if (digitalRead(PB_OK) == LOW) {
            delay(200);
            return PB_OK;
        }

        else if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            return PB_CANCEL;
        }
        update_time();
    }
}

void go_to_menu() {
    while (digitalRead(PB_CANCEL) == HIGH) {
        display.clearDisplay();
        print_line(modes[current_mode], 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            current_mode = (current_mode + 1) % max_modes;
        } else if (pressed == PB_DOWN) {
            current_mode = (current_mode - 1 + max_modes) % max_modes;
        } else if (pressed == PB_OK) {
            run_mode(current_mode);
            break;
        }
    }
}

void set_time() {
    
    int temp_hour = hours;
    while (true) {
        display.clearDisplay();
        print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }

        else if (pressed == PB_DOWN) {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0) {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK) {
            delay(200);
            hours = temp_hour;
            break;
        }

        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }

    int temp_minute = minutes;
    while (true) {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }

        else if (pressed == PB_DOWN) {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0) {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK) {
            delay(200);
            minutes = temp_minute;
            break;
        }

        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }
    display.clearDisplay();
    print_line("Time set!", 0, 0, 2);
    delay(1000);
}

void set_time_zone() {
    int temp_offset = utc_offset;

    while (true) {
        display.clearDisplay();
        print_line("Set UTC Offset:", 0, 0, 2);
        print_line("Current: " + String(temp_offset), 0, 20, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_offset += 1;
        } else if (pressed == PB_DOWN) {
            delay(200);
            temp_offset -= 1;
        } else if (pressed == PB_OK) {
            delay(200);
            utc_offset = temp_offset;
            configTime(utc_offset * 3600, 0, NTP_SERVER);  // Update time with new offset
            break;
        } else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }

    display.clearDisplay();
    print_line("Time zone set!", 0, 0, 2);
    delay(1000);
}


void set_alarm(int alarm){
    int temp_hour = alarm_hours[alarm];
    while (true) {
        display.clearDisplay();
        print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }

        else if (pressed == PB_DOWN) {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0) {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK) {
            delay(200);
            alarm_hours[alarm] = temp_hour;
            break;
        }

        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }

    int temp_minute = alarm_minutes[alarm];
    while (true) {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }

        else if (pressed == PB_DOWN) {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0) {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK) {
            delay(200);
            alarm_minutes[alarm] = temp_minute;
            break;
        }

        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }
    display.clearDisplay();
    print_line("alarm set!", 0, 0, 2);
    delay(1000);
}

void run_mode(int mode) {
    if (mode == 0) {
        set_time();
    } else if (mode == 1 || mode == 2) {
        set_alarm(mode - 1);
    } else if (mode == 3) {
        alarm_enabled = false;
    } else if (mode == 4) {  // Add a new mode for setting the time zone
        set_time_zone();
    }
}


void check_temp() {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    bool temp_warning = false;
    bool humidity_warning = false;

    display.clearDisplay();
    print_line("Temp: " + String(data.temperature) + "C", 0, 20, 1);
    print_line("Humidity: " + String(data.humidity) + "%", 0, 30, 1);

    // Check temperature limits
    if (data.temperature < 24) {
        print_line("TEMP LOW!", 0, 40, 1);
        temp_warning = true;
    } else if (data.temperature > 32) {
        print_line("TEMP HIGH!", 0, 40, 1);
        temp_warning = true;
    }

    // Check humidity limits
    if (data.humidity < 65) {
        print_line("HUMIDITY LOW!", 0, 50, 1);
        humidity_warning = true;
    } else if (data.humidity > 80) {
        print_line("HUMIDITY HIGH!", 0, 50, 1);
        humidity_warning = true;
    }

    // Activate buzzer and LED if there is a warning
    if (temp_warning || humidity_warning) {
        digitalWrite(LED_1, HIGH);
        tone(BUZZER, 1000);  // 1kHz warning beep
        delay(500);
        noTone(BUZZER);
    } else {
        digitalWrite(LED_1, LOW); // Turn off LED if no warning
    }

    display.display();
}
