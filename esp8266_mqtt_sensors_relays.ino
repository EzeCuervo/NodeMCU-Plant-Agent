/*  
 *  ESP8266 + DHT11/22 + 4 RELAYS + MQTT + Home Assistant
 *  Author: EzeLibrandi
 *  Date: 05/08/2020
 *  Version 1.0
 *  Tested and running with the following libraries/boards:
 *  DHT Library v1.3.1 
 *  Adafruit Unified Sensors v1.0.3
 *  ES9266 Board v2.6.1
 *  Board on Arduino IDE: NodeMCU 1.0
 *  
 *  Lolin NodeMCUv3 - CH340 Driver
 */

// NOTE: Before upload you have to change wifi/mqtt and place on mqtt topics. i.e. place could be bedroom

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

//Set debug mode (true = display log message via serial)
bool debug = false;  

// Wifi Settings
#define wifi_ssid "SSID name"
#define wifi_password "wifi passowrd"

//MQTT Broker Settings
#define mqtt_server "XXX.XXX.XXX.XXX"
#define mqtt_user "mqtt username"
#define mqtt_password "mqtt password"

//MQTT Topics
#define temperature_topic "place/sensor/temperature"
#define humidity_topic "place/sensor/humidity"

//Buffer to decode MQTT messages
char message_buff[100];
long lastMsg = 0;   
long lastRecu = 0;

//DHTPIN and DHTTYPE Settings ( Un-comment you sensor)
#define DHTPIN D1               
#define DHTTYPE DHT11           // DHT 11 
//#define DHTTYPE DHT22         // DHT 22  (AM2302)

// Assign output variables to GPIO pins for relays
const int switch1 = 4;          // GPIO5 D2
const int switch2 = 13;         // GPIO2 D7
const int switch3 = 14;         // GPIO4 D5
const int switch4 = 12;         // GPIO0 D6

// Create abjects and commit setup
DHT dht(DHTPIN, DHTTYPE);     
WiFiClient espClient;
PubSubClient client(espClient);
void setup() {
  Serial.begin(9600);     
  //initialize the switch as an output and set to LOW (off)
  pinMode(switch1, OUTPUT);               // Relay Switch 1
  digitalWrite(switch1, LOW);
  pinMode(switch2, OUTPUT);               // Relay Switch 2
  digitalWrite(switch2, LOW);
  pinMode(switch3, OUTPUT);               // Relay Switch 3
  digitalWrite(switch3, LOW);
  pinMode(switch4, OUTPUT);               // Relay Switch 4
  digitalWrite(switch4, LOW);
  setup_wifi();                           // Connect to Wifi network
  client.setServer(mqtt_server, 1883);    // Configure MQTT connection
  client.setCallback(callback);           // Callback function to execute when a MQTT message   
  dht.begin();
}

//Conect to WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi OK!");
  Serial.print("ESP8266 IP address: ");
  Serial.println(WiFi.localIP());
}

//Try-reconnect function
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker... ");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected OK!");
      client.subscribe("place/switch1");
      client.subscribe("place/switch2");
      client.subscribe("place/switch3");
      client.subscribe("place/switch4");
    } else {
      Serial.print("Error: ");
      Serial.println(client.state());
      Serial.println("Wait 5 secondes before to retry...");
      delay(5000);
    }
  }
}

//Loop main code
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  
  long now = millis();
  // Send a message every minute
  if (now - lastMsg > 1000 * 60) {
    lastMsg = now;
    // Read humidity & temp (Celcius)
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    
    // Oh, nothing to send
    if ( isnan(t) || isnan(h)) {
      Serial.println("No info, please check DHT sensor!");
      return;
    }
    if ( debug ) {
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.print(" & Humidity: ");
      Serial.println(h);
    }  
    client.publish(temperature_topic, String(t).c_str(), true);   // Publish temperature on temperature_topic
    client.publish(humidity_topic, String(h).c_str(), true);      // Publish humidity on humidity_topic
  }
  if (now - lastRecu > 100 ) {
    lastRecu = now;
  }
}

// MQTT callback function
void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String message = String(message_buff);
  
  if ( debug ) {
    Serial.println("Topic: " + String(topic));
    Serial.println("Payload: " + message);
  }
  
 // Check topic and message and set status on different pins
  if (strcmp(topic,"place/switch1")==0){
    if ( message == "on" ) {
      digitalWrite(switch1,HIGH);
      client.publish("place/switch1/state", "on");
    } else if (message == "off"){
      digitalWrite(switch1,LOW);  
      client.publish("place/switch1/state", "off");
    }
  }
  if (strcmp(topic,"place/switch2")==0){
    if ( message == "on" ) {
      digitalWrite(switch2,HIGH);
      client.publish("place/switch2/state", "on");
    } else if (message == "off"){
      digitalWrite(switch2,LOW);
      client.publish("place/switch2/state", "off");
    }
  }
  if(strcmp(topic,"place/switch3")==0){
    if ( message == "on" ) {
      digitalWrite(switch3,HIGH);
      client.publish("place/switch3/state", "on");  
    } else if (message == "off"){
      digitalWrite(switch3,LOW);
      client.publish("place/switch3/state", "off"); 
    }
  }
  if (strcmp(topic,"place/switch4")==0){
    if ( message == "on" ) {
      digitalWrite(switch4,HIGH); 
      client.publish("place/switch4/state", "on"); 
    } else if (message == "off"){
      digitalWrite(switch4,LOW);
      client.publish("place/switch4/state", "off");
    }
  }
}