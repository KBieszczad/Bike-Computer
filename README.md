# Custom IoT Bike Computer

A low-power custom bicycle computer featuring speed measurement, GPS tracking, environmental sensing, and an OLED display. The hardware is designed from scratch with a custom PCB created in KiCad.

## Description

This project aims to build a standalone, power-efficient bike computer. At its core is the Seeed Studio **XIAO nRF52840**, a very energy efficient microcontroller with native Bluetooth Low Energy support.

The device combines data from a classic magnetic reed switch (for precise speed and distance tracking) with an external GPS module for recording traces. Additionally, it integrates a BME280 sensor to monitor environmental conditions (temperature, humidity, and barometric pressure).

### Technical Features
* **Super-Loop Architecture:** Runs on a non-blocking state machine utilizing hardware interrupts (ISR) and atomics for exact speed calculation and UI rendering, completely avoiding `delay()` functions.
* **Persistent Storage (LittleFS):** Automatically saves user settings, wheel circumference, and route history to the internal non-volatile flash memory.
* **GPS Trace Recording:** Caches GPS coordinates in the background while moving and writes directly to flash. Traces can be directly exported over USB Serial to a raw `.gpx` file compatible with for example Strava.
* **Dynamic Power Management:** Implements multiple power modes (Normal, Eco, Ultra Eco). The device sends standard CASIC protocol sleep commands to the GPS module and hardware sleep flags to the BME280 sensor to drastically cut power consumption when full tracking is not needed.

## Hardware Components

* **MCU:** Seeed Studio XIAO nRF52840
* **GPS Module:** ATGM336H
* **Environment Sensor:** BME280
* **Display:** 1.54" I2C OLED (SSD1309)
* **Sensors:** Magnetic Reed Switch (Normally Open)
* **Power:** 470mAh Li-Pol Battery with hardware switch

## Project Structure

```text
.
├── hardware/
│   ├── project.kicad_sch        # KiCad Schematic file
│   ├── project.kicad_pcb        # KiCad PCB layout file
│   └── img/                     # Schematic and PCB layout images
├── software/                    # PlatformIO Firmware files
└── README.md
```

## Visuals

### Schematic
![Schematics](hardware/img/schematic.png)

### PCB
![Front](hardware/img/pcb_F.png)
![Back](hardware/img/pcb_B.png)

## Installation & Build

### Prerequisites
* **Visual Studio Code** with the **PlatformIO** extension installed.
* Required libraries are automatically managed by `platformio.ini`

### Installing & Flashing
1. **Clone the repository:**
```bash
git clone https://github.com/KBieszczad/Bike-Computer.git
```
2. **Open the `software` folder** in VS Code (PlatformIO).
3. **Connect your XIAO nRF52840** via USB-C.
4. **Build and Upload** the firmware using the PlatformIO build tools.

## Usage & Configuration

1. **Navigation:**
   * **Button 1 (Left):** Short press to cycle through the available screens (Main, Environment, History, GPS, Wheel Setup, Power Mode).
   * **Button 2 (Right):** Used to interact with the current screen (e.g., scroll menus, change settings).
2. **Recording a Trip:**
   * Ensure GPS is toggled `ON` in the GPS Settings screen. The device will automatically start tracking your route once satellites are locked and movement is detected.
   * To finish and save the trip into the device's history, navigate to the **Main Screen**, press and hold **Button 2** for 2 seconds. The trip history will update and distance will reset.
3. **Exporting GPX Track:**
   * Connect the bike computer to a PC via USB.
   * Open a Serial Monitor tool (e.g., in PlatformIO) set to `115200` baud.
   * Navigate to the **GPS SETTINGS** screen on the device, select **EXPORT ROUTE**, and hold Button 2.
   * The device will output raw XML data. Copy this text, save it as a `.gpx` file, and upload it directly to Strava or GPX Studio.

## Authors
Krzysztof Bieszczad
[@KBieszczad](https://github.com/KBieszczad)
