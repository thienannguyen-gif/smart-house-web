#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==== Thông tin Wi-Fi & Firebase ====
#define WIFI_SSID "Tesla_physics"
#define WIFI_PASSWORD "top102102"

#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==== GPIO cửa cuốn ====
#define IN1 D1
#define IN2 D2
#define ENA D3

#define BUTTON_UP D5
#define BUTTON_DOWN D6

unsigned long startTime = 0;
bool isRunning = false;
int currentDirection = 0; // 0: stop, 1: up, 2: down

String lastCommand = "";

void setup() {
  Serial.begin(9600);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  stopMotor();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nĐã kết nối WiFi");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void loop() {
  // ==== Đọc lệnh từ Firebase ====
  if (Firebase.ready() && !isRunning) {
    if (Firebase.RTDB.getString(&fbdo, "/home/floor_1/garage/command")) {
      String cmd = fbdo.stringData();
      if (cmd != lastCommand) {
        if (cmd == "open") {
          rollUp();
          startTime = millis();
          isRunning = true;
          currentDirection = 1;
          lastCommand = cmd;
        } else if (cmd == "close") {
          rollDown();
          startTime = millis();
          isRunning = true;
          currentDirection = 2;
          lastCommand = cmd;
        }
      }
    }
  }

  // ==== Kiểm tra nút nhấn vật lý ====
  if (!isRunning) {
    if (digitalRead(BUTTON_UP) == LOW) {
      rollUp();
      startTime = millis();
      isRunning = true;
      currentDirection = 1;
      lastCommand = "open";
    } else if (digitalRead(BUTTON_DOWN) == LOW) {
      rollDown();
      startTime = millis();
      isRunning = true;
      currentDirection = 2;
      lastCommand = "close";
    }
  }

  // ==== Dừng sau 300ms ====
  if (isRunning && millis() - startTime >= 300) {
    stopMotor();
    isRunning = false;
    currentDirection = 0;

    // Cập nhật trạng thái cửa lên Firebase
    if (lastCommand == "open") {
      Firebase.RTDB.setString(&fbdo, "/home/floor_1/garage/status", "open");
    } else if (lastCommand == "close") {
      Firebase.RTDB.setString(&fbdo, "/home/floor_1/garage/status", "closed");
    }
  }
}

void rollUp() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 90);  // Giữ nguyên theo yêu cầu
}

void rollDown() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, 85);  // Giữ nguyên theo yêu cầu
}

void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}
