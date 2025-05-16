#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Servo.h>
#include <math.h>

#define DHTPIN 12
#define BUZZER 5
#define LDR_PIN 34 // First LDR
#define LDR2_PIN 35 // Second LDR
#define SERVO_PIN 13 // Servo motor

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Servo servo;

char tempAr[6];
char lightAr[6];
char light2Ar[6];
char servoAngleAr[6];
DHTesp dhtSensor;
bool isScheduleON = false;
unsigned long scheduledOnTime;

// LDR variables
float ldrReadings[24];
float ldr2Readings[24];
int ldrIndex = 0;
unsigned long lastLdrSample = 0;
unsigned long lastLdrPublish = 0;
int sampleInterval = 5000; // Default 5 seconds (in ms)
int sendInterval = 120000; // Default 2 minutes (in ms)
int ldrReadingCount = 0;

// Servo control parameters
float thetaOffset = 30.0; // Default θ_offset
float gamma = 0.75; // Default γ
float tMed = 30.0; // Default T_med

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  dhtSensor.setup(DHTPIN, DHTesp::DHT11); // Use DHT11
  timeClient.begin();
  timeClient.setTimeOffset(5.5 * 3600);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(LDR_PIN, INPUT);
  pinMode(LDR2_PIN, INPUT);
  servo.attach(SERVO_PIN);
  servo.write(30); // Initial position
}

void loop() {
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();
  
  // Temperature update
  updateTemperature();
  Serial.println("Temperature: " + String(tempAr));
  mqttClient.publish("CSE-ADMIN-TEMP", tempAr);
  
  // LDR sampling
  unsigned long currentMillis = millis();
  if (currentMillis - lastLdrSample >= sampleInterval) {
    readLdr();
    updateServoAngle();
    lastLdrSample = currentMillis;
  }
  
  // Publish LDR averages
  if (currentMillis - lastLdrPublish >= sendInterval && ldrReadingCount >= 24) {
    publishLdrAverage();
    lastLdrPublish = currentMillis;
  }
  
  checkSchedule();
  delay(100);
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
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  char payloadCharAr[length + 1];
  for (int i = 0; i < length; i++) {
    payloadCharAr[i] = (char)payload[i];
  }
  payloadCharAr[length] = '\0';
  Serial.println(payloadCharAr);

  if (strcmp(topic, "CSE-ADMIN-MAIN-ON-OFF") == 0) {
    buzzerOn(payloadCharAr[0] == '1');
  } else if (strcmp(topic, "CSE-ADMIN-SCH-ON") == 0) {
    if (payloadCharAr[0] == 'N') {
      isScheduleON = false;
    } else {
      isScheduleON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  } else if (strcmp(topic, "CSE-ADMIN-LDR-SAMPLE-INTERVAL") == 0) {
    sampleInterval = atoi(payloadCharAr) * 1000;
    ldrIndex = 0;
    ldrReadingCount = 0;
  } else if (strcmp(topic, "CSE-ADMIN-LDR-SEND-INTERVAL") == 0) {
    sendInterval = atoi(payloadCharAr) * 1000;
    ldrIndex = 0;
    ldrReadingCount = 0;
  } else if (strcmp(topic, "CSE-ADMIN-THETA-OFFSET") == 0) {
    thetaOffset = atof(payloadCharAr);
  } else if (strcmp(topic, "CSE-ADMIN-GAMMA") == 0) {
    gamma = atof(payloadCharAr);
  } else if (strcmp(topic, "CSE-ADMIN-TMED") == 0) {
    tMed = atof(payloadCharAr);
  }
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("connected");
      mqttClient.subscribe("CSE-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("CSE-ADMIN-SCH-ON");
      mqttClient.subscribe("CSE-ADMIN-LDR-SAMPLE-INTERVAL");
      mqttClient.subscribe("CSE-ADMIN-LDR-SEND-INTERVAL");
      mqttClient.subscribe("CSE-ADMIN-THETA-OFFSET");
      mqttClient.subscribe("CSE-ADMIN-GAMMA");
      mqttClient.subscribe("CSE-ADMIN-TMED");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
}

void setupWifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

void checkSchedule() {
  if (isScheduleON) {
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isScheduleON = false;
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

void readLdr() {
  int rawValue = analogRead(LDR_PIN);
  float normalized = rawValue / 4095.0;
  ldrReadings[ldrIndex] = normalized;
  
  int rawValue2 = analogRead(LDR2_PIN);
  float normalized2 = rawValue2 / 4095.0;
  ldr2Readings[ldrIndex] = normalized2;
  
  ldrIndex = (ldrIndex + 1) % 24;
  if (ldrReadingCount < 24) ldrReadingCount++;
}

void publishLdrAverage() {
  if (ldrReadingCount == 0) return;
  float sum = 0;
  float sum2 = 0;
  for (int i = 0; i < ldrReadingCount; i++) {
    sum += ldrReadings[i];
    sum2 += ldr2Readings[i];
  }
  float average = sum / ldrReadingCount;
  float average2 = sum2 / ldrReadingCount;
  String(average, 2).toCharArray(lightAr, 6);
  String(average2, 2).toCharArray(light2Ar, 6);
  mqttClient.publish("CSE-ADMIN-LDR-AVG", lightAr);
  mqttClient.publish("CSE-ADMIN-LDR2-AVG", light2Ar);
  Serial.println("LDR1 Average: " + String(lightAr));
  Serial.println("LDR2 Average: " + String(light2Ar));
}

void updateServoAngle() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float T = data.temperature;
  float I = ldrReadings[ldrIndex == 0 ? 23 : ldrIndex - 1]; // Latest LDR1 reading
  float ts = sampleInterval / 1000.0; // Convert to seconds
  float tu = sendInterval / 1000.0; // Convert to seconds
  float theta = thetaOffset + (180.0 - thetaOffset) * I * gamma * log(ts / tu) * (T / tMed);
  theta = constrain(theta, 0, 180); // Ensure within 0-180
  servo.write((int)theta);
  String(theta, 2).toCharArray(servoAngleAr, 6);
  mqttClient.publish("CSE-ADMIN-SERVO-ANGLE", servoAngleAr);
  Serial.println("Servo Angle: " + String(servoAngleAr));
}