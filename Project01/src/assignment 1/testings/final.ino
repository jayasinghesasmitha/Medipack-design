#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

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
#define SNOOZE_MINUTES 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
int n_alarms = 2;
int max_alarms = 5;
int alarm_hours[] = {0, 1, -1, -1, -1}; 
int alarm_minutes[] = {1, 10, -1, -1, -1};
bool alarm_triggered[] = {false, false, false, false, false};
int snooze_hours[] = {-1, -1, -1, -1, -1};
int snooze_minutes[] = {-1, -1, -1, -1, -1};

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
int max_modes = 7;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", 
                  "4 - Disable Alarm", "5 - Set Time Zone", "6 - View Alarms", 
                  "7 - Delete Alarm"};

float utc_offset = 0;

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

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();
  print_line("Welcome to Medibox!", 10, 20, 2);
  delay(500);
  display.clearDisplay();
}

void loop() {
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
  display.setCursor(column, row);
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
}

void ring_alarm() {
  bool alarm_stopped = false;
  bool alarm_snoozed = false;
  unsigned long last_blink = millis();
  bool led_state = HIGH;
  int current_note = 0;
  int current_alarm = current_mode; 
  while (!alarm_stopped && !alarm_snoozed) {
    if (millis() - last_blink > 500) {
      led_state = !led_state;
      digitalWrite(LED_1, led_state);
      last_blink = millis();
      
      display.clearDisplay();
      if (led_state == HIGH) {
        print_line("MEDICINE TIME!", 0, 0, 2);
        print_line(String(alarm_hours[current_alarm]) + ":" + 
                 (alarm_minutes[current_alarm] < 10 ? "0" : "") + 
                 String(alarm_minutes[current_alarm]), 0, 20, 2);
        print_line("OK: Snooze 5min", 0, 40, 1);
        print_line("CANCEL: Stop", 0, 50, 1);
      }
    }
    
    if (!alarm_stopped && !alarm_snoozed) {
      tone(BUZZER, notes[current_note], 300);
      delay(350);
      noTone(BUZZER);
      current_note = (current_note + 1) % n_notes;
    }
    
    if (digitalRead(PB_CANCEL) == LOW) {
      alarm_stopped = true;
      delay(200); 
    }

    if (digitalRead(PB_OK) == LOW) {
      alarm_snoozed = true;
      delay(200); 
    }
  }
  
  digitalWrite(LED_1, LOW);
  noTone(BUZZER);
  
  if (alarm_stopped) {
    display.clearDisplay();
    print_line("Alarm stopped", 0, 0, 1);
    alarm_triggered[current_alarm] = true;
    snooze_hours[current_alarm] = -1; 
    snooze_minutes[current_alarm] = -1;
    delay(1000);
  }
  
  if (alarm_snoozed) {
    int snooze_min = minutes + SNOOZE_MINUTES;
    int snooze_hr = hours;
    
    if (snooze_min >= 60) {
      snooze_min -= 60;
      snooze_hr += 1;
      if (snooze_hr >= 24) {
        snooze_hr -= 24;
      }
    }
    
    snooze_hours[current_alarm] = snooze_hr;
    snooze_minutes[current_alarm] = snooze_min;
    alarm_triggered[current_alarm] = false; 
    
    display.clearDisplay();
    print_line("Alarm snoozed", 0, 0, 1);
    print_line("Until " + String(snooze_hr) + ":" + (snooze_min < 10 ? "0" : "") + String(snooze_min), 0, 20, 1);
    delay(2000);
  }
}

void ring_alarm_2(bool should_stop_ringing) {
    bool alarm_stopped = false;
    
    while (!alarm_stopped) {
        // If should_stop_ringing is FALSE, keep ringing
        if (!should_stop_ringing) {
            tone(BUZZER, 200, 300); // Beep at 200Hz for 300ms
            delay(350);             // Short delay between beeps
            noTone(BUZZER);          // Stop sound
        }
        
        // Check if CANCEL button is pressed (stops in all cases)
        if (digitalRead(PB_CANCEL) == LOW) {
            alarm_stopped = true;
            delay(200); // Debounce delay
        }
    }
    
    // Ensure buzzer is off when stopped
    noTone(BUZZER);
}

void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
      for (int i = 0; i < max_alarms; i++) {
          bool is_original_alarm_time = (alarm_hours[i] != -1) && 
                                     (alarm_triggered[i] == false) && 
                                     (hours == alarm_hours[i]) && 
                                     (minutes == alarm_minutes[i]);
          
          bool is_snooze_time = (snooze_hours[i] != -1) && 
                              (hours == snooze_hours[i]) && 
                              (minutes == snooze_minutes[i]);
          
          if (is_original_alarm_time || is_snooze_time) {
              current_mode = i; 
              ring_alarm();
              
              if (is_snooze_time) {
                snooze_hours[i] = -1;
                snooze_minutes[i] = -1;
              }
              
              alarm_triggered[i] = true;
          }
          
          if (hours == 0 && minutes == 0) {
            alarm_triggered[i] = false;
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
          delay(200);
          current_mode += 1;
          current_mode = current_mode % max_modes;
        }
    
        if (pressed == PB_DOWN) {
          delay(200);
          current_mode -= 1;
    
          if (current_mode < 0 ) {
            current_mode = max_modes - 1;
          }
        }
    
        else if (pressed == PB_OK) {
          delay(200);
          Serial.println(current_mode);
          run_mode(current_mode);
        }
    
        else if (pressed == PB_CANCEL) {
          delay(200);
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

void set_alarm(int alarm){
    int temp_hour = 0;
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
            return;
        }
    }

    int temp_minute = 0;
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
            return;
        }
    }
    display.clearDisplay();
    print_line("Alarm set!", 0, 0, 2);
    delay(1000);
}

void set_time_zone() {
    float temp_offset = utc_offset;
    while (true) {
        display.clearDisplay();
        print_line("UTC Offset: " + String(temp_offset), 0, 0, 2);
        print_line("(-12 to +14)", 2, 40, 1);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_offset += 0.5;
            if (temp_offset > 14) temp_offset = -12;
        }
        else if (pressed == PB_DOWN) {
            delay(200);
            temp_offset -= 0.5;
            if (temp_offset < -12) temp_offset = 14;
        }
        else if (pressed == PB_OK) {
            delay(200);
            utc_offset = temp_offset;
            configTime(utc_offset * 3600, UTC_OFFSET_DST, NTP_SERVER);
            break;
        }
        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }
    display.clearDisplay();
    print_line("Timezone set!", 0, 0, 2);
    delay(1000);
}

void view_alarms() {
    display.clearDisplay();
    int y_pos = 0;
    for (int i = 0; i < max_alarms; i++) {
        if (alarm_hours[i] != -1) {
            String alarm_str = "Alarm " + String(i+1) + ": " + 
                             String(alarm_hours[i]) + ":" + 
                             (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]);
            print_line(alarm_str, 0, y_pos, 1);
            y_pos += 10;
        }
    }
    if (y_pos == 0) {
        print_line("No alarms set", 0, 0, 1);
    }
    wait_for_button_press(); 
}

void delete_alarm() {
    int selected_alarm = 0;
    int active_alarms = 0;
 
    for (int i = 0; i < max_alarms; i++) {
        if (alarm_hours[i] != -1) active_alarms++;
    }
    
    if (active_alarms == 0) {
        display.clearDisplay();
        print_line("No alarms to delete", 0, 0, 1);
        delay(1000);
        return;
    }
    
    while (alarm_hours[selected_alarm] == -1 && selected_alarm < max_alarms) {
        selected_alarm++;
    }
    
    while (true) {
        display.clearDisplay();
        print_line("Delete Alarm " + String(selected_alarm+1), 0, 0, 1);
        print_line(String(alarm_hours[selected_alarm]) + ":" + 
                 (alarm_minutes[selected_alarm] < 10 ? "0" : "") + 
                 String(alarm_minutes[selected_alarm]), 0, 15, 2);
        print_line("OK to delete", 0, 40, 1);
        print_line("UP/DOWN to change", 0, 50, 1);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            do {
                selected_alarm = (selected_alarm + 1) % max_alarms;
            } while (alarm_hours[selected_alarm] == -1 && selected_alarm < max_alarms);
        }
        else if (pressed == PB_DOWN) {
            delay(200);
            do {
                selected_alarm = (selected_alarm - 1);
                if (selected_alarm < 0) selected_alarm = max_alarms - 1;
            } while (alarm_hours[selected_alarm] == -1 && selected_alarm < max_alarms);
        }
        else if (pressed == PB_OK) {
            delay(200);
            alarm_hours[selected_alarm] = -1;
            alarm_minutes[selected_alarm] = -1;
            alarm_triggered[selected_alarm] = false;
            display.clearDisplay();
            print_line("Alarm deleted", 0, 0, 1);
            delay(1000);
            break;
        }
        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }
}

void run_mode(int mode) {
    if (mode == 0) {
        set_time();
    }
    else if (mode == 1 || mode == 2) {
        set_alarm(mode - 1);
    }
    else if (mode == 3) {
        alarm_enabled = !alarm_enabled;
        display.clearDisplay();
        print_line("Alarms " + String(alarm_enabled ? "enabled" : "disabled"), 0, 0, 1);
        delay(1000);
    }
    else if (mode == 4) {
        set_time_zone();
    }
    else if (mode == 5) {
        view_alarms();
    }
    else if (mode == 6) {
        delete_alarm();
    }
}

void check_temp() {
    static bool wasAbnormal = false; 
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    bool abnormalCondition = false;

    if (data.temperature > 32) {
        display.clearDisplay();
        print_line("TEMP HIGH", 0, 40, 1);
        abnormalCondition = true;
    }
    else if (data.temperature < 24) {
        display.clearDisplay();
        print_line("TEMP LOW", 0, 40, 1);
        abnormalCondition = true;
    }

    if (data.humidity > 80) {
        display.clearDisplay();
        print_line("HUMIDITY HIGH", 0, 50, 1);
        abnormalCondition = true;
    }
    else if (data.humidity < 65) {
        display.clearDisplay();
        print_line("HUMIDITY LOW", 0, 50, 1);
        abnormalCondition = true;
    }

    if (abnormalCondition) {
        digitalWrite(LED_1, HIGH);
        if (!wasAbnormal) {
            tone(BUZZER, 1000); 
        }
    } 
    else {
        digitalWrite(LED_1, LOW);
        noTone(BUZZER); 
        display.clearDisplay();
        print_line("NORMAL", 0, 40, 1);
    }
    wasAbnormal = abnormalCondition;

    static unsigned long lastButtonCheck = 0;
    if (millis() - lastButtonCheck > 200) { 
        if (digitalRead(PB_CANCEL) == LOW) {
            noTone(BUZZER); 
            digitalWrite(LED_1, LOW); 
            display.clearDisplay();
            print_line("ALARM SILENCED", 0, 40, 1);
            delay(1000);
        }
        lastButtonCheck = millis();
    }
}