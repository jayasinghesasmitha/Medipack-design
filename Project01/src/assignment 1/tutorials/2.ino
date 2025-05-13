#include <Arduino.h>

int summation = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Hello, ESP32!");
}

void loop() {
  // put your main code here, to run repeatedly:
  summation = local_addition(2, 3);
  Serial.print("Local addition: ");
  Serial.println(summation);
  delay(1000); // this speeds up the simulation

  global_addition(8, 3);
  Serial.print("Global addition: ");
  Serial.println(summation);
}

int local_addition(int a, int b) {
  int sum =  a + b;
  return sum;
}

void global_addition(int a, int b) {  
  summation = a+b;
}