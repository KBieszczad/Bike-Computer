#pragma once

#include <Arduino.h>

#define PIN_REED D1   
#define PIN_BTN1 D10   
#define PIN_BTN2 D9  

#define GPS_BAUD 9600

enum Screen {
  SCREEN_MAIN = 0,
  SCREEN_ENV,
  SCREEN_HISTORY,
  SCREEN_GPS,
  SCREEN_WHEEL,
  SCREEN_POWER,
  SCREEN_COUNT
};

enum PowerMode {
  MODE_NORMAL = 0,
  MODE_ECO,
  MODE_ULTRA_ECO
};

struct TripRecord {
  float distanceKm;
  float speedAvgKmh;
  uint32_t durationSec;
};

#define MAX_HISTORY 5

struct SystemSettings {
  uint16_t wheelCircumferenceMm;
  PowerMode powerMode;
  bool gpsEnabled;
  uint8_t padding; 
  TripRecord history[MAX_HISTORY];
  uint8_t historyCount;
};

extern SystemSettings settings;
extern Screen currentScreen;

extern volatile float speedKmh;
extern volatile float distanceTripKm;
extern float temp, hum, pres;
extern uint8_t batteryPercent;
extern bool gpsFix;

#include <U8g2lib.h>
#include <TinyGPSPlus.h>
extern U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2;
extern TinyGPSPlus gps;

void saveData();
void loadData();
void switchPowerMode(PowerMode newMode);
