#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <DHT.h>
#include <Wire.h>
#define DHTPIN 15     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 11

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

float h;
float t;

void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Incoming message from topic: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);
  const char* message = doc["message"];
  Serial.println(message);
}

DHT dht(DHTPIN, DHTTYPE);

WiFiClientSecure net;
PubSubClient client(net);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org");

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin("Wokwi-GUEST", "");

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("Not Connected");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // Create a message handler
  client.setCallback(messageHandler);

  Serial.println("Connecting to AWS IoT");

  while (!client.connected())
  {
    Serial.print(".");
    if (client.connect(THINGNAME))
    {
      Serial.println("Connected to AWS IoT");
      client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
    }
    else
    {
      Serial.println("AWS IoT Connection Failed! Retrying...");
      delay(1000);
    }
  }
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["timestamp"] = timeClient.getEpochTime() + (7 * 3600);  
  doc["humidity"] = h;
  doc["temperature"] = t;
  char jsonBuffer[512];

  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup()
{
  Serial.begin(115200);
  connectAWS();
  dht.begin();
  Wire.begin();
  timeClient.begin();
  timeClient.setTimeOffset(0);
}
void loop()
{
  h = dht.readHumidity();
  t = dht.readTemperature();

  timeClient.update();
  if (isnan(h) || isnan(t))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected");
  } else {
    Serial.println("WiFi Not Connected");
  }

  publishMessage();
  client.loop();
  delay(5000);
}