#include <Adafruit_AHTX0.h>
#include <Wire.h>

#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

TwoWire wire = TwoWire();
Adafruit_AHTX0 aht;

void setup() {
  Serial.begin(9600);
  Serial.println("Adafruit AHT10/AHT20 demo!");

  wire.pins(I2C_SDA_PIN, I2C_SCL_PIN);

  if (! aht.begin(&wire)) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
}

void loop() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

  delay(1000);
}
