

#define I2C_SDA_PIN 4 // GPIO4 = pin D2 = pin 19
#define I2C_SCL_PIN 5 // GPIO5 = pin D1 = pin 20
#define HDC302X_ADDRESS 0x44

// Arduino Example for basic usage of the HDC302x Lib
#include "Arduino.h"
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "secrets.h"


TwoWire wire = TwoWire();
uint8_t buffer[6] = {0};

const char* temp_topic = "home/bed/temperature";
const char* humi_topic = "home/bed/humidity";

WiFiClient espClient;
PubSubClient client(espClient);

boolean sendDiscovery();

void setup()
{
    Serial.begin(9600);                       // Init the serial device

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("connected");

    client.setServer(mqtt_server, 1883);

    wire.pins(I2C_SDA_PIN, I2C_SCL_PIN);

    wire.begin();
}

void loop()
{
    Wire.beginTransmission(HDC302X_ADDRESS);
    Wire.write(0x24);
    Wire.write(0x00);
    Wire.endTransmission();

    delay(20);

    Wire.requestFrom(HDC302X_ADDRESS, 6);

    int buffer_idx = 0;
    while (Wire.available()) {
      char c = Wire.read();
      buffer[buffer_idx] = c;
      buffer_idx++;
    }

    float temperature = 0.0;
    float humidity = 0.0;

    if (buffer_idx != 6) {
        Serial.print("Got "); Serial.print(buffer_idx); Serial.println(" bytes");
    }
    else
    {
        uint16_t humidity_raw = buffer[3] << 8;   
        humidity_raw = (humidity_raw + buffer[4]);
        humidity = (((float)(humidity_raw)) / 65535) * 100;  // conversion on the HDC3x datasheet

        uint16_t temp_raw = (buffer[0] << 8);
        temp_raw = (temp_raw + buffer[1]);   
        temperature = ((float)(temp_raw) / 65535) * (175) - 45;

        Serial.print("Temp: ");  
        Serial.print(temperature);        
        Serial.print("°C, Humidity: "); 
        Serial.println(humidity);         
    }

    if (!client.connected()) {
        boolean connect_ret = client.connect("ArduinoClient", mqtt_user, mqtt_pass);

        if (connect_ret) {
            Serial.println("MQTTConnected");

            boolean pub_ret = sendDiscovery();

            if (pub_ret) {
                Serial.println("Published config");
            }
            else {
                Serial.println("Failed config");
            }

            pub_ret = client.publish(temp_topic, String(temperature).c_str());

            if (pub_ret) {
                Serial.println("Published temp");
            }
            else {
                Serial.println("Failed pub");
            }

            pub_ret = client.publish(humi_topic, String(humidity).c_str());

            if (pub_ret) {
                Serial.println("Published humidity");
            }
            else {
                Serial.println("Failed pub humidity");
            }

            client.disconnect();
        }
        else {
            Serial.println("Failed connect");
        }
    }    

    ESP.deepSleep(2*1000000, RF_DEFAULT);
}


boolean sendDiscovery() {
  JSONVar config;

  config["name"] = "Arduino Temperature";
  config["state_topic"] = temp_topic;
  config["unit_of_measurement"] = "°C";
  config["unique_id"] = "arduino_temp_1";

  JSONVar device;
  device["identifiers"] = "arduino_device_001";
  device["name"] = "My Arduino Sensor";
  device["manufacturer"] = "Custom";
  device["model"] = "Arduino MQTT";
  device["sw_version"] = "1.0";

  config["device"] = device;

  String payload = JSON.stringify(config);

  if (payload.length() > MQTT_MAX_PACKET_SIZE) {
    Serial.println("Payload too long");
    return false;
  }

  boolean ret = client.publish("homeassistant/sensor/arduino_temp/config", payload.c_str(), true);

  if (!ret) {
    return false;
  }

  JSONVar humConfig;

  humConfig["name"] = "Arduino Humidity";
  humConfig["state_topic"] = humi_topic;
  humConfig["unit_of_measurement"] = "%";
  humConfig["unique_id"] = "bed_humidity_1";

  humConfig["device"] = device;

  String humPayload = JSON.stringify(humConfig);
  return client.publish("homeassistant/sensor/arduino_humidity/config", humPayload.c_str(), true);
}
