#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TinyGPSPlus.h>
#include <Bounce2.h>
#include "Globals.h"
#include "Bitmaps.h"

SystemSettings settings = {2155, MODE_NORMAL, true, 0};
Screen currentScreen = SCREEN_MAIN;

volatile float speedKmh = 0.0;
volatile float distanceTripKm = 0.0;
volatile uint32_t tripDurationSec = 0;
float temp = 0.0, hum = 0.0, pres = 0.0;
uint8_t batteryPercent = 100;
bool gpsFix = false;

struct GpsPoint {
  float lat;
  float lon;
  float alt;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hr;
  uint8_t min;
  uint8_t sec;
};
unsigned long lastGpsSaveTime = 0;

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
TinyGPSPlus gps;
Adafruit_BME280 bme; 
TwoWire I2C_BME(NRF_TWIM1, NRF_TWIS1, SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn, D3, D2); 

Bounce2::Button btn1 = Bounce2::Button();
Bounce2::Button btn2 = Bounce2::Button();

unsigned long lastDisplayUpdate = 0;
unsigned long lastWeatherUpdate = 0;
volatile unsigned long lastReedPulseTime = 0;
volatile uint8_t pulseAccumulator = 0;
volatile unsigned long isrDt = 0;


int gpsMenuCursor = 0;
int historyCursor = 0;
const uint16_t wheelSizes[] = {2096, 2105, 2125, 2136, 2145, 2155, 2205, 2235, 2288};
const int NUM_WHEEL_SIZES = sizeof(wheelSizes) / sizeof(wheelSizes[0]);
int wheelSizeCursor = 5;
unsigned long lastSecondTick = 0;

float getSpeed() {
  noInterrupts();
  float v = speedKmh;
  interrupts();
  return v;
}

float getDistance() {
  noInterrupts();
  float d = distanceTripKm;
  interrupts();
  return d;
}

uint32_t getDuration() {
  noInterrupts();
  uint32_t t = tripDurationSec;
  interrupts();
  return t;
}

void resetTrip() {
  noInterrupts();
  distanceTripKm = 0.0;
  tripDurationSec = 0;
  speedKmh = 0.0;
  lastReedPulseTime = millis();
  pulseAccumulator = 0;
  interrupts();
}

void magnetInterrupt() {
  unsigned long currentTime = millis();
  unsigned long dt = currentTime - lastReedPulseTime;
  if (dt > 80) { 
    isrDt = dt;
    pulseAccumulator++;
    lastReedPulseTime = currentTime;
  }
}
#include <InternalFileSystem.h>
#include <Adafruit_LittleFS.h>
using namespace Adafruit_LittleFS_Namespace;

void saveData() {
  InternalFS.remove("/settings.dat");
  auto file = InternalFS.open("/settings.dat", FILE_O_WRITE);
  if (file) {
    file.write((uint8_t*)&settings, sizeof(SystemSettings));
    file.close();
  }
}

void loadData() {
  InternalFS.begin();
  auto file = InternalFS.open("/settings.dat", FILE_O_READ);
  if (file) {
    if (file.size() == sizeof(SystemSettings)) {
      file.read((uint8_t*)&settings, sizeof(SystemSettings));
      file.close();
    } else {
      file.close();
      InternalFS.remove("/settings.dat");
      saveData();
    }
  } else {
    saveData();
  }
  
  for (int i = 0; i < NUM_WHEEL_SIZES; i++) {
    if (wheelSizes[i] == settings.wheelCircumferenceMm) {
      wheelSizeCursor = i;
      break;
    }
  }
}

void switchPowerMode(PowerMode newMode) {
  settings.powerMode = newMode;
  if (newMode == MODE_NORMAL) {
    // bme, gps wake
    bme.setSampling(Adafruit_BME280::MODE_NORMAL, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::FILTER_OFF, 
                    Adafruit_BME280::STANDBY_MS_0_5);
    Serial1.print("\r\n"); 
    settings.gpsEnabled = true;
  } else if (newMode == MODE_ECO) {
    // bme wake, gps sleep
    bme.setSampling(Adafruit_BME280::MODE_NORMAL, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::FILTER_OFF, 
                    Adafruit_BME280::STANDBY_MS_0_5);
    Serial1.print("$PCAS04,2*1A\r\n");
    settings.gpsEnabled = false;
  } else if (newMode == MODE_ULTRA_ECO) {
    // bme, gps sleep
    bme.setSampling(Adafruit_BME280::MODE_SLEEP, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::SAMPLING_X16, 
                    Adafruit_BME280::FILTER_OFF, 
                    Adafruit_BME280::STANDBY_MS_0_5);
    Serial1.print("$PCAS04,2*1A\r\n");
    settings.gpsEnabled = false;
  }
}

void drawMainScreen() {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    if (batteryPercent > 80) {
        u8g2.drawXBMP(100, 2, 24, 16, image_battery_100_bits);
    } else if (batteryPercent > 45) {
        u8g2.drawXBMP(100, 2, 24, 16, image_battery_67_bits);
    } else if (batteryPercent > 15) {
        u8g2.drawXBMP(100, 2, 24, 16, image_battery_33_bits);
    } else {
        u8g2.drawXBMP(62, 2, 62, 13, image_battery_10_bits); 
    }

    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.setCursor(43, 57);
    u8g2.print(getDistance(), 1); u8g2.print(" km");
  
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.setCursor(10, 41);
    u8g2.print(getSpeed(), 1); u8g2.print(" km/h");
    
    if (gpsFix) {
        u8g2.drawXBMP(58, 1, 14, 16, image_checked_bits);
    } else {
        u8g2.drawXBMP(58, 1, 11, 16, image_crossed_bits);
    }
    
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.setCursor(6, 13);
    u8g2.print((int)temp); u8g2.print(" C");
}

void drawEnvScreen() {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(3, 16, "Temperature:");
    u8g2.drawStr(4, 35, "Pressure:");
    u8g2.drawStr(4, 54, "Humidity:");
    
    u8g2.setCursor(92, 16); u8g2.print(temp, 1);
    u8g2.setCursor(70, 35); u8g2.print(pres, 0); u8g2.print(" hPa");
    u8g2.setCursor(71, 54); u8g2.print(hum, 0); u8g2.print(" %");
}

void drawHistoryScreen() {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(29, 13, "LAST ROUTE");
    u8g2.drawStr(6, 29, "Distance:");
    u8g2.drawStr(5, 45, "AVG Speed:");
    u8g2.drawStr(5, 60, "Time:");
    
    if (settings.historyCount > 0) {
      TripRecord &rec = settings.history[historyCursor];
      u8g2.setCursor(80, 29); u8g2.print(rec.distanceKm, 1);
      u8g2.setCursor(80, 45); u8g2.print(rec.speedAvgKmh, 1); 
      
      uint32_t h = rec.durationSec / 3600;
      uint8_t m = (rec.durationSec % 3600) / 60;
      uint8_t s = rec.durationSec % 60;
      char timeBuf[10];
      sprintf(timeBuf, "%02lu:%02u:%02u", (unsigned long)h, m, s);
      u8g2.setCursor(55, 60); u8g2.print(timeBuf); 
      
      u8g2.setCursor(105, 13);
      u8g2.print(historyCursor + 1); u8g2.print("/"); u8g2.print(settings.historyCount);
    } else {
      u8g2.setCursor(80, 45); u8g2.print("---");
    }
}

void drawGpsScreen() {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(22, 13, "GPS SETTINGS");
    
    const char* options[] = {"GPS: ON", "EXPORT ROUTE", "DELETE ROUTE"};
    if (!settings.gpsEnabled) options[0] = "GPS: OFF";
    
    int startIdx = gpsMenuCursor;
    if (startIdx == 2) startIdx = 1; 
    
    u8g2.setCursor(7, 32);
    if (gpsMenuCursor == startIdx) u8g2.print("> ");
    u8g2.print(options[startIdx]);
    
    u8g2.setCursor(7, 53);
    if (gpsMenuCursor == startIdx + 1) u8g2.print("> ");
    u8g2.print(options[startIdx + 1]);
}

void drawWheelScreen() {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(33, 13, "SET WHEEL");
    
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.setCursor(22, 44);
    u8g2.print(settings.wheelCircumferenceMm); u8g2.print(" mm");
}

void drawPowerScreen() {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont17_tr);
    u8g2.drawStr(20, 14, "Power mode");
    
    u8g2.setCursor(24, 42);
    if (settings.powerMode == MODE_NORMAL) u8g2.print("NORMAL");
    else if (settings.powerMode == MODE_ECO) u8g2.print("ECO");
    else u8g2.print("Ultra ECO");
}

void setup() {
  Serial.begin(115200);
  
  analogReadResolution(12); 
  
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setContrast(100);
  
  I2C_BME.begin();
  bme.begin(0x76, &I2C_BME); 
  
  Serial1.begin(GPS_BAUD); 

  btn1.attach(PIN_BTN1, INPUT_PULLUP);
  btn1.interval(20);
  btn1.setPressedState(LOW);
  
  btn2.attach(PIN_BTN2, INPUT_PULLUP);
  btn2.interval(20);
  btn2.setPressedState(LOW);

  pinMode(PIN_REED, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_REED), magnetInterrupt, FALLING);

  loadData();
}

void loop() {
  btn1.update();
  btn2.update();

  noInterrupts();
  uint8_t pulses = pulseAccumulator;
  unsigned long dt = isrDt;
  pulseAccumulator = 0;
  interrupts();
  
  if (pulses > 0) {
    float circM = settings.wheelCircumferenceMm / 1000.0;
    float dtSec = dt / 1000.0; 
    
    noInterrupts();
    speedKmh = (circM / dtSec) * 3.6; 
    distanceTripKm += pulses * (circM / 1000.0); 
    interrupts();
  }

  if (settings.gpsEnabled) {
    while (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    gpsFix = gps.location.isValid() && gps.location.age() < 2000;

    if (gpsFix && gps.date.isValid() && gps.time.isValid()) {
      if (millis() - lastGpsSaveTime > 5000) {
        if (getSpeed() > 2.0) {
          GpsPoint pt;
          pt.lat = gps.location.lat();
          pt.lon = gps.location.lng();
          pt.alt = gps.altitude.meters();
          pt.year = gps.date.year();
          pt.month = gps.date.month();
          pt.day = gps.date.day();
          pt.hr = gps.time.hour();
          pt.min = gps.time.minute();
          pt.sec = gps.time.second();
          
          auto file = InternalFS.open("/trace.dat", FILE_O_WRITE);
          if (file) {
            file.seek(file.size());
            file.write((uint8_t*)&pt, sizeof(GpsPoint));
            file.close();
          }
        }
        lastGpsSaveTime = millis();
      }
    }
  } else {
    gpsFix = false;
  }

  if (btn1.pressed()) {
    if (currentScreen == SCREEN_WHEEL || currentScreen == SCREEN_POWER) {
      saveData();
    }
    currentScreen = (Screen)((currentScreen + 1) % SCREEN_COUNT);
  }

  static bool btn2LongPressHandled = false;
  if (btn2.pressed()) {
    btn2LongPressHandled = false;
  }

  if (currentScreen == SCREEN_MAIN) {
    if (btn2.isPressed() && btn2.currentDuration() > 2000 && !btn2LongPressHandled) {
      float dist = getDistance();
      uint32_t dur = getDuration();
      
      if (dist > 0.1) { 
        for (int i = MAX_HISTORY - 1; i > 0; i--) {
          settings.history[i] = settings.history[i-1];
        }
        settings.history[0].distanceKm = dist;
        settings.history[0].durationSec = dur;
        settings.history[0].speedAvgKmh = (dur > 0) ? (dist / (dur / 3600.0)) : 0;
        if (settings.historyCount < MAX_HISTORY) settings.historyCount++;
        saveData();
      }
      InternalFS.remove("/trace.dat"); 
      resetTrip();
      btn2LongPressHandled = true; 
    }
  } 
  else if (currentScreen == SCREEN_HISTORY) {
    if (btn2.pressed()) {
      if (settings.historyCount > 0) {
        historyCursor = (historyCursor + 1) % settings.historyCount;
      }
    }
  }
  else if (currentScreen == SCREEN_GPS) {
    if (btn2.released() && btn2.previousDuration() < 1000) {
      gpsMenuCursor = (gpsMenuCursor + 1) % 3;
    }
    if (btn2.isPressed() && btn2.currentDuration() > 1000 && !btn2LongPressHandled) {
      if (gpsMenuCursor == 0) {
        settings.gpsEnabled = !settings.gpsEnabled;
        saveData();
      } else if (gpsMenuCursor == 1) {
        // export route to serial
        auto file = InternalFS.open("/trace.dat", FILE_O_READ);
        if (file) {
          Serial.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
          Serial.println("<gpx version=\"1.1\" creator=\"BikeComputer\">");
          Serial.println("  <trk><name>Bike Route</name><trkseg>");
          GpsPoint pt;
          while (file.read((uint8_t*)&pt, sizeof(GpsPoint)) == sizeof(GpsPoint)) {
            Serial.print("    <trkpt lat=\""); Serial.print(pt.lat, 6);
            Serial.print("\" lon=\""); Serial.print(pt.lon, 6); Serial.println("\">");
            Serial.print("      <ele>"); Serial.print(pt.alt, 1); Serial.println("</ele>");
            char timeBuf[50];
            sprintf(timeBuf, "      <time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>", 
                    pt.year, pt.month, pt.day, pt.hr, pt.min, pt.sec);
            Serial.println(timeBuf);
            Serial.println("    </trkpt>");
          }
          Serial.println("  </trkseg></trk>");
          Serial.println("</gpx>");
          file.close();
        } else {
          Serial.println("No trace saved in memory.");
        }
      } else if (gpsMenuCursor == 2) {
        InternalFS.remove("/trace.dat");
      }
      btn2LongPressHandled = true;
    }
  }
  else if (currentScreen == SCREEN_WHEEL) {
    if (btn2.pressed()) {
      wheelSizeCursor = (wheelSizeCursor + 1) % NUM_WHEEL_SIZES;
      settings.wheelCircumferenceMm = wheelSizes[wheelSizeCursor];
    }
  }
  else if (currentScreen == SCREEN_POWER) {
    if (btn2.pressed()) {
      PowerMode next = (PowerMode)((settings.powerMode + 1) % 3);
      switchPowerMode(next);
    }
  }

  if (millis() - lastReedPulseTime > 3000) {
    speedKmh = 0.0;
  }
  
  if (millis() - lastSecondTick > 1000) {
    if (getSpeed() > 1.0) { 
      noInterrupts();
      tripDurationSec++;
      interrupts();
    }
    lastSecondTick = millis();
  }
  if (millis() - lastWeatherUpdate > 2000 && settings.powerMode != MODE_ULTRA_ECO) {
    temp = bme.readTemperature();
    hum = bme.readHumidity();
    pres = bme.readPressure() / 100.0F;
    
#if defined(PIN_VBAT)
    int vbat_raw = analogRead(PIN_VBAT);
#else
    int vbat_raw = analogRead(A6); 
#endif
    int pct = map(vbat_raw, 1876, 2388, 0, 100);
    batteryPercent = constrain(pct, 0, 100);

    lastWeatherUpdate = millis();
  }

  if (millis() - lastDisplayUpdate > 250) {
    lastDisplayUpdate = millis();
    u8g2.clearBuffer();

    switch (currentScreen) {
      case SCREEN_MAIN: drawMainScreen(); break;
      case SCREEN_ENV: drawEnvScreen(); break;
      case SCREEN_HISTORY: drawHistoryScreen(); break;
      case SCREEN_GPS: drawGpsScreen(); break;
      case SCREEN_WHEEL: drawWheelScreen(); break;
      case SCREEN_POWER: drawPowerScreen(); break;
      default: break;
    }
    
    u8g2.sendBuffer();
  }
}