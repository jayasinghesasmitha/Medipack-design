#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN 12
#define BUZZER 5

WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

char tempAr[6];
DHTesp dhtSensor;
bool isSceduleON =false;
unsigned long scheduledOnTime;

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  timeClient.begin();
  timeClient.setTimeOffset(5.5 * 3600); 
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();
  updateTemperature();
  Serial.println(tempAr);
  mqttClient.publish("CSE-ADMIN-TEMP", tempAr);
  checkSchdule();
  delay(1000);
}

void buzzerOn(bool on) {
  if (on) {
    tone(BUZZER, 256);
  } else {
    noTone(BUZZER);
  }
}

void setupMqtt() {
  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived [");
  Serial.println(topic);
  Serial.println("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++) {
    Serial.println((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println();

  if (strcmp(topic, "CSE-ADMIN-MAIN-ON-OFF") == 0) {
    buzzerOn(payloadCharAr[0] == '1');
  }else if(strcmp(topic, "CSE-ADMIN-SCH-ON") == 0) {
    if (payloadCharAr[0] == 'N') {
      isSceduleON = false;
    } else {
      isSceduleON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }
}

void connectToBroker() {
  while(!mqttClient.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("connected");
      mqttClient.subscribe("CSE-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("CSE-ADMIN-SCH-ON");
    } else {
      Serial.println("failed");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
}

void setupWifi(){
  Serial.println();
  Serial.println("Connecting to WiFi...");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  Serial.println("WiFi connected"); 
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

void checkSchdule(){
  if(isSceduleON) {
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isSceduleON = false;
      mqttClient.publish("CSE-ADMIN-MAIN-ON-OFF-ESP", "1");
      mqttClient.publish("CSE-ADMIN-SCH-ESP-ON", "0");
      Serial.println("Scheduled ON time");
    }
  } else {
    if (mqttClient.connected()) {
      mqttClient.publish("CSE-ADMIN-MAIN-ON-OFF", "0");
    }
  }
}