# FarmHub Supervisor

## Overview
The **FarmHub Supervisor** is the central management node for the FarmHub system. Based on the **Seeed Studio XIAO ESP32C3**, its primary role is to manage the power state of the 4G router and monitor system health (battery voltage and ambient light). It ensures the system operates efficiently, protecting the battery from deep discharge and reducing power consumption at night.

## Features
- **Power Management:** Automatically toggles the 4G router based on battery voltage and daylight.
- **Battery Monitoring:** Reads LFP (Lithium Iron Phosphate) battery voltage with precision.
- **Environment Sensing:** Monitors ambient light levels using an LDR to determine day/night cycles.
- **AWS IoT Integration:** Sends real-time telemetry (voltage, LDR raw values, router status, uptime) to AWS IoT Core.
- **Anti-Seesaw Logic:** Implements a cooldown timer (5 minutes) to prevent rapid power toggling.
- **Self-Healing:** Automatically reboots if WiFi or MQTT connection fails or if DNS issues are detected.

## Hardware Configuration
| Component | Pin (GPIO) | Pin (Label) | Description |
|-----------|------------|-------------|-------------|
| **Battery Sense** | GPIO 2 | D0 | Analog input for voltage divider |
| **LDR Sensor** | GPIO 3 | D1 | Analog input for light levels |
| **Router Relay** | GPIO 4 | D2 | Output to control router power |

### Voltage Divider
- **Ratio:** 5.48
- **LFP Critical Threshold:** 12.5V (Emergency Shutdown)
- **LFP Recovery Threshold:** 13.1V (Power On)

## Communication
- **WiFi:** Connects to the local 4G router.
- **MQTT:** Uses `PubSubClient` to communicate with AWS IoT Core.
- **Port:** 8883 (Secure MQTT).
- **Topic:** `farm/telemetry`

## Telemetry Payload (JSON)
```json
{
  "device_id": "FarmHub_XIAO_01",
  "v_bat": "13.20",
  "ldr_raw": 2500,
  "router_status": 1,
  "uptime": 3600
}
```

## Setup & Deployment
1. **Secrets:** Copy `src/example_secrets.h` to `src/secrets.h` and fill in your WiFi credentials and AWS IoT certificates.
2. **PlatformIO:** Use the provided `platformio.ini` to build and upload to a Seeed XIAO ESP32C3.
3. **Intervals:** Default telemetry interval is set to 10 minutes.
