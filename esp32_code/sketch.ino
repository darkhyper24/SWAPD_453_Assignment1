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

// MQTT Topics
const char *TOPIC_TEMP = "johmmmsensors/temperature";
const char *TOPIC_HUM = "johmmmsensors/humidity";
const char *TOPIC_LIGHT = "johmmmsensors/light";
const char *TOPIC_MOTION = "johmmmsensors/motion";

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
float tempMax = 30.0f;
int lightMin = 500;
float distMin = 10.0f;
unsigned long reportIntervalMs = 2000;
int servoOpenAngle = 90;
int servoClosedAngle = 0;

DHT dht(DHTPIN, DHTTYPE);
Servo servoMotor;

// Buzzer
const bool BUZZER_USE_TONE = true;
const int BUZZER_FREQUENCY = 2000;
const int BUZZER_RESOLUTION = 8;

unsigned long lastSampleMs = 0;
unsigned long buzzerOffAt = 0;
bool lastMotion = false;
bool buzzerIsOn = false;

// -----------------------------
// WiFi connect
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
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// MQTT reconnect
void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("Connecting to MQTT...");
    String clientId = String(MQTT_CLIENT_ID);
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Subscribe to all sensor topics
      mqttClient.subscribe(TOPIC_TEMP);
      mqttClient.subscribe(TOPIC_HUM);
      mqttClient.subscribe(TOPIC_LIGHT);
      mqttClient.subscribe(TOPIC_MOTION);
    }
    else
    {
      Serial.print("failed rc=");
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

void setLedState(bool redOn, bool yellowOn, bool greenOn)
{
  digitalWrite(RED_LED, redOn);
  digitalWrite(YELLOW_LED, yellowOn);
  digitalWrite(GREEN_LED, greenOn);
}

void buzzerOn()
{
  ledcWriteTone(BUZZER, BUZZER_FREQUENCY);
  buzzerIsOn = true;
}

void buzzerOff()
{
  ledcWriteTone(BUZZER, 0);
  buzzerIsOn = false;
}

void scheduleBuzzer(unsigned long durationMs)
{
  buzzerOn();
  buzzerOffAt = millis() + durationMs;
}

void moveServoTo(int angle)
{
  servoMotor.write(angle);
}

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
  if (BUZZER_USE_TONE)
    ledcAttach(BUZZER, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, 500, 2400);
  moveServoTo(servoClosedAngle);
  dht.begin();
  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);
  connectWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

// -----------------------------
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
    connectWiFi();
  if (!mqttClient.connected())
    reconnectMQTT();
  mqttClient.loop(); // process incoming messages

  unsigned long now = millis();
  if (buzzerOffAt != 0 && now >= buzzerOffAt)
  {
    buzzerOff();
    buzzerOffAt = 0;
  }

  if (now - lastSampleMs < reportIntervalMs)
    return;
  lastSampleMs = now;

  // Read sensors
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int lightRaw = readLdrAveraged();
  bool motion = digitalRead(PIR_PIN);
  float distance = readDistanceCm();

  bool tempHigh = !isnan(temperature) && temperature > tempMax;
  bool dark = lightRaw < lightMin;
  bool nearObj = !isnan(distance) && distance < distMin;

  setLedState(tempHigh, dark, !tempHigh && !dark);
  digitalWrite(RELAY, tempHigh);
  if (motion && !lastMotion)
    scheduleBuzzer(2000);
  lastMotion = motion;
  moveServoTo(nearObj ? servoOpenAngle : servoClosedAngle);

  // Publish JSON
  String tempJSON = "{\"temperature\": " + String(temperature) + "}";
  String humJSON = "{\"humidity\": " + String(humidity) + "}";
  String lightJSON = "{\"light\": " + String(lightRaw) + "}";
  String motionJSON = "{\"motion\": " + String(motion ? "true" : "false") + "}";

  mqttClient.publish(TOPIC_TEMP, tempJSON.c_str());
  mqttClient.publish(TOPIC_HUM, humJSON.c_str());
  mqttClient.publish(TOPIC_LIGHT, lightJSON.c_str());
  mqttClient.publish(TOPIC_MOTION, motionJSON.c_str());

  Serial.printf("T=%.2f H=%.2f L=%d M=%d D=%.2f\n",
                temperature, humidity, lightRaw, motion, distance);
}