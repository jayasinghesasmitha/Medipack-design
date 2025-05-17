#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>
#include <math.h>

#define DHTPIN 12
#define BUZZER 5
#define LDR_PIN 34 // Single LDR
#define SERVO_PIN 13 // Servo motor

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Servo servo;

char tempAr[6];
char lightAr[6];
char servoAngleAr[6];
DHTesp dhtSensor;
bool isScheduleON = false;
unsigned long scheduledOnTime;

// LDR variables
float ldrReadings[24];
int ldrIndex = 0;
unsigned long lastLdrSample = 0;
unsigned long lastLdrPublish = 0;
int sampleInterval = 5000; // Default 5 seconds (in ms)
int sendInterval = 120000; // Default 2 minutes (in ms)
int ldrReadingCount = 0;

// Servo control parameters
float thetaOffset = 30.0; // Default θ_offset
float controlFactor = 0.75; // Default γ
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
  servo.setPeriodHertz(50); // Standard 50 Hz servo
  servo.attach(SERVO_PIN, 500, 2500); // Min/max pulse width for ESP32
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
  
  // Publish LDR average
  if (currentMillis - lastLdrPublish >= sendInterval && ldrReadingCount >= 24) {
    publishLdrAverage();
    lastLdrPublish = currentMillis;
    // Reset readings after publishing
    ldrIndex = 0;
    ldrReadingCount = 0;
    for (int i = 0; i < 24; i++) {
      ldrReadings[i] = 0;
    }
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
    int newInterval = atoi(payloadCharAr) * 1000;
    if (newInterval >= 1000) { // Minimum 1s
      sampleInterval = newInterval;
      ldrIndex = 0;
      ldrReadingCount = 0;
    }
  } else if (strcmp(topic, "CSE-ADMIN-LDR-SEND-INTERVAL") == 0) {
    int newInterval = atoi(payloadCharAr) * 1000;
    if (newInterval >= 30000) { // Minimum 30s
      sendInterval = newInterval;
      ldrIndex = 0;
      ldrReadingCount = 0;
    }
  } else if (strcmp(topic, "CSE-ADMIN-THETA-OFFSET") == 0) {
    float newOffset = atof(payloadCharAr);
    if (newOffset >= 0 && newOffset <= 120) {
      thetaOffset = newOffset;
    }
  } else if (strcmp(topic, "CSE-ADMIN-CONTROL-FACTOR") == 0) {
    float newFactor = atof(payloadCharAr);
    if (newFactor >= 0 && newFactor <= 1) {
      controlFactor = newFactor;
    }
  } else if (strcmp(topic, "CSE-ADMIN-TMED") == 0) {
    float newTmed = atof(payloadCharAr);
    if (newTmed >= 10 && newTmed <= 40) {
      tMed = newTmed;
    }
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
      mqttClient.subscribe("CSE-ADMIN-CONTROL-FACTOR");
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
  if (!isnan(data.temperature)) {
    String(data.temperature, 2).toCharArray(tempAr, 6);
  } else {
    strcpy(tempAr, "0.00");
  }
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
  
  ldrIndex = (ldrIndex + 1) % 24;
  if (ldrReadingCount < 24) ldrReadingCount++;
}

void publishLdrAverage() {
  if (ldrReadingCount == 0) return;
  float sum = 0;
  for (int i = 0; i < ldrReadingCount; i++) {
    sum += ldrReadings[i];
  }
  float average = sum / ldrReadingCount;
  String(average, 2).toCharArray(lightAr, 6);
  mqttClient.publish("CSE-ADMIN-LDR-AVG", lightAr);
  Serial.println("LDR Average: " + String(lightAr));
}

void updateServoAngle() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float T = data.temperature;
  float I = ldrReadings[ldrIndex == 0 ? 23 : ldrIndex - 1]; // Latest LDR reading
  float ts = sampleInterval / 1000.0; // Convert to seconds
  float tu = sendInterval / 1000.0; // Convert to seconds
  
  // Validate inputs
  if (isnan(T) || T <= 0 || tMed <= 0 || tu <= 0 || ts <= 0) {
    servo.write((int)thetaOffset); // Default to minimum angle
    strcpy(servoAngleAr, "30.00");
    mqttClient.publish("CSE-ADMIN-SERVO-ANGLE", servoAngleAr);
    Serial.println("Servo Angle: Invalid input, set to " + String(servoAngleAr));
    return;
  }
  
  // Use ln(tu/ts) for positive logarithm
  float theta = thetaOffset + (180.0 - thetaOffset) * I * controlFactor * log(tu / ts) * (T / tMed);
  theta = constrain(theta, 0, 180); // Ensure within 0-180
  servo.write((int)theta);
  String(theta, 2).toCharArray(servoAngleAr, 6);
  mqttClient.publish("CSE-ADMIN-SERVO-ANGLE", servoAngleAr);
  Serial.println("Servo Angle: " + String(servoAngleAr));
}