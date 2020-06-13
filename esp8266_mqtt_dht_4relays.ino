/*  
 *  ESP8266 + DHT11/22 + MOISTURE SENSOR + 4 RELAYS + MQTT + Home Assistant
 *  Author: EzeLibrandi
 *  Based on: https://github.com/projetsdiy/esp8266-dht22-mqtt-home-assistant
 *  Date: 06/13/2020
 *  Version 1.1.0
 *  Tested and running with the following libraries/boards:
 *  DHT Library v1.3.1 
 *  Adafruit Unified Sensors v1.0.3
 *  ES8266 Board v2.6.1
 *  Board on Arduino IDE: NodeMCU 1.0
 *  
 *  Lolin NodeMCUv3 - CH340 Driver
 *  IMPORTANT: You MUST change MQTT_MAX_PACKET_SIZE to 256 on \Arduino\libraries\PubSubClient\src\PubSubClient.h
 *  https://github.com/knolleary/pubsubclient#limitations
 *  
 *  NOTE: Before upload you have to change wifi/mqtt and place on mqtt topics. i.e. place could be bedroom
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <ArduinoJson.h>

//Set debug mode (true = display log message via serial)
bool debug = false;

// Client name
#define client_name "ESP8266-01"
String place = "place";

// Wifi Settings
#define wifi_ssid "wifi ssid name"
#define wifi_password "wifi password"

//MQTT Broker Settings
#define mqtt_server "XXX.XXX.XXX.XXX"
#define mqtt_user "mqtt username"
#define mqtt_password "mqtt password"

//Buffer to decode MQTT messages
char message_buff[100];
long lastMsg = 0;
long lastRecu = 0;

//DHTPIN and DHTTYPE Settings ( Un-comment you sensor)
#define DHTPIN D1
#define DHTTYPE DHT11       // DHT 11
//#define DHTTYPE DHT22 // DHT 22  (AM2302)

// Assign output variables to GPIO pins for relays
const int switch1 = 4;           // GPIO5 D2
const int switch2 = 13;          // GPIO2 D7
const int switch3 = 14;          // GPIO4 D5
const int switch4 = 12;          // GPIO0 D6

// Create abjects and commit setup
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);
void setup()
{
  Serial.begin(9600);
  //initialize the switch as an output and set to LOW (off)
  pinMode(switch1, OUTPUT); // Relay Switch 1
  digitalWrite(switch1, LOW);
  pinMode(switch2, OUTPUT); // Relay Switch 2
  digitalWrite(switch2, LOW);
  pinMode(switch3, OUTPUT); // Relay Switch 3
  digitalWrite(switch3, LOW);
  pinMode(switch4, OUTPUT); // Relay Switch 4
  digitalWrite(switch4, LOW);
  setup_wifi();                        // Connect to Wifi network
  client.setServer(mqtt_server, 1883); // Configure MQTT connection
  client.setCallback(callback);        // Callback function to execute when a MQTT message
  dht.begin();
}

//Conect to WiFi
void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi OK!");
  Serial.print("ESP8266 IP address: ");
  Serial.println(WiFi.localIP());
}

//Try-reconnect function
void reconnect(){
  StaticJsonDocument<300> jsonBuffer;
  while (!client.connected()){
    Serial.print("Connecting to MQTT broker... ");
    if (client.connect(client_name, mqtt_user, mqtt_password)){
      Serial.println("Connected OK!");
      // MQTT Auto Discover for all switches and send actual status
      for (int i = 1; i < 5; i++){
        String switchActual = "switch";
        switchActual.concat(i);
        StaticJsonDocument<300> jsonBuffer;
        jsonBuffer["name"] = place + " " + switchActual;
        jsonBuffer["uniq_id"] = place + "_" + switchActual;
        jsonBuffer["cmd_t"] = ("homeassistant/switch/" + place + "/" + switchActual + "/set");
        jsonBuffer["stat_t"] = ("homeassistant/switch/" + place + "/" + switchActual + "/state");
        char JSONmessageBuffer[300];
        serializeJson(jsonBuffer, JSONmessageBuffer);
        client.publish(("homeassistant/switch/" + place + "/" + switchActual + "/config").c_str(), JSONmessageBuffer, true);
        int readedValue = digitalRead(atoi(switchActual.c_str()));
        if (debug){
          Serial.print("Switch: ");
          Serial.print(switchActual);
          Serial.print(" Readed value: ");
          Serial.println(readedValue);
        }
        if (readedValue == 1){
          client.publish(("homeassistant/switch/" + place + "/" + switchActual + "/state").c_str(), "OFF", true);
        } else {
          client.publish(("homeassistant/switch/" + place + "/" + switchActual + "/state").c_str(), "ON", true);
        }
        client.subscribe(("homeassistant/switch/" + place + "/" + switchActual + "/set").c_str());
      }

      //MQTT Topic - Temp
      StaticJsonDocument<300> JSONTemp;
      JSONTemp["dev_cla"] = "temperature";
      JSONTemp["name"] = place + " Temperature";
      JSONTemp["uniq_id"] = place + "_temperature";
      JSONTemp["unit_of_meas"] = "°C";
      JSONTemp["val_tpl"] = "{{ value_json.temperature}}";
      JSONTemp["stat_t"] = "homeassistant/sensor/" + place + "/sensors/state";
      char JSONmessageBuffer[300];
      serializeJson(JSONTemp, JSONmessageBuffer);
      client.publish(("homeassistant/sensor/" + place + "/sensors/config").c_str(), JSONmessageBuffer, true);

      //MQTT Topic - Humidity
      StaticJsonDocument<300> JSONHum;
      JSONHum["dev_cla"] = "humidity";
      JSONHum["name"] = place + " Humidity";
      JSONHum["uniq_id"] = place + "_humidity";
      JSONHum["unit_of_meas"] = "%";
      JSONHum["val_tpl"] = "{{ value_json.humidity}}";
      JSONHum["stat_t"] = "homeassistant/sensor/" + place + "/sensors/state";
      serializeJson(JSONHum, JSONmessageBuffer);
      client.publish(("homeassistant/sensor/" + place + "/sensorH/config").c_str(), JSONmessageBuffer, true);
    } else {
      Serial.print("Error: ");
      Serial.println(client.state());
      Serial.println("Wait 5 seconds before to retry...");
      delay(5000);
    }
  }
}

//Loop main code
void loop(){
  if (!client.connected()){
    reconnect();
  }
  client.loop();
  long now = millis();
  // Send a message every minute
  if (now - lastMsg > 1000 * 60){
    lastMsg = now;
    // Read humidity & temp (Celcius)
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Oh, nothing to send
    if (isnan(t) || isnan(h)) {
      Serial.println("No info, please check DHT sensor!");
      return;
    }
    if (debug){
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.print(" & Humidity: ");
      Serial.print(h);
    }
    //JSON with sensors info
    StaticJsonDocument<300> jsonBuffer;
    jsonBuffer["temperature"] = t;
    jsonBuffer["humidity"] = h;
    char JSONmessageBuffer2[300];
    serializeJson(jsonBuffer, JSONmessageBuffer2);
    if (debug){
      Serial.print("Message: ");
      Serial.println(JSONmessageBuffer2);
    }
    client.publish(("homeassistant/sensor/" + place + "/sensors/state").c_str(), JSONmessageBuffer2, true);
  }
  if (now - lastRecu > 100){
    lastRecu = now;
  }
}

// MQTT callback function
void callback(char *topic, byte *payload, unsigned int length)
{
  int i = 0;
  for (i = 0; i < length; i++)
  {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String message = String(message_buff);

  if (debug){
    Serial.println("Topic: " + String(topic));
    Serial.println("Payload: " + message);
  }

  // Check topic and message and set status on different pins
  if (strcmp(topic, ("homeassistant/switch/" + place + "/switch1/set").c_str()) == 0){
    if (message == "ON"){
      digitalWrite(switch1, HIGH);
      Serial.print(digitalRead(switch1));
      client.publish(("homeassistant/switch/" + place + "/switch1/state").c_str(), "ON", true);
    }
    else if (message == "OFF"){
      digitalWrite(switch1, LOW);
      Serial.print(digitalRead(switch1));
      client.publish(("homeassistant/switch/" + place + "/switch1/state").c_str(), "OFF", true);
    }
  }
  if (strcmp(topic, ("homeassistant/switch/" + place + "/switch2/set").c_str()) == 0){
    if (message == "ON"){
      digitalWrite(switch2, HIGH);
      client.publish(("homeassistant/switch/" + place + "/switch2/state").c_str(), "ON", true);
    }
    else if (message == "OFF"){
      digitalWrite(switch2, LOW);
      client.publish(("homeassistant/switch/" + place + "/switch2/state").c_str(), "OFF", true);
    }
  }
  if (strcmp(topic, ("homeassistant/switch/" + place + "/switch3/set").c_str()) == 0){
    if (message == "ON")
    {
      digitalWrite(switch3, HIGH);
      client.publish(("homeassistant/switch/" + place + "/switch3/state").c_str(), "ON", true);
    }
    else if (message == "OFF"){
      digitalWrite(switch3, LOW);
      client.publish(("homeassistant/switch/" + place + "/switch3/state").c_str(), "OFF", true);
    }
  }
  if (strcmp(topic, ("homeassistant/switch/" + place + "/switch4/set").c_str()) == 0)
  {
    if (message == "ON"){
      digitalWrite(switch4, HIGH);
      client.publish(("homeassistant/switch/" + place + "/switch4/state").c_str(), "ON", true);
    }
    else if (message == "OFF"){
      digitalWrite(switch4, LOW);
      client.publish(("homeassistant/switch/" + place + "/switch4/state").c_str(), "OFF", true);
    }
  }
}