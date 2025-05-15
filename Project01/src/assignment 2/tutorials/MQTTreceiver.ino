#include <PubSubClient.h>
#include <WiFi.h>
#include <DHTesp.h>

#define DHTPIN 12

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;

char tempAr[6];

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMQTT();
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
}

void loop() {
  if(!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();

  updateTemperature();
  Serial.println(tempAr);
  mqttClient.publish("CSE",tempAr);
  delay(1000);
}

void setupMQTT() {
  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(receiveCallback);
}

void setupWifi() {
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");       
    Serial.println(WiFi.localIP());
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("connected");
      mqttClient.subscribe("CSE-ON-OFF");
    } else {
      Serial.print("failed");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void updateTemperature() {
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr, 6);
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char payloadCharAr[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println();
}