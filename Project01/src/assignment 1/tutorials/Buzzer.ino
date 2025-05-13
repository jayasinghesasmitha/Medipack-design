#include <Arduino.h>
#define Buzzer 13

int C = 261;
int D = 294;
int E = 329;
int F = 349;
int G = 392;
int A = 440;
int B = 493;
int C_H = 523;

int notes[] = {C, D, E, F, G, A, B, C_H};
int count = 8;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(Buzzer, OUTPUT);
  
  for (int i = 0; i < count; i++)
  {
    tone(Buzzer, notes[i]);
    delay(500);
    noTone(Buzzer);
    delay(2);
  }
  
}

void loop() {

}

