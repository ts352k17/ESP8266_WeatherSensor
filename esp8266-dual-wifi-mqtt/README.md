# ESP8266 Dual WiFi with EEPROM and MQTT Configuration

This firmware enables an ESP8266 to connect to one of two stored WiFi networks with automatic fallback, configurable via MQTT. It reads temperature and humidity from a DHT11 sensor, supports OTA updates, and uses deep sleep for power saving.

## Features

- Stores two WiFi SSID/password sets in EEPROM  
- MQTT server configuration and update interval stored in EEPROM  
- Automatic WiFi fallback between two networks  
- Publishes temperature and humidity data to MQTT topics  
- Subscribes to MQTT topics for:  
  - Changing measurement interval  
  - Enabling/disabling OTA updates  
  - Updating WiFi credentials dynamically  
- OTA firmware update support  
- Deep sleep mode between measurements  

## Getting Started

### Prerequisites

- ESP8266 board (e.g., NodeMCU, Wemos D1 Mini)  
- DHT11 temperature and humidity sensor connected to GPIO14 (D5)  
- MQTT broker running (e.g., Mosquitto)  

### Installation

1. Flash the firmware to your ESP8266.  
2. On first run, default credentials and settings will be saved to EEPROM:  
   - WiFi1: SSID=`WLAN1`, Password=`pass1`  
   - WiFi2: SSID=`WLAN2`, Password=`pass2`  
   - MQTT server: `192.168.178.xxx`  
   - Update interval: 60 seconds  
3. The ESP8266 will attempt to connect to WiFi1, fallback to WiFi2 if needed.  
4. Sensor data will be published to MQTT topics.  

## MQTT Topics

| Topic           | Payload                              | Description                      |
|-----------------|------------------------------------|---------------------------------|
| `esp8266/temperature` | float (e.g., `22.45`)                | Published temperature readings   |
| `esp8266/humidity`    | float (e.g., `55.12`)                | Published humidity readings      |
| `esp8266/config`      | int (seconds between 10 and 86400)  | Set measurement update interval  |
| `esp8266/ota`         | `"on"` or `"off"`                    | Enable or disable OTA mode       |
| `esp8266/wifi1`       | `SSID;PASSWORD`                     | Update WiFi1 credentials         |
| `esp8266/wifi2`       | `SSID;PASSWORD`                     | Update WiFi2 credentials         |

## OTA Updates

- Enable OTA via MQTT by publishing `"on"` to `esp8266/ota`.  
- The device enters OTA mode for 3 minutes, allowing firmware upload.  
- Disable OTA by publishing `"off"`.  

## Example MQTT Messages

```bash
mosquitto_pub -t esp8266/config -m 120
mosquitto_pub -t esp8266/ota -m on
mosquitto_pub -t esp8266/wifi1 -m "MySSID;MyPassword"
mosquitto_pub -t esp8266/wifi2 -m "OtherSSID;OtherPassword"
