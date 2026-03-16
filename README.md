## Smart Environment Monitoring and Control (ESP32 + MQTT)

This project reads environmental data from multiple sensors using an ESP32, controls actuators based on rules, and sends sensor data to MQTT topics for Node-RED/dashboard integration.

## Project Structure

1. `esp32_code/` - ESP32 Arduino sketch (`sketch.ino`)
2. `nodered_flow/` - Node-RED flow export (JSON)
3. `diagrams/` - Wiring diagrams
4. `screenshots/` - Dashboard and demo screenshots

## Hardware Used

1. ESP32 Dev Board
2. DHT22 temperature/humidity sensor
3. LDR (photoresistor) + resistor divider
4. PIR motion sensor (HC-SR501)
5. HC-SR04 ultrasonic sensor
6. Red, Yellow, Green LEDs
7. Buzzer
8. Servo motor (SG90)
9. Relay module

## Pin Mapping

| Device | ESP32 Pin |
|---|---|
| DHT22 Data | `GPIO 4` |
| LDR Analog Input | `GPIO 34` |
| PIR Output | `GPIO 26` |
| HC-SR04 Trig | `GPIO 27` |
| HC-SR04 Echo | `GPIO 14` |
| Red LED | `GPIO 16` |
| Yellow LED | `GPIO 17` |
| Green LED | `GPIO 5` |
| Buzzer | `GPIO 18` |
| Servo Signal | `GPIO 19` |
| Relay IN | `GPIO 21` |

## Required Arduino Libraries

Install these from Arduino IDE Library Manager:

1. `PubSubClient` by Nick O'Leary
2. `DHT sensor library` by Adafruit
3. `ESP32Servo` by Kevin Harrington / John K. Bennett

ESP32 core dependency:

1. Install `ESP32 by Espressif Systems` from Boards Manager

## Software Setup

1. Open `esp32_code/sketch.ino` in Arduino IDE or VS Code Arduino extension.
2. Select board: `ESP32 Dev Module`.
3. Confirm WiFi settings in code:
	`WIFI_SSID`, `WIFI_PASSWORD`
4. Confirm MQTT settings in code:
	`MQTT_SERVER`, `MQTT_PORT`, `MQTT_CLIENT_ID`
5. Compile and upload.
6. Open Serial Monitor at `115200` baud.

## Setup Commands

This project is run in Wokwi, so no local Arduino CLI/board installation is required.

## Wokwi + Node-RED + Mosquitto Quick Start

If this is your exact stack, use this flow.

### 1) Install and Start Mosquitto (Local)

Install Mosquitto from the official installer:

1. https://mosquitto.org/download/

Start broker in terminal:

```powershell
mosquitto -v
```

### 2) Install and Start Node-RED (Local)

```powershell
npm install -g --unsafe-perm node-red
node-red
```

Open Node-RED at `http://127.0.0.1:1880`.

### 3) Install Node-RED Dashboard Nodes

From Node-RED menu: `Manage palette -> Install`

1. `node-red-dashboard`
2. `node-red-node-ui-table` (optional for alert/history table)

### 4) Verify Mosquitto is receiving data

```powershell
mosquitto_sub -h localhost -t "johmmmsensors/#" -v
```

### 5) Important Wokwi broker note

Wokwi usually cannot reach your local machine broker directly (`localhost` or LAN IP).

Use one of these options:

1. Keep sketch broker as public broker (current code uses `broker.hivemq.com`).
2. Expose local Mosquitto with a public tunnel/domain and set `MQTT_SERVER` to that host.

### 6) Minimal Node-RED MQTT setup

1. Add `mqtt in` nodes for:
	`johmmmsensors/temperature`, `johmmmsensors/humidity`, `johmmmsensors/light`, `johmmmsensors/motion`
2. Connect each to a `json` node.
3. Map values in `function` node if needed, then connect to gauges/charts/text widgets.
4. For controls, publish to:
	`actuators/led/red`, `actuators/led/yellow`, `actuators/led/green`, `actuators/buzzer`, `actuators/relay`, `actuators/servo`

## Wokwi Setup

1. Use WiFi credentials in sketch:
	`WIFI_SSID = "Wokwi-GUEST"`
	`WIFI_PASSWORD = ""`
2. Use a public MQTT broker or your reachable broker host.
3. Ensure LDR is wired as a voltage divider into `GPIO34`.

LDR divider example:

1. `3.3V -> LDR -> junction`
2. `junction -> GPIO34`
3. `junction -> 10k resistor -> GND`

## Current MQTT Topics in Sketch

Published by ESP32:

1. `johmmmsensors/temperature`
2. `johmmmsensors/humidity`
3. `johmmmsensors/light`
4. `johmmmsensors/motion`

Subscribed actuator topics:

1. `actuators/led/red`
2. `actuators/led/yellow`
3. `actuators/led/green`
4. `actuators/buzzer`
5. `actuators/relay`
6. `actuators/servo`

## Default Rule Logic in `sketch.ino`

1. Red LED and relay turn on when temperature exceeds `tempMax` (`30.0`).
2. Yellow LED turns on when light is below `lightMin` (`500`).
3. Green LED is on when normal (not hot and not dark).
4. Buzzer turns on for 2 seconds on motion rising edge.
5. Servo moves to open angle when distance is below `distMin` (`10.0 cm`).

## Troubleshooting

1. If LDR always reads `4095` or `0`, check divider wiring and common ground.
2. If MQTT does not connect, verify broker address and network reachability.
3. If servo jitters, use stable 5V supply and common ground.
4. If buzzer behavior is inverted, reverse logic for your buzzer type (active-high vs active-low).


