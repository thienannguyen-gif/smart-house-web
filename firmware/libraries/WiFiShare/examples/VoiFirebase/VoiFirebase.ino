// Dùng WiFiShare với thư viện Firebase của Mobizt (Firebase Arduino Client Library for ESP8266 and ESP32).
//
// Điểm cần nhớ khi ghép vào firmware có sẵn:
//  - Bỏ hàm tự vào Wi-Fi (WiFi.begin, vòng while chờ WL_CONNECTED). WiFiShare lo việc này.
//  - Firebase.reconnectNetwork(false): để thư viện Firebase không tự nối lại Wi-Fi, vì lúc mạch đang
//    làm mạch chủ hoặc mạch con, việc nối lại đó làm hỏng Wi-Fi cài đặt.
//  - Chỉ gọi Firebase khi wifiShare.online(). Phần chạy không cần mạng (nút, cảm biến, còi) để ngoài.
#include <WiFiShare.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

#define DEVICE_NAME "den_phong_khach"
#define SETUP_SSID "NhaToi-CaiDat"
#define SETUP_PASSWORD "doi-mat-khau-nay"

#define API_KEY "DIEN_API_KEY"
#define DATABASE_URL "https://ten-project-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"
#define LIGHT_PATH "/home/light_room_1"

const int LIGHT_PIN = 5;

WiFiShare wifiShare;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;
bool streamStarted = false;
unsigned long lastStreamTry = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_PIN, OUTPUT);
  wifiShare.begin(DEVICE_NAME, SETUP_SSID, SETUP_PASSWORD);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(false);
  Firebase.begin(&config, &auth);
  stream.setBSSLBufferSize(2048, 512);
}

void loop() {
  wifiShare.loop();

  // Phần không cần mạng đặt ở đây

  if (!wifiShare.online()) {
    streamStarted = false;  // có mạng lại thì mở stream lại
    return;
  }
  if (!Firebase.ready()) return;

  if (!streamStarted) {
    if (lastStreamTry && millis() - lastStreamTry < 5000) return;
    lastStreamTry = millis();
    streamStarted = Firebase.RTDB.beginStream(&stream, LIGHT_PATH);
    return;
  }
  if (!Firebase.RTDB.readStream(&stream)) Serial.println(stream.errorReason());
  if (stream.streamAvailable() && stream.dataType() == "boolean") {
    digitalWrite(LIGHT_PIN, stream.boolData() ? HIGH : LOW);
  }
}
