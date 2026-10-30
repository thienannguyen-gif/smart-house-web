#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==== CẤU HÌNH WI-FI & FIREBASE ====
#define WIFI_SSID "Tesla_physics"
#define WIFI_PASSWORD "DIEN_MAT_KHAU_WIFI"
#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==== KHAI BÁO CHÂN KẾT NỐI ====
const int pirPin = D4;     // PIR 1 (WC1)
const int ledPin = D0;     // Đèn WC1

const int pirPin2 = D5;    // PIR 2 (WC2)
const int ledPin2 = D7;    // Đèn WC2

// ==== THỜI GIAN SÁNG ====
const unsigned long lightDuration = 3000;  // 3 giây

// ==== TRẠNG THÁI & THỜI GIAN ====
unsigned long lastMotionTime1 = 0;
bool led1On = false;
int lastPirState1 = LOW;

unsigned long lastMotionTime2 = 0;
bool led2On = false;
int lastPirState2 = LOW;

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nĐã kết nối Wi-Fi");
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void updateFirebaseLightState(const String &path, bool state) {
  Firebase.RTDB.setBool(&fbdo, path, state);
}

void setup() {
  Serial.begin(9600);

  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(pirPin2, INPUT);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin2, LOW);

  connectToWiFi();
  initFirebase();
}

void loop() {
  unsigned long currentMillis = millis();

  // ==== PIR 1 ====
  int currentPirState1 = digitalRead(pirPin);
  if (currentPirState1 == HIGH && lastPirState1 == LOW) {
    Serial.println("PIR 1: Phát hiện chuyển động!");
    digitalWrite(ledPin, HIGH);
    lastMotionTime1 = currentMillis;
    led1On = true;
    updateFirebaseLightState("/home/floor_1/wc1_light", true);
  }
  lastPirState1 = currentPirState1;

  if (led1On && (currentMillis - lastMotionTime1 >= lightDuration)) {
    digitalWrite(ledPin, LOW);
    led1On = false;
    updateFirebaseLightState("/home/floor_1/wc1_light", false);
  }

  // ==== PIR 2 ====
  int currentPirState2 = digitalRead(pirPin2);
  if (currentPirState2 == HIGH && lastPirState2 == LOW) {
    Serial.println("PIR 2: Phát hiện chuyển động!");
    digitalWrite(ledPin2, HIGH);
    lastMotionTime2 = currentMillis;
    led2On = true;
    updateFirebaseLightState("/home/floor_1/wc2_light", true);
  }
  lastPirState2 = currentPirState2;

  if (led2On && (currentMillis - lastMotionTime2 >= lightDuration)) {
    digitalWrite(ledPin2, LOW);
    led2On = false;
    updateFirebaseLightState("/home/floor_1/wc2_light", false);
  }
}
