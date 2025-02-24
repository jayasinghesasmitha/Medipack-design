#include <Arduino.h>
#define LED 13

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(9600);
    Serial.println("Led blink");
    pinMode(LED, OUTPUT);
}

void loop()
{
    // put your main code here, to run repeatedly:
    delay(1000); // this speeds up the simulation
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED,LOW);
}
