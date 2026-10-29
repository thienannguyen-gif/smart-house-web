#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==== LCD I2C ====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==== DHT11 ====
#define DHTPIN D6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==== PIR & Buzzer ====
const int pirPin = D5;
const int buzzerPin = D3;

bool lastPirState = LOW;
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000;

float lastTemp = -1000;
float lastHumi = -1000;

// ==== WiFi & Firebase ====
#define WIFI_SSID "Tesla_physics"
#define WIFI_PASSWORD "top102102"
#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool alarmEnabled = true;

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔌 Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Đã kết nối WiFi");
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  Serial.println("🔄 Kết nối Firebase...");
  while (!Firebase.ready()) delay(100);

  if (Firebase.ready()) {
    Serial.println("✅ Firebase đã sẵn sàng!");
    Firebase.RTDB.setBool(&fbdo, "/home/floor_1/alarm_triggered_1", false);
    Firebase.RTDB.setFloat(&fbdo, "/home/floor_1/temperature_1", 0);
    Firebase.RTDB.setFloat(&fbdo, "/home/floor_1/humidity_1", 0);
    Firebase.RTDB.setString(&fbdo, "/home/thongbao", "ESP8266 PIR DHT đã kết nối");
  }
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  dht.begin();

  pinMode(pirPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  lcd.setCursor(0, 0);
  lcd.print("Khoi dong...");
  delay(2000);
  lcd.clear();

  connectToWiFi();
  initFirebase();
}

void loop() {
  unsigned long currentMillis = millis();

  // === Đọc trạng thái bật/tắt còi từ Firebase ===
  if (Firebase.ready()) {
    Firebase.RTDB.getBool(&fbdo, "/home/floor_1/alarm_enabled_1");
    alarmEnabled = fbdo.boolData();
  }

  // === PIR ===
  bool pirDetected = digitalRead(pirPin) == HIGH;
  if (pirDetected && !lastPirState && alarmEnabled) {
    Serial.println("🚨 Phat hien chuyen dong!");
    digitalWrite(buzzerPin, HIGH);
    delay(3000);
    digitalWrite(buzzerPin, LOW);
    Firebase.RTDB.setBool(&fbdo, "/home/floor_1/alarm_triggered_1", true);
  } else if (!pirDetected) {
    Firebase.RTDB.setBool(&fbdo, "/home/floor_1/alarm_triggered_1", false);
  }
lastPirState = pirDetected;

  // === DHT11 ===
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      if (abs(t - lastTemp) >= 0.1 || abs(h - lastHumi) >= 0.1) {
        lcd.setCursor(0, 0);
        lcd.print("Nhiet do: ");
        lcd.print(t, 1);
        lcd.write(223);
        lcd.print("C  ");

        lcd.setCursor(0, 1);
        lcd.print("Do am  : ");
        lcd.print(h, 1);
        lcd.print("%   ");

        Firebase.RTDB.setFloat(&fbdo, "/home/floor_1/temperature_1", t);
        Firebase.RTDB.setFloat(&fbdo, "/home/floor_1/humidity_1", h);

        lastTemp = t;
        lastHumi = h;

        Serial.print("🌡 ");
        Serial.print(t);
        Serial.print(" *C | 💧 ");
        Serial.print(h);
        Serial.println(" %");
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print(" Loi cam bien DHT");
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
  }
}