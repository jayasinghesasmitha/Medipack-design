#include <Arduino.h>
int button_1 = 2;
int button_2 = 15;

void setup() {
  // put your setup code here, to run once:
  pinMode(button_1, INPUT);
  pinMode(button_2, INPUT);

  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  int button_1_state = digitalRead(button_1);
  int button_2_state = digitalRead(button_2);

  if (button_1_state == LOW) {
    Serial.println("Button 1 is pressed");
    delay(200);
  }

  if (button_2_state == HIGH) {
    Serial.println("Button 2 is pressed");
    delay(200);
  }
}