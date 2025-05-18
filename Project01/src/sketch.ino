#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; 
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

char tempAr[10]; 
char lightAr[10]; 
char servoAngleAr[10]; 
bool isScheduleON = false;
unsigned long scheduledOnTime;

// LDR variables
float lightCurrent = 0.0; 
unsigned long lastLdrSample = 0;
int sampleInterval = 5000; 
float averageWait = 120000; 

// LDR calibration constants
const float LOW_LIM = 15051.50;
const float UP_LIM = 36088.47;

// Servo control parameters
float thetaOffset = 30.0; 
float controlFactor = 0.75; 
float tMed = 30.0;

// WiFi and MQTT credentials
//special note: The private MQTT server is not available after 2025/05/30 because the free trial period is over
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "limetawny-161906ba.a03.euc1.aws.hivemq.cloud";
const int mqtt_port = 8883;
//After the trial period, the public MQTT server can be used.
//const char *mqtt_server = "broker.hivemq.com";
//const int mqtt_port = 1883;
const char *mqtt_user = "uvindu";
const char *mqtt_pass = "Sas220261v";

unsigned long previousMillisTemp = 0;
unsigned long previousMillisServo = 0;

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
      dtostrf(temperature, 4, 2, tempAr);
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
    Serial.print("Sending light level: ");
    Serial.println(lightCurrent, 2);
    dtostrf(lightCurrent, 4, 2, lightAr); 
    mqttClient.publish("ADMIN-LDR-AVG", lightAr);
  }
}

void readLdr() {
  // Average multiple readings to reduce noise
  const int numReadings = 5;
  long sum = 0;
  for (int i = 0; i < numReadings; i++) {
    sum += analogRead(LDR_PIN);
    delay(10); 
  }
  int analogValue = sum / numReadings;
  analogValue = max(1, analogValue); 
  float lightLevel = log10((float)analogValue);
  float scaledLux = lightLevel * 10000;
  scaledLux = (scaledLux - LOW_LIM) / (UP_LIM - LOW_LIM);
  lightCurrent = 1 - constrain(scaledLux, 0.0, 1.0); 
  Serial.print("Raw ADC: "); Serial.println(analogValue);
  Serial.print("LDR reading: "); Serial.println(lightCurrent, 2);
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
  float I = lightCurrent;
  float ratio = averageWait / sampleInterval;
  ratio = max(ratio, 1.0f); 

  Serial.print("LDR (I): "); Serial.println(I, 2);
  Serial.print("Temperature (T): "); Serial.println(T, 2);
  Serial.print("averageWait: "); Serial.println(averageWait, 2);
  Serial.print("sampleInterval: "); Serial.println(sampleInterval, 2);
  Serial.print("controlFactor: "); Serial.println(controlFactor, 2);
  Serial.print("tMed: "); Serial.println(tMed, 2);
  Serial.print("thetaOffset: "); Serial.println(thetaOffset, 2);

  if (isnan(T) || T <= 0 || tMed <= 0 || ratio <= 0 || isnan(I)) {
    servo.write((int)thetaOffset);
    dtostrf(thetaOffset, 4, 2, servoAngleAr);
    mqttClient.publish("ADMIN-SERVO-ANGLE", servoAngleAr);
    Serial.println("Servo Angle: Invalid input, set to " + String(servoAngleAr));
    return;
  }

  float logVal = log(ratio);
  float theta = thetaOffset + (180.0 - thetaOffset) * I * controlFactor * logVal * (T / tMed);
  theta = constrain(theta, 0, 180);

  Serial.print("log(ratio): "); Serial.println(logVal, 2);
  Serial.print("T/tMed: "); Serial.println(T / tMed, 2);
  Serial.print("Formula before constrain: "); Serial.println(thetaOffset + (180.0 - thetaOffset) * I * controlFactor * sampleInterval * logVal * (T / tMed) * 0.001, 2);

  servo.write((int)theta);
  dtostrf(theta, 4, 2, servoAngleAr);
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
    }
  } else if (strcmp(topic, "ADMIN-LDR-SEND-INTERVAL") == 0) {
    float newWait = atof(payloadCharAr) * 1000;
    if (newWait >= 30000) {
      averageWait = newWait;
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
  } else {
    Serial.println("Unknown topic");
  }
}

