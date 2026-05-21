#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TinyGPSPlus.h>

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
TwoWire I2C_BME(NRF_TWIM1, NRF_TWIS1, SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn, D3, D2);
Adafruit_BME280 bme; 
TinyGPSPlus gps;


#define PIN_REED D1   
#define PIN_BTN1 D9   
#define PIN_BTN2 D10  

const float CIRCUMFERENCE_M = 2.234; 
volatile unsigned long lastPulseTime = 0;
volatile float speedKmh = 0.0;
volatile float distanceKm = 0.0;
int currentScreen = 0; 
unsigned long lastDisplayUpdate = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastButtonPress = 0;
float temp = 0.0, hum = 0.0, pres = 0.0;

void magnetInterrupt() {
  unsigned long currentTime = millis();
  unsigned long dt = currentTime - lastPulseTime;

  if (dt > 50) { 
    float dtSec = dt / 1000.0; 
    speedKmh = (CIRCUMFERENCE_M / dtSec) * 3.6; 
    distanceKm += (CIRCUMFERENCE_M / 1000.0);
    lastPulseTime = currentTime;
  }
}

void setup() {
  u8g2.begin();
  u8g2.setContrast(100);
  
  I2C_BME.begin();
  bme.begin(0x76, &I2C_BME); 
  Serial1.begin(9600); 

  pinMode(PIN_REED, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_REED), magnetInterrupt, FALLING);
}

void loop() {
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (millis() - lastWeatherUpdate > 2000) {
    temp = bme.readTemperature();
    hum = bme.readHumidity();
    pres = bme.readPressure() / 100.0F;
    lastWeatherUpdate = millis();
  }

  if (digitalRead(PIN_BTN1) == LOW && (millis() - lastButtonPress > 200)) {
    currentScreen = (currentScreen + 1) % 3; 
    lastButtonPress = millis();
  }

  if (millis() - lastPulseTime > 3000) {
    speedKmh = 0.0;
  }

  if (millis() - lastDisplayUpdate > 250) {
    lastDisplayUpdate = millis();
    u8g2.clearBuffer();

    switch (currentScreen) {
      case 0: 
        u8g2.setFont(u8g2_font_ncenB14_tr); 
        u8g2.setCursor(5, 25);
        u8g2.print(speedKmh, 1);
        u8g2.print(" km/h");
        
        u8g2.setFont(u8g2_font_ncenB10_tr); 
        u8g2.setCursor(5, 55);
        u8g2.print("Dyst: ");
        u8g2.print(distanceKm, 2);
        u8g2.print(" km");
        break;

      case 1: 
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(5, 15); u8g2.print("Temp: "); u8g2.print(temp, 1); u8g2.print(" C");
        u8g2.setCursor(5, 35); u8g2.print("Wilg: "); u8g2.print(hum, 0); u8g2.print(" %");
        u8g2.setCursor(5, 55); u8g2.print("Cisn: "); u8g2.print(pres, 1); u8g2.print(" hPa");
        break;

      case 2: 
        u8g2.setFont(u8g2_font_ncenB10_tr);
        if (gps.location.isValid()) {
          u8g2.setCursor(5, 25); u8g2.print("Lat: "); u8g2.print(gps.location.lat(), 5);
          u8g2.setCursor(5, 45); u8g2.print("Lng: "); u8g2.print(gps.location.lng(), 5);
        } else {
          u8g2.setCursor(5, 30); u8g2.print("Szukam GPS...");
          u8g2.setCursor(5, 50); u8g2.print("Satelity: "); u8g2.print(gps.satellites.value());
        }
        break;
    }
    u8g2.sendBuffer();
  }
}