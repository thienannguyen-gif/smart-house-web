#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiShare.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// Wi-Fi nhà do WiFiShare lo (firmware/libraries/WiFiShare). Mọi mạch dùng cùng SETUP_SSID và
// SETUP_PASSWORD. Không commit mật khẩu thật lên GitHub.
#define DEVICE_NAME     "bao_chay_tang_2"
#define SETUP_SSID      "SmartHouse-CaiDat"
#define SETUP_PASSWORD  "DIEN_MAT_KHAU_CAI_DAT"

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

// Wi-Fi, Firebase
WiFiShare wifiShare;
FirebaseData fbdo;    // ghi trạng thái, cảm biến
FirebaseData stream;  // nhận lệnh đèn và tắt còi từ web (cả nhánh /home/floor_2)
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseStarted = false;
bool firebaseAnnounced = false;  // đã tạo các nhánh tầng 2 và ghi thông báo sau khi khởi động
bool streamStarted = false;
unsigned long lastStreamTry = 0;

// Biến trạng thái
bool lastLightState = false;
bool lastMuteState = false;
bool smokeDetected = false;

// Cảm biến khói: đọc mỗi 0,5 giây như cũ. Chỉ gửi lên Firebase khi báo khói đổi (gửi ngay),
// hoặc khi giá trị đổi từ 10 trở lên và đã qua 5 giây kể từ lần gửi trước.
const int smokeThreshold = 120;
const unsigned long smokeReadInterval = 500;
const unsigned long smokeUploadInterval = 5000;
unsigned long lastSmokeRead = 0;
unsigned long lastSmokeUpload = 0;
int sentSmokeLevel = -1000;
bool sentSmokeDetected = false;

// Biến cho nút nhấn
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

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
  stream.setBSSLBufferSize(2048, 512);
  firebaseStarted = true;
  Serial.println("🔄 Đang kết nối Firebase...");
}

// Chỉ dùng Firebase khi đã vào mạng nhà. Lần đầu vào được mạng thì khởi động Firebase, sẵn sàng
// lần đầu thì tạo các nhánh tầng 2.
bool firebaseReady() {
  if (!wifiShare.online()) return false;
  if (!firebaseStarted) initFirebase();
  if (!Firebase.ready()) return false;

  if (!firebaseAnnounced) {
    firebaseAnnounced = true;
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
  }
  return true;
}

void applyLight(bool lightState, const char *source) {
  if (lightState == lastLightState) return;
  digitalWrite(LIGHT_RELAY_PIN, lightState ? HIGH : LOW);
  Serial.print("💡 Đèn tầng 2 (");
  Serial.print(source);
  Serial.print("): ");
  Serial.println(lightState ? "BẬT" : "TẮT");
  lastLightState = lightState;
}

// ====== NHẬN LỆNH TỪ WEB ======
// Stream giữ một kết nối mở và chỉ nhận dữ liệu khi có thay đổi. Bản cũ đọc 2 giá trị và ghi
// 2 giá trị mỗi 0,5 giây, làm cạn hạn mức tải xuống của gói Firebase miễn phí.
void readFloor2Stream() {
  if (!firebaseReady()) {
    if (!wifiShare.online()) streamStarted = false;  // vào lại mạng thì mở stream lại
    return;
  }

  if (!streamStarted) {
    if (lastStreamTry != 0 && millis() - lastStreamTry < 5000) return;
    lastStreamTry = millis();
    streamStarted = Firebase.RTDB.beginStream(&stream, "/home/floor_2");
    if (!streamStarted) {
      Serial.print("❌ Lỗi mở stream: ");
      Serial.println(stream.errorReason());
    }
    return;
  }

  if (!Firebase.RTDB.readStream(&stream)) {
    Serial.print("❌ Lỗi đọc stream: ");
    Serial.println(stream.errorReason());
  }
  if (!stream.streamAvailable()) return;

  String path = stream.dataPath();
  if (stream.dataType() == "json" && path == "/") {
    // Lần đầu (hoặc khi ghi nhiều giá trị một lúc) Firebase gửi cả nhánh floor_2
    FirebaseJson &json = stream.jsonObject();
    FirebaseJsonData result;
    if (json.get(result, "light_room_2") && result.type == "boolean") applyLight(result.boolValue, "web");
    if (json.get(result, "buzzer_mute_2") && result.type == "boolean") lastMuteState = result.boolValue;
  } else if (stream.dataType() == "boolean") {
    if (path == "/light_room_2") applyLight(stream.boolData(), "web");
    else if (path == "/buzzer_mute_2") lastMuteState = stream.boolData();
  }
}

void uploadSmoke(int smokeValue) {
  unsigned long since = millis() - lastSmokeUpload;
  bool detectedChanged = smokeDetected != sentSmokeDetected;
  bool levelChanged = abs(smokeValue - sentSmokeLevel) >= 10;
  // Báo khói đổi: gửi ngay, nếu lỗi thì thử lại sau 1 giây. Giá trị khói: cách nhau ít nhất 5 giây.
  bool due = detectedChanged ? since >= 1000 : (levelChanged && since >= smokeUploadInterval);
  if (!due || !firebaseReady()) return;

  lastSmokeUpload = millis();
  FirebaseJson json;
  json.set("smoke_level_2", smokeValue);
  json.set("smoke_detected_2", smokeDetected);
  if (Firebase.RTDB.updateNode(&fbdo, "/home/floor_2", &json)) {
    sentSmokeLevel = smokeValue;
    sentSmokeDetected = smokeDetected;
  } else {
    Serial.print("❌ Lỗi ghi cảm biến khói: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP); // Nút nhấn nối GND ↔ D5

  digitalWrite(LIGHT_RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  wifiShare.begin(DEVICE_NAME, SETUP_SSID, SETUP_PASSWORD);
}

// Nút nhấn, cảm biến khói và còi chạy cả khi mất mạng. WiFiShare tự vào lại mạng, stream tự mở
// lại khi có mạng.
void loop() {
  wifiShare.loop();
  readFloor2Stream();

  // ====== ĐIỀU KHIỂN ĐÈN BẰNG NÚT NHẤN ======
  int reading = digitalRead(SWITCH_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {  // Nhấn nút (nối GND)
        applyLight(!lastLightState, "nút nhấn");

        // Cập nhật trạng thái đèn về Firebase
        if (firebaseReady()) Firebase.RTDB.setBool(&fbdo, "/home/floor_2/light_room_2", lastLightState);
      }
    }
  }
  lastButtonState = reading;

  // ====== CẢM BIẾN KHÓI ======
  if (millis() - lastSmokeRead >= smokeReadInterval) {
    lastSmokeRead = millis();
    int smokeValue = analogRead(SMOKE_SENSOR_PIN);
    Serial.print("🌫️ Giá trị khói: ");
    Serial.println(smokeValue);

    smokeDetected = smokeValue > smokeThreshold;
    uploadSmoke(smokeValue);
  }

  // ====== CÒI BÁO KHÓI ======
  if (smokeDetected && !lastMuteState) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}
