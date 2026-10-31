#include <ESP8266WiFi.h>
#include <WiFiShare.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==== CẤU HÌNH WI-FI & FIREBASE ====
// Wi-Fi nhà do WiFiShare lo (firmware/libraries/WiFiShare). Mọi mạch dùng cùng SETUP_SSID và
// SETUP_PASSWORD. Không commit mật khẩu thật lên GitHub.
#define DEVICE_NAME "den_wc"
#define SETUP_SSID "SmartHouse-CaiDat"
#define SETUP_PASSWORD "DIEN_MAT_KHAU_CAI_DAT"
#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

WiFiShare wifiShare;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseStarted = false;

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

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  // false: thư viện Firebase không tự nối lại Wi-Fi, việc đó làm hỏng Wi-Fi cài đặt của WiFiShare.
  // Phải gọi trước Firebase.begin().
  Firebase.reconnectNetwork(false);
  Firebase.begin(&config, &auth);
  firebaseStarted = true;
}

// Chỉ dùng Firebase khi đã vào mạng nhà. Lần đầu vào được mạng thì khởi động Firebase.
bool firebaseReady() {
  if (!wifiShare.online()) return false;
  if (!firebaseStarted) initFirebase();
  return Firebase.ready();
}

// Mất mạng thì đèn vẫn bật/tắt theo PIR, chỉ không ghi lên Firebase
void updateFirebaseLightState(const String &path, bool state) {
  if (!firebaseReady()) return;
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

  wifiShare.begin(DEVICE_NAME, SETUP_SSID, SETUP_PASSWORD);
}

void loop() {
  wifiShare.loop();
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
