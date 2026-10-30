#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <Servo.h>

// --- WiFi & Firebase config ---
#define WIFI_SSID       "Tesla_physics"
#define WIFI_PASSWORD   "DIEN_MAT_KHAU_WIFI"

#define API_KEY         "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL    "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL      "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD   "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

// --- GPIO ---
#define LIGHT_RELAY_PIN D6    // Relay điều khiển đèn 6V
#define BUZZER_PIN D5         // Còi báo
#define SERVO_PIN D4          // Servo mở cửa

// --- Fingerprint setup ---
SoftwareSerial fingerSerial(D7, D8);  // RX=D8, TX=D7
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// --- Servo ---
Servo doorServo;
const int openPos = 180;
const int closePos = 0;

// --- Firebase objects ---
FirebaseData fbdo;    // ghi trạng thái
FirebaseData stream;  // nhận lệnh đèn từ web
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseStarted = false;
bool streamStarted = false;
unsigned long lastStreamTry = 0;

// --- Biến trạng thái vân tay ---
int lastFingerprintID = -1;

// --- Hàm khởi động WiFi ---
bool initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    Serial.print(".");
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Đã kết nối WiFi");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Kết nối WiFi thất bại!");
    return false;
  }
}

// --- Hàm khởi động Firebase ---
void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  stream.setBSSLBufferSize(2048, 512);
  firebaseStarted = true;

  Serial.println("🔄 Đang kết nối Firebase...");
  unsigned long timeout = millis();
  while (!Firebase.ready() && millis() - timeout < 15000) {
    delay(100);
  }

  if (Firebase.ready()) {
    Serial.println("✅ Firebase kết nối thành công!");

    // Khởi tạo node đèn nếu chưa tồn tại
    if (!Firebase.RTDB.getBool(&fbdo, "home/light_room_3")) {
      Serial.println("⚠️ Node 'home/light_room_3' chưa tồn tại. Tạo mới...");
      if (Firebase.RTDB.setBool(&fbdo, "home/light_room_3", false)) {
        Serial.println("✅ Node 'light_room_3' đã được tạo (false).");
      } else {
        Serial.print("❌ Lỗi tạo node: ");
        Serial.println(fbdo.errorReason());
      }
    }

    // Khởi tạo node trạng thái cửa
    if (!Firebase.RTDB.getBool(&fbdo, "home/door_status")) {
      Serial.println("⚠️ Node 'home/door_status' chưa tồn tại. Tạo mới...");
      if (Firebase.RTDB.setBool(&fbdo, "home/door_status", false)) {
        Serial.println("✅ Node 'door_status' đã được tạo (false).");
      } else {
        Serial.print("❌ Lỗi tạo node door_status: ");
        Serial.println(fbdo.errorReason());
      }
    }

    Firebase.RTDB.setString(&fbdo, "home/thongbao", "ESP8266 tầng 3 đã kết nối");
  } else {
    Serial.println("❌ Firebase connection failed!");
  }
}

// --- Nhận lệnh đèn từ Firebase ---
// Stream giữ một kết nối mở và chỉ nhận dữ liệu khi giá trị đổi. Bản cũ đọc đèn mỗi 0,2 giây,
// làm cạn hạn mức tải xuống của gói Firebase miễn phí.
void readLightStream() {
  if (!Firebase.ready()) return;

  if (!streamStarted) {
    if (lastStreamTry != 0 && millis() - lastStreamTry < 5000) return;
    lastStreamTry = millis();
    streamStarted = Firebase.RTDB.beginStream(&stream, "/home/light_room_3");
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
  if (stream.streamAvailable() && stream.dataType() == "boolean") {
    bool lightState = stream.boolData();
    digitalWrite(LIGHT_RELAY_PIN, lightState ? HIGH : LOW);
    Serial.print("💡 Trạng thái relay đèn: ");
    Serial.println(lightState ? "BẬT" : "TẮT");
  }
}

// --- Hàm đọc ID vân tay ---
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  return finger.fingerID;
}

// --- Hàm cập nhật trạng thái đèn trên Firebase ---
void updateLightStatus(bool on) {
  if (Firebase.RTDB.setBool(&fbdo, "home/light_room_3", on)) {
    Serial.print("✅ Cập nhật trạng thái đèn: ");
    Serial.println(on ? "BẬT" : "TẮT");
  } else {
    Serial.print("❌ Lỗi cập nhật trạng thái đèn: ");
    Serial.println(fbdo.errorReason());
  }
}

// --- Hàm cập nhật trạng thái cửa trên Firebase ---
void updateDoorStatus(bool open) {
  if (Firebase.RTDB.setBool(&fbdo, "home/door_status", open)) {
    Serial.print("✅ Cập nhật trạng thái cửa: ");
    Serial.println(open ? "MỞ" : "ĐÓNG");
  } else {
    Serial.print("❌ Lỗi cập nhật trạng thái cửa: ");
    Serial.println(fbdo.errorReason());
  }
}

// --- Mở cửa servo ---
void openDoor() {
  doorServo.write(openPos);
  Serial.println("🔓 Cửa đang mở...");
  updateDoorStatus(true);

  delay(2000);  // giữ cửa mở 2 giây

  doorServo.write(closePos);
  Serial.println("🔒 Cửa đã đóng.");
  updateDoorStatus(false);
}

void setup() {
  Serial.begin(115200);

  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  digitalWrite(LIGHT_RELAY_PIN, LOW);  // Đèn tắt lúc đầu

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  doorServo.attach(SERVO_PIN);
  doorServo.write(closePos);

  fingerSerial.begin(57600);
  finger.begin(57600);
  delay(100);
  Serial.println("Đang kết nối cảm biến vân tay...");

  if (finger.verifyPassword()) {
    Serial.println("✅ Cảm biến vân tay sẵn sàng.");
  } else {
    Serial.println("❌ Không tìm thấy cảm biến vân tay!");
    while (1) delay(1);
  }

  finger.getTemplateCount();
  Serial.print("Số vân tay đã lưu: ");
  Serial.println(finger.templateCount);

  if (initWiFi()) {
    initFirebase();
  }
}

void loop() {
  // Lấy trạng thái đèn từ Firebase để điều khiển relay. Mất mạng thì thư viện Firebase tự nối
  // lại WiFi (reconnectNetwork), trong lúc đó cửa vân tay vẫn dùng được.
  if (firebaseStarted) {
    readLightStream();
  } else if (WiFi.status() == WL_CONNECTED) {
    initFirebase();  // lúc khởi động chưa có WiFi
  }

  // Quét vân tay
  int id = getFingerprintID();

  if (id >= 0 && id != lastFingerprintID) {
    lastFingerprintID = id;
    Serial.print("✅ Vân tay hợp lệ! ID: ");
    Serial.println(id);

    // Phát còi báo thành công
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);

    // Bật đèn và cập nhật trạng thái đèn trên Firebase
    digitalWrite(LIGHT_RELAY_PIN, HIGH);
    updateLightStatus(true);

    // Mở cửa servo và cập nhật trạng thái cửa trên Firebase
    openDoor();
  } else if (id == -1) {
    lastFingerprintID = -1;
  }

  delay(200);
}
