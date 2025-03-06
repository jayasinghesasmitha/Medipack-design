#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// Define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Declare OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long time_now = 0;
unsigned long time_last = 0;

// Function prototype (declaring before setup)
void print_line(String text);

void setup() {
  Serial.begin(9600);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  // Display initial message
  display.display();
  delay(2000);
  display.clearDisplay();

print_line("Welcome to Medibox!", 10, 20, 2);
display.clearDisplay();

}

void loop() {
  // put your main code here, to run repeatedly:
  update_time();
  print_time_now();
}

void print_line(String text, int column, int row, int text_size){

  display.setTextSize(text_size);

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row); // (column, row)

  display.println(text);

  display.display();
}

void print_time_now(void){
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
}

void update_time(){
  // seconds passed from bootup
  time_now = millis()/1000; 
  seconds = time_now - time_last;

  if (seconds >= 60){
    // minutes = minutes + 1
    minutes += 1; 
    time_last += 60;
  }

  if (minutes >= 60){
    hours += 1;
    minutes = 0;
  }

  if (hours >= 24){
    days += 1;
    hours = 0;
  }
}