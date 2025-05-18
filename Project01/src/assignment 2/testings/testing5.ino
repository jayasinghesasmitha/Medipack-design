#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <vector>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // 5.5 hours in seconds
const int daylightOffset_sec = 0;

// Pin definitions
#define DHTPIN 12
#define BUZZER 5
#define LDR_PIN 34
#define SERVO_PIN 13

// Global variables
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Servo servo;
DHTesp dhtSensor;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

char tempAr[6];
char lightAr[6];
char servoAngleAr[6];
bool isScheduleON = false;
unsigned long scheduledOnTime;

// LDR variables
std::vector<float> ldrReadings;
int ldrIndex = 0;
unsigned long lastLdrSample = 0;
unsigned long lastLdrPublish = 0;
int sampleInterval = 5000; // Default 5 seconds (in ms)
int sendInterval = 120000; // Default 2 minutes (in ms)
int ldrBufferSize = 24; // Default buffer size
int ldrReadingCount = 0;

// LDR calibration constants
const float LDR_MIN = 0.0; // Minimum raw ADC value
const float LDR_MAX = 4095.0; // Maximum raw ADC value
const float LDR_OUT_MIN = 0.0; // Output min (0)
const float LDR_OUT_MAX = 1.0; // Output max (1)

// Servo control parameters
float thetaOffset = 30.0; // Default θ_offset
float controlFactor = 0.75; // Default γ
float tMed = 30.0; // Default T_med

// WiFi and MQTT credentials
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "limetawny-161906ba.a03.euc1.aws.hivemq.cloud";
const int mqtt_port = 8883;
const char *mqtt_user = "uvindu";
const char *mqtt_pass = "Sas220261v";

unsigned long previousMillisTemp = 0;
unsigned long previousMillisLight = 0;
unsigned long previousMillisServo = 0;

// Initialization functions
void initializeWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void initializeMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(receiveCallback);
  mqttClient.setKeepAlive(15);
}

void initializeDHT() {
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
}

void initializePins() {
  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  buzzerOn(false);
}

void reconnectMQTT() {
  int retryCount = 0;
  const int maxRetries = 5;
  while (!mqttClient.connected() && retryCount < maxRetries) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      mqttClient.subscribe("ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("ADMIN-SCH-ON");
      mqttClient.subscribe("ADMIN-LDR-SAMPLE-INTERVAL");
      mqttClient.subscribe("ADMIN-LDR-SEND-INTERVAL");
      mqttClient.subscribe("ADMIN-THETA-OFFSET");
      mqttClient.subscribe("ADMIN-CONTROL-FACTOR");
      mqttClient.subscribe("ADMIN-TMED");
      mqttClient.subscribe("ADMIN-LDR-BUFFER-SIZE");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println();
      delay(2000);
      retryCount++;
    }
  }
  if (!mqttClient.connected()) {
    Serial.println("MQTT connection failed after max retries");
  }
}

// Main functions
void send_temperature() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisTemp >= sampleInterval * 1.5) {
    previousMillisTemp = currentMillis;
    float temperature;
    if (readDhtWithRetry(temperature)) {
      String(temperature, 2).toCharArray(tempAr, 6);
    } else {
      strcpy(tempAr, "0.00");
    }
    Serial.print("Sending temperature: ");
    Serial.println(tempAr);
    mqttClient.publish("ADMIN-TEMP", tempAr);
  }
}

void send_light_level() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastLdrSample >= sampleInterval) {
    readLdr();
    lastLdrSample = currentMillis;
  }

  if (currentMillis - lastLdrPublish >= sendInterval && ldrReadingCount >= 10) {
    Serial.println("Attempting to publish LDR average...");
    publishLdrAverage();
    lastLdrPublish = currentMillis;
    while (ldrReadings.size() > ldrBufferSize) {
      ldrReadings.erase(ldrReadings.begin());
      ldrReadingCount--;
    }
  }
}

void readLdr() {
  int rawValue = analogRead(LDR_PIN);
  float normalized = (rawValue - LDR_MIN) * (LDR_OUT_MAX - LDR_OUT_MIN) / (LDR_MAX - LDR_MIN) + LDR_OUT_MIN;
  normalized = constrain(normalized, 0.0, 1.0);
  ldrReadings.push_back(normalized);
  ldrReadingCount++;
  Serial.print("LDR reading: ");
  Serial.println(normalized, 2);
}

void publishLdrAverage() {
  if (ldrReadingCount == 0) {
    Serial.println("No LDR readings to average");
    return;
  }
  float sum = 0;
  for (float reading : ldrReadings) {
    sum += reading;
  }
  float average = sum / ldrReadingCount;
  String(average, 2).toCharArray(lightAr, 6);
  Serial.print("Sending LDR average: ");
  Serial.println(lightAr);

  int retries = 0;
  const int maxRetries = 3;
  while (retries < maxRetries) {
    if (mqttClient.publish("ADMIN-LDR-AVG", lightAr)) {
      Serial.println("LDR average published successfully");
      break;
    } else {
      Serial.println("LDR publish failed, retrying...");
      delay(500);
      retries++;
    }
  }
  if (retries >= maxRetries) {
    Serial.println("LDR publish failed after max retries");
  }
}

void control_servo() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisServo < sampleInterval * 2) {
    return;
  }
  previousMillisServo = currentMillis;

  float T;
  if (!readDhtWithRetry(T)) {
    T = 0.0;
  }
  float I = ldrReadings.empty() ? 0.0 : ldrReadings.back();
  float ts = sampleInterval / 1000.0;
  float tu = sendInterval / 1000.0;

  Serial.print("LDR (I): "); Serial.println(I, 2);
  Serial.print("Temperature (T): "); Serial.println(T, 2);
  Serial.print("tu: "); Serial.println(tu, 2);
  Serial.print("ts: "); Serial.println(ts, 2);
  Serial.print("controlFactor: "); Serial.println(controlFactor, 2);
  Serial.print("tMed: "); Serial.println(tMed, 2);
  Serial.print("thetaOffset: "); Serial.println(thetaOffset, 2);

  if (isnan(T) || T <= 0 || tMed <= 0 || tu <= 0 || ts <= 0 || isnan(I)) {
    servo.write((int)thetaOffset);
    String(thetaOffset, 2).toCharArray(servoAngleAr, 6);
    mqttClient.publish("ADMIN-SERVO-ANGLE", servoAngleAr);
    Serial.println("Servo Angle: Invalid input, set to " + String(servoAngleAr));
    return;
  }

  float ratio = tu / ts;
  float logVal = log(ratio);
  float theta = thetaOffset + (180.0 - thetaOffset) * I * controlFactor * logVal * (T / tMed) * 0.1;
  theta = constrain(theta, 0, 180);

  Serial.print("log(tu/ts): "); Serial.println(logVal, 2);
  Serial.print("T/tMed: "); Serial.println(T / tMed, 2);
  Serial.print("Formula before constrain: "); Serial.println(thetaOffset + (180.0 - thetaOffset) * I * controlFactor * logVal * (T / tMed) * 0.1, 2);

  servo.write((int)theta);
  String(theta, 2).toCharArray(servoAngleAr, 6);
  mqttClient.publish("ADMIN-SERVO-ANGLE", servoAngleAr);
  Serial.println("Servo Angle: " + String(servoAngleAr));
}

void buzzerOn(bool on) {
  if (on) {
    tone(BUZZER, 256);
  } else {
    noTone(BUZZER);
  }
}

bool readDhtWithRetry(float& temperature) {
  const int maxRetries = 3;
  for (int i = 0; i < maxRetries; i++) {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    if (!isnan(data.temperature) && data.temperature > 0) {
      temperature = data.temperature;
      return true;
    }
    Serial.println("DHT22 read failed, retrying...");
    delay(500);
  }
  Serial.println("DHT22 read failed after max retries");
  return false;
}

void checkSchedule() {
  if (isScheduleON) {
    unsigned long currentTime = timeClient.getEpochTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isScheduleON = false;
      mqttClient.publish("ADMIN-MAIN-ON-OFF-ESP", "1");
      mqttClient.publish("ADMIN-SCH-ESP-ON", "0");
      Serial.println("Scheduled ON time");
    }
  }
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char payloadCharAr[length + 1];
  for (int i = 0; i < length; i++) {
    payloadCharAr[i] = (char)payload[i];
  }
  payloadCharAr[length] = '\0';
  Serial.println(payloadCharAr);

  if (strcmp(topic, "ADMIN-MAIN-ON-OFF") == 0) {
    buzzerOn(payloadCharAr[0] == '1');
  } else if (strcmp(topic, "ADMIN-SCH-ON") == 0) {
    if (payloadCharAr[0] == 'N') {
      isScheduleON = false;
    } else {
      isScheduleON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  } else if (strcmp(topic, "ADMIN-LDR-SAMPLE-INTERVAL") == 0) {
    int newInterval = atoi(payloadCharAr) * 1000;
    if (newInterval >= 1000) {
      sampleInterval = newInterval;
      ldrReadings.clear();
      ldrReadingCount = 0;
    }
  } else if (strcmp(topic, "ADMIN-LDR-SEND-INTERVAL") == 0) {
    int newInterval = atoi(payloadCharAr) * 1000;
    if (newInterval >= 30000) {
      sendInterval = newInterval;
      ldrReadings.clear();
      ldrReadingCount = 0;
    }
  } else if (strcmp(topic, "ADMIN-THETA-OFFSET") == 0) {
    float newOffset = atof(payloadCharAr);
    if (newOffset >= 0 && newOffset <= 120) {
      thetaOffset = newOffset;
    }
  } else if (strcmp(topic, "ADMIN-CONTROL-FACTOR") == 0) {
    float newFactor = atof(payloadCharAr);
    if (newFactor >= 0 && newFactor <= 1) {
      controlFactor = newFactor;
    }
  } else if (strcmp(topic, "ADMIN-TMED") == 0) {
    float newTmed = atof(payloadCharAr);
    if (newTmed >= 10 && newTmed <= 40) {
      tMed = newTmed;
    }
  } else if (strcmp(topic, "ADMIN-LDR-BUFFER-SIZE") == 0) {
    int newBufferSize = atoi(payloadCharAr);
    if (newBufferSize >= 10 && newBufferSize <= 100) {
      ldrBufferSize = newBufferSize;
      while (ldrReadings.size() > ldrBufferSize) {
        ldrReadings.erase(ldrReadings.begin());
        ldrReadingCount--;
      }
    }
  } else {
    Serial.println("Unknown topic");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  initializeDHT();
  initializeWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    initializeMQTT();
  }
  initializePins();
  servo.attach(SERVO_PIN, 500, 2500);
  servo.write((int)thetaOffset);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  timeClient.update();

  send_light_level();
  send_temperature();
  control_servo();
  checkSchedule();
}