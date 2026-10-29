#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// WiFi credentials
#define WIFI_SSID       "Tesla_physics"
#define WIFI_PASSWORD   "top102102"

// Firebase config
#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

// ==== Chân kết nối phần cứng ====
#define SMOKE_SENSOR_PIN A0
#define BUZZER_PIN D1
#define SWITCH_PIN D5
#define LIGHT_RELAY_PIN D6

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Biến trạng thái
bool lastLightState = false;
bool lastMuteState = false;

// Biến cho nút nhấn
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

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

  Serial.println("🔄 Đang kết nối Firebase...");
  unsigned long timeout = millis();
  while (!Firebase.ready() && millis() - timeout < 15000) {
    delay(100);
  }

  if (Firebase.ready()) {
    Serial.println("✅ Firebase kết nối thành công!");

    // Khởi tạo các nhánh tầng 2 nếu chưa có
    if (!Firebase.RTDB.getBool(&fbdo, "/home/floor_2/light_room_2")) {
      Firebase.RTDB.setBool(&fbdo, "/home/floor_2/light_room_2", false);
    }
    if (!Firebase.RTDB.getBool(&fbdo, "/home/floor_2/smoke_detected_2")) {
      Firebase.RTDB.setBool(&fbdo, "/home/floor_2/smoke_detected_2", false);
    }
    if (!Firebase.RTDB.getInt(&fbdo, "/home/floor_2/smoke_level_2")) {
      Firebase.RTDB.setInt(&fbdo, "/home/floor_2/smoke_level_2", 0);
    }
    if (!Firebase.RTDB.getBool(&fbdo, "/home/floor_2/buzzer_mute_2")) {
      Firebase.RTDB.setBool(&fbdo, "/home/floor_2/buzzer_mute_2", false);
    }

    // Cập nhật thông báo tầng 2
    if (Firebase.RTDB.setString(&fbdo, "/home/thongbao", "ESP8266 tầng 2 đã kết nối")) {
      Serial.println("📢 Đã cập nhật thông báo tầng 2.");
    } else {
      Serial.print("❌ Lỗi cập nhật thông báo: ");
      Serial.println(fbdo.errorReason());
    }

  } else {
    Serial.println("❌ Kết nối Firebase thất bại!");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP); // Nút nhấn nối GND ↔ D5

  digitalWrite(LIGHT_RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  connectToWiFi();
  initFirebase();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    Serial.println("🔄 Mất kết nối, thử kết nối lại...");
    connectToWiFi();
    initFirebase();
    return;
  }

  // ====== ĐIỀU KHIỂN ĐÈN TỪ WEB ======
  if (Firebase.RTDB.getBool(&fbdo, "/home/floor_2/light_room_2")) {
    bool lightState = fbdo.boolData();
    if (lightState != lastLightState) {
      digitalWrite(LIGHT_RELAY_PIN, lightState ? HIGH : LOW);
      Serial.print("💡 Đèn tầng 2 (web): ");
      Serial.println(lightState ? "BẬT" : "TẮT");
      lastLightState = lightState;
    }
  }

  // ====== ĐIỀU KHIỂN ĐÈN BẰNG NÚT NHẤN ======
  int reading = digitalRead(SWITCH_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {  // Nhấn nút (nối GND)
        lastLightState = !lastLightState;
        digitalWrite(LIGHT_RELAY_PIN, lastLightState ? HIGH : LOW);
        Serial.print("🔘 Nút nhấn: đèn ");
        Serial.println(lastLightState ? "BẬT" : "TẮT");

        // Cập nhật trạng thái đèn về Firebase
        Firebase.RTDB.setBool(&fbdo, "/home/floor_2/light_room_2", lastLightState);
      }
    }
  }
  lastButtonState = reading;

  // ====== CẢM BIẾN KHÓI ======
  int smokeValue = analogRead(SMOKE_SENSOR_PIN);
  Serial.print("🌫️ Giá trị khói: ");
  Serial.println(smokeValue);
  Firebase.RTDB.setInt(&fbdo, "/home/floor_2/smoke_level_2", smokeValue);

  bool smokeDetected = smokeValue > 120;
  Firebase.RTDB.setBool(&fbdo, "/home/floor_2/smoke_detected_2", smokeDetected);

  // ====== CÒI BÁO KHÓI ======
  bool mute = false;
  if (Firebase.RTDB.getBool(&fbdo, "/home/floor_2/buzzer_mute_2")) {
    mute = fbdo.boolData();
  }

  if (smokeDetected && !mute) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(500);
}
