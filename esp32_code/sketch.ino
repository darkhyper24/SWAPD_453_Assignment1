#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>

// -----------------------------
// WiFi Configuration
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";

// -----------------------------
// MQTT Configuration
const char *MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char *MQTT_CLIENT_ID = "JohmESP32Client";

// -----------------------------
// MQTT Topics (Sensors)
const char *TOPIC_TEMP = "johmmmsensors/temperature";
const char *TOPIC_HUM = "johmmmsensors/humidity";
const char *TOPIC_LIGHT = "johmmmsensors/light";
const char *TOPIC_MOTION = "johmmmsensors/motion";
const char *TOPIC_STATUS = "johmmmsensors/status";
const char *TOPIC_HEARTBEAT = "johmmmsensors/heartbeat";

// -----------------------------
// MQTT Client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// -----------------------------
// Pin Mapping
#define DHTPIN 4
#define DHTTYPE DHT22
#define LDR_PIN 34
#define PIR_PIN 26
#define TRIG_PIN 27
#define ECHO_PIN 14

#define RED_LED 16
#define YELLOW_LED 17
#define GREEN_LED 5

#define BUZZER 18
#define SERVO_PIN 19
#define RELAY 21

// -----------------------------
float tempMax = 30.0;
int lightMin = 500;
float distMin = 10.0;

unsigned long reportIntervalMs = 1000;

int servoOpenAngle = 90;
int servoClosedAngle = 0;

DHT dht(DHTPIN, DHTTYPE);
Servo servoMotor;

// -----------------------------
unsigned long lastSampleMs = 0;
unsigned long buzzerOffAt = 0;
unsigned long lastHeartbeatMs = 0;
bool lastMotion = false;

// -----------------------------
// WiFi connection
void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());
}

// -----------------------------
// MQTT Callback
void callback(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  String myTopic = String(topic);

  if (myTopic == "actuators/led/red")
  {
    digitalWrite(RED_LED, message == "ON");
    Serial.print("Red LED Status: ");
    Serial.println(message);
  }
  else if (myTopic == "actuators/led/yellow")
  {
    digitalWrite(YELLOW_LED, message == "ON");
    Serial.print("Yellow LED Status: ");
    Serial.println(message);
  }
  else if (myTopic == "actuators/led/green")
  {
    digitalWrite(GREEN_LED, message == "ON");
    Serial.print("Green LED Status: ");
    Serial.println(message);
  }
  else if (myTopic == "actuators/buzzer")
  {
    digitalWrite(BUZZER, message == "ON");
    Serial.print("Buzzer Status: ");
    Serial.println(message);
  }
  else if (myTopic == "actuators/relay")
  {
    digitalWrite(RELAY, message == "ON");
    Serial.print("Relay Status: ");
    Serial.println(message);
  }
  else if (myTopic == "actuators/servo")
  {
    int angle = message.toInt();
    servoMotor.write(angle);
    Serial.print("Servo Angle: ");
    Serial.println(angle);
  }
}

// -----------------------------
// MQTT reconnect
void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect(MQTT_CLIENT_ID))
    {
      Serial.println("Connected");
      mqttClient.subscribe("actuators/led/red");
      mqttClient.subscribe("actuators/led/yellow");
      mqttClient.subscribe("actuators/led/green");
      mqttClient.subscribe("actuators/buzzer");
      mqttClient.subscribe("actuators/relay");
      mqttClient.subscribe("actuators/servo");
    }
    else
    {
      Serial.print("Failed rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

// -----------------------------
int readLdrAveraged()
{
  long sum = 0;
  for (int i = 0; i < 8; i++)
  {
    sum += analogRead(LDR_PIN);
    delay(2);
  }
  return sum / 8;
}

// -----------------------------
void setLedState(bool redOn, bool yellowOn, bool greenOn)
{
  digitalWrite(RED_LED, redOn);
  digitalWrite(YELLOW_LED, yellowOn);
  digitalWrite(GREEN_LED, greenOn);
}

// -----------------------------
float readDistanceCm()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration <= 0)
    return NAN;
  return duration * 0.0343 / 2;
}

// -----------------------------
void setup()
{
  Serial.begin(115200);

  pinMode(LDR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY, OUTPUT);

  setLedState(false, false, true);

  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, 500, 2400);
  servoMotor.write(servoClosedAngle);

  dht.begin();

  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  connectWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
}

// -----------------------------
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
    connectWiFi();
  if (!mqttClient.connected())
    reconnectMQTT();

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastSampleMs < reportIntervalMs)
    return;
  lastSampleMs = now;

  // --- Read Sensors ---
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int lightRaw = readLdrAveraged();
  bool motion = digitalRead(PIR_PIN);
  float distance = readDistanceCm();

  // --- Logic ---
  bool tempHigh = !isnan(temperature) && temperature > tempMax;
  bool dark = lightRaw < lightMin;
  bool nearObj = !isnan(distance) && distance < distMin;

  setLedState(tempHigh, dark, !tempHigh && !dark);

  // --- Log LED States ---
  Serial.printf("LEDs -> RED: %d | YELLOW: %d | GREEN: %d\n",
                tempHigh ? 1 : 0,
                dark ? 1 : 0,
                (!tempHigh && !dark) ? 1 : 0);

  digitalWrite(RELAY, tempHigh);

  if (motion && !lastMotion)
  {
    digitalWrite(BUZZER, HIGH);
    delay(2000);
    digitalWrite(BUZZER, LOW);
  }
  lastMotion = motion;

  servoMotor.write(nearObj ? servoOpenAngle : servoClosedAngle);

  // --- Publish Sensor Data ---
  String tempJSON = "{\"temperature\": " + String(temperature) + "}";
  String humJSON = "{\"humidity\": " + String(humidity) + "}";
  String lightJSON = "{\"light\": " + String(lightRaw) + "}";
  String motionJSON = "{\"motion\": " + String(motion ? "true" : "false") + "}";

  mqttClient.publish(TOPIC_TEMP, tempJSON.c_str());
  mqttClient.publish(TOPIC_HUM, humJSON.c_str());
  mqttClient.publish(TOPIC_LIGHT, lightJSON.c_str());
  mqttClient.publish(TOPIC_MOTION, motionJSON.c_str());

  // --- Publish System Status ---
  unsigned long uptimeSec = millis() / 1000;
  String statusJSON = "{\"uptime\": " + String(uptimeSec) +
                      ", \"rssi\": " + String(WiFi.RSSI()) +
                      ", \"freeHeap\": " + String(ESP.getFreeHeap()) + "}";

  mqttClient.publish(TOPIC_STATUS, statusJSON.c_str());

  // --- Heartbeat every 10 seconds ---
  if (now - lastHeartbeatMs >= 10000)
  {
    lastHeartbeatMs = now;
    String hbJSON = "{\"status\": \"alive\", \"uptime\": " + String(millis() / 1000) + "}";
    mqttClient.publish(TOPIC_HEARTBEAT, hbJSON.c_str());
    Serial.println("Heartbeat sent");
  }

  // --- Serial Monitor Summary ---
  Serial.printf("T=%.2f H=%.2f L=%d M=%d D=%.2f RSSI=%d Heap=%d\n",
                temperature, humidity, lightRaw, motion, distance,
                WiFi.RSSI(), ESP.getFreeHeap());
}