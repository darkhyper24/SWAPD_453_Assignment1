#include <DHT.h>
#include <ESP32Servo.h>

// -----------------------------
// Pin Mapping
// -----------------------------
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
// Local test thresholds (offline)
// -----------------------------
float tempMax = 30.0f;
int lightMin = 500;
float distMin = 10.0f;

unsigned long reportIntervalMs = 2000;
int servoOpenAngle = 90;
int servoClosedAngle = 0;

DHT dht(DHTPIN, DHTTYPE);
Servo servoMotor;

// Buzzer mode:
// true  -> passive buzzer/piezo (Wokwi typical) using generated tone
// false -> active buzzer module using digital ON/OFF
const bool BUZZER_USE_TONE = true;
const bool BUZZER_ACTIVE_LOW = false;  // Used only when BUZZER_USE_TONE == false
const int BUZZER_FREQUENCY = 2000;
const int BUZZER_RESOLUTION = 8;

unsigned long lastSampleMs = 0;
unsigned long buzzerOffAt = 0;
bool lastMotion = false;
bool buzzerIsOn = false;

int readLdrAveraged() {
  long sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(LDR_PIN);
    delay(2);
  }
  return (int)(sum / samples);
}

void setLedState(bool redOn, bool yellowOn, bool greenOn) {
  digitalWrite(RED_LED, redOn ? HIGH : LOW);
  digitalWrite(YELLOW_LED, yellowOn ? HIGH : LOW);
  digitalWrite(GREEN_LED, greenOn ? HIGH : LOW);
}

void buzzerOn() {
  if (BUZZER_USE_TONE) {
    ledcWriteTone(BUZZER, BUZZER_FREQUENCY);
  } else {
    digitalWrite(BUZZER, BUZZER_ACTIVE_LOW ? LOW : HIGH);
  }
  if (!buzzerIsOn) {
    buzzerIsOn = true;
    Serial.println("[BUZZER] ON");
  }
}

void buzzerOff() {
  if (BUZZER_USE_TONE) {
    ledcWriteTone(BUZZER, 0);
  } else {
    digitalWrite(BUZZER, BUZZER_ACTIVE_LOW ? HIGH : LOW);
  }
  if (buzzerIsOn) {
    buzzerIsOn = false;
    Serial.println("[BUZZER] OFF");
  }
}

void scheduleBuzzer(unsigned long durationMs) {
  buzzerOn();
  buzzerOffAt = millis() + durationMs;
}

void moveServoTo(int angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  servoMotor.write(angle);
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration <= 0) return NAN;
  return (duration * 0.0343f) / 2.0f;
}

void setup() {
  Serial.begin(115200);
  delay(300);

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
  digitalWrite(RELAY, LOW);

  if (BUZZER_USE_TONE) {
    ledcAttach(BUZZER, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  }
  buzzerOff();

  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, 500, 2400);
  moveServoTo(servoClosedAngle);

  dht.begin();

  // Keep full ESP32 ADC range for LDR divider readings.
  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  Serial.printf("Offline circuit test mode started (no WiFi/MQTT), buzzer mode: %s\n",
                BUZZER_USE_TONE ? "TONE" : "DIGITAL");
}

void loop() {
  unsigned long now = millis();

  if (buzzerOffAt != 0 && now >= buzzerOffAt) {
    buzzerOff();
    buzzerOffAt = 0;
  }

  if (now - lastSampleMs < reportIntervalMs) {
    return;
  }
  lastSampleMs = now;

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int lightRaw = readLdrAveraged();
  bool motion = digitalRead(PIR_PIN) == HIGH;
  float distance = readDistanceCm();

  bool tempHigh = !isnan(temperature) && temperature > tempMax;
  bool dark = lightRaw < lightMin;
  bool nearObj = !isnan(distance) && distance < distMin;

  bool red = tempHigh;
  bool yellow = dark;
  bool green = !tempHigh && !dark;
  setLedState(red, yellow, green);

  digitalWrite(RELAY, tempHigh ? HIGH : LOW);

  // Motion alarm for 2s on rising edge.
  if (motion && !lastMotion) {
    scheduleBuzzer(2000);
  }
  lastMotion = motion;

  moveServoTo(nearObj ? servoOpenAngle : servoClosedAngle);

  Serial.printf("T=%.2fC H=%.2f%% L=%d M=%s D=%.2fcm | RED=%d YEL=%d GRN=%d RELAY=%d SERVO=%d BUZZ=%s\n",
                temperature,
                humidity,
                lightRaw,
                motion ? "YES" : "NO",
                distance,
                red,
                yellow,
                green,
                tempHigh ? 1 : 0,
                nearObj ? servoOpenAngle : servoClosedAngle,
                buzzerIsOn ? "ON" : "OFF");

  if (lightRaw >= 4090 || lightRaw <= 5) {
    Serial.println("[LDR] ADC near rail; check divider wiring (must be voltage divider into GPIO34).");
  }
}
