#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <WiFiShare.h>
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

// Nhiệt độ, độ ẩm chỉ gửi lên Firebase khi đổi đủ nhiều, và cách nhau ít nhất 10 giây.
// LCD vẫn cập nhật mỗi 2 giây như cũ.
const unsigned long uploadInterval = 10000;
unsigned long lastUpload = 0;
float sentTemp = -1000;
float sentHumi = -1000;

// ==== WiFi & Firebase ====
// Wi-Fi nhà do WiFiShare lo (firmware/libraries/WiFiShare). Mọi mạch dùng cùng SETUP_SSID và
// SETUP_PASSWORD. Không commit mật khẩu thật lên GitHub.
#define DEVICE_NAME "bao_trom_do_am"
#define SETUP_SSID "SmartHouse-CaiDat"
#define SETUP_PASSWORD "DIEN_MAT_KHAU_CAI_DAT"
#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

WiFiShare wifiShare;
FirebaseData fbdo;    // ghi trạng thái, cảm biến
FirebaseData stream;  // nhận lệnh bật/tắt báo trộm từ web
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseStarted = false;
bool firebaseAnnounced = false;  // đã ghi các giá trị đầu và thông báo sau khi khởi động
bool streamStarted = false;
unsigned long lastStreamTry = 0;

bool alarmEnabled = true;
bool alarmTriggered = false;  // giá trị alarm_triggered_1 đã ghi lên Firebase

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
  Serial.println("🔄 Kết nối Firebase...");
}

// Chỉ dùng Firebase khi đã vào mạng nhà. Lần đầu vào được mạng thì khởi động Firebase, sẵn sàng
// lần đầu thì ghi các giá trị đầu. Mất mạng thì còi, cảm biến và LCD vẫn chạy.
bool firebaseReady() {
  if (!wifiShare.online()) return false;
  if (!firebaseStarted) initFirebase();
  if (!Firebase.ready()) return false;

  if (!firebaseAnnounced) {
    firebaseAnnounced = true;
    Serial.println("✅ Firebase đã sẵn sàng!");
    Firebase.RTDB.setBool(&fbdo, "/home/floor_1/alarm_triggered_1", alarmTriggered);
    Firebase.RTDB.setFloat(&fbdo, "/home/floor_1/temperature_1", 0);
    Firebase.RTDB.setFloat(&fbdo, "/home/floor_1/humidity_1", 0);
    Firebase.RTDB.setString(&fbdo, "/home/thongbao", "ESP8266 PIR DHT đã kết nối");
  }
  return true;
}

// === Đọc trạng thái bật/tắt còi từ Firebase ===
// Stream giữ một kết nối mở và chỉ nhận dữ liệu khi giá trị đổi. Bản cũ đọc alarm_enabled_1
// và ghi alarm_triggered_1 ở mỗi vòng loop(), làm cạn hạn mức của gói Firebase miễn phí.
void readAlarmStream() {
  if (!firebaseReady()) {
    if (!wifiShare.online()) streamStarted = false;  // vào lại mạng thì mở stream lại
    return;
  }

  if (!streamStarted) {
    if (lastStreamTry != 0 && millis() - lastStreamTry < 5000) return;
    lastStreamTry = millis();
    streamStarted = Firebase.RTDB.beginStream(&stream, "/home/floor_1/alarm_enabled_1");
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
    alarmEnabled = stream.boolData();
    Serial.println(alarmEnabled ? "🔔 Báo trộm: BẬT" : "🔕 Báo trộm: TẮT");
  }
}

// Chỉ ghi khi giá trị đổi. Ghi lỗi thì không thử lại ngay, để không gửi request ở mỗi vòng loop().
// Lúc mất mạng thì chưa ghi, có mạng lại mới ghi.
void setAlarmTriggered(bool triggered) {
  if (triggered == alarmTriggered || !firebaseReady()) return;
  alarmTriggered = triggered;
  if (!Firebase.RTDB.setBool(&fbdo, "/home/floor_1/alarm_triggered_1", triggered)) {
    Serial.print("❌ Lỗi ghi alarm_triggered_1: ");
    Serial.println(fbdo.errorReason());
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

  wifiShare.begin(DEVICE_NAME, SETUP_SSID, SETUP_PASSWORD);
}

void loop() {
  wifiShare.loop();
  unsigned long currentMillis = millis();

  readAlarmStream();

  // === PIR ===
  bool pirDetected = digitalRead(pirPin) == HIGH;
  if (pirDetected && !lastPirState && alarmEnabled) {
    Serial.println("🚨 Phat hien chuyen dong!");
    digitalWrite(buzzerPin, HIGH);
    delay(3000);
    digitalWrite(buzzerPin, LOW);
    setAlarmTriggered(true);
  } else if (!pirDetected) {
    setAlarmTriggered(false);
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

        lastTemp = t;
        lastHumi = h;

        Serial.print("🌡 ");
        Serial.print(t);
        Serial.print(" *C | 💧 ");
        Serial.print(h);
        Serial.println(" %");
      }

      bool changed = abs(t - sentTemp) >= 0.5 || abs(h - sentHumi) >= 1;
      if (changed && (lastUpload == 0 || currentMillis - lastUpload >= uploadInterval) && firebaseReady()) {
        lastUpload = currentMillis;
        FirebaseJson json;
        json.set("temperature_1", t);
        json.set("humidity_1", h);
        if (Firebase.RTDB.updateNode(&fbdo, "/home/floor_1", &json)) {
          sentTemp = t;
          sentHumi = h;
        }
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print(" Loi cam bien DHT");
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
  }
}
