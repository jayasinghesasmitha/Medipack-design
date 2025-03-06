#include <Arduino.h>
#include <DHTesp.h>

const int DHTPin = 15;
DHTesp dhtSensor; // Corrected Typo

void setup() {
  Serial.begin(115200);
  dhtSensor.setup(DHTPin, DHTesp::DHT22); 
}

void loop() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  Serial.println("Temperature: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("--------------------");
  delay(2000);
}