

#define I2C_SDA_PIN 4 // GPIO4 = pin D2 = pin 19
#define I2C_SCL_PIN 5 // GPIO5 = pin D1 = pin 20
#define HDC302X_ADDRESS 0x44

// Arduino Example for basic usage of the HDC302x Lib
#include "Arduino.h"
#include <Wire.h>

TwoWire wire = TwoWire();
uint8_t buffer[6] = {0};

void setup()
{
    Serial.begin(9600);                       // Init the serial device

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

    if (buffer_idx != 6) {
        Serial.print("Got "); Serial.print(buffer_idx); Serial.println(" bytes");
    }
    else
    {
        uint16_t humidity_raw = buffer[3] << 8;   
        humidity_raw = (humidity_raw + buffer[4]);
        float humidity = (((float)(humidity_raw)) / 65535) * 100;  // conversion on the HDC3x datasheet

        uint16_t temp_raw = (buffer[0] << 8);
        temp_raw = (temp_raw + buffer[1]);   
        float temperature = ((float)(temp_raw) / 65535) * (175) - 45;

        Serial.print("Temp: ");  
        Serial.print(temperature);        
        Serial.print("°C, Humidity: "); 
        Serial.println(humidity);         
    }

    ESP.deepSleep(60*1000000, RF_DISABLED);
}
