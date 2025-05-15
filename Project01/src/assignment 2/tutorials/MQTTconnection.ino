#include <PubSubClient.h>
#include <WiFi.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
  Serial.begin(115200);
  setupWifi();
}

void loop() {

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
