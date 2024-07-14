#include "secrets.h"
#include <time.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <NTPClient.h>
 
#include "DHT.h"
#define DHTPIN 15
#define DHTTYPE DHT22
#define LOOPTIME 60 * 5000 // Five Minutes

#define NTPSERVER "id.pool.ntp.org"
 
#define AWS_IOT_PUBLISH_TOPIC   "dev/dht"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
 
float h;
float t;

DHT dht(DHTPIN, DHTTYPE);
 
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTPSERVER);

void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}

void publishMessage()
{
  Serial.println("Publishing message to AWS IoT Core...");
  StaticJsonDocument<200> doc;

 /* 
    Timestamp Config ( Convertable To JS Date )

    --- Starts Here ---
  */

  timeClient.update();

  int epochtime = timeClient.getEpochTime();
  struct tm *ptm = localtime((time_t *)&epochtime);

  int currentYear = ptm->tm_year + 1900;
  int currentMonth = ptm->tm_mon + 1;
  int monthDay = ptm->tm_mday;
  int hours = timeClient.getHours();
  int minu = timeClient.getMinutes();
  int sece = timeClient.getSeconds();

  String time = String(currentYear) + "-" + 
             (currentMonth < 10 ? "0" : "") + String(currentMonth) + "-" + 
             (monthDay < 10 ? "0" : "") + String(monthDay) + "T" + 
             (hours < 10 ? "0" : "") + String(hours) + ":" + 
             (minu < 10 ? "0" : "") + String(minu) + ":" + 
             (sece < 10 ? "0" : "") + String(sece) + "Z";

  /* 
    Timestamp Config ( Convertable To JS Date )
    
    --- Ends Here ---
  */

  doc["timestamp"] = time;
  doc["origin"] = THINGNAME;
  doc["humidity"] = h;
  doc["temperature"] = t;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  Serial.print("Publishing message: ");
  Serial.println(jsonBuffer);
 
  if (!client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer))
  {
    Serial.println("Error publishing message to AWS IoT Core!");
  }
  else
  {
    Serial.println("Message published to AWS IoT Core successfully!");
  }
}
 
void setup()
{
  Serial.begin(115200);
  connectAWS();
  dht.begin();
  timeClient.begin();
}
 
void loop()
{
  h = dht.readHumidity();
  t = dht.readTemperature();
 
  if (isnan(h) || isnan(t) )
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));

  if (!client.connected()) {
    Serial.println("Disconnected...");
    Serial.println("Reconnecting...");
    connectAWS();
  }
 
  publishMessage();
  client.loop();
  delay(LOOPTIME);
}