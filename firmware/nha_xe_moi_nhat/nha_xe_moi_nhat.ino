#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==== Thông tin Wi-Fi & Firebase ====
#define WIFI_SSID "Tesla_physics"
#define WIFI_PASSWORD "DIEN_MAT_KHAU_WIFI"

#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH"

#define COMMAND_PATH "/home/floor_1/garage/command"

FirebaseData fbdo;    // ghi trạng thái cửa
FirebaseData stream;  // nhận lệnh từ web
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
String pendingCommand = "";  // lệnh từ web đến lúc motor đang chạy, xử lý khi motor dừng
bool fromButton = false;     // lần chạy hiện tại do nút nhấn tại chỗ

bool streamStarted = false;
unsigned long lastStreamTry = 0;

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
  stream.setBSSLBufferSize(2048, 512);
}

// ==== Nhận lệnh từ Firebase ====
// Stream giữ một kết nối mở và chỉ nhận dữ liệu khi lệnh đổi. Bản cũ đọc lệnh ở mỗi vòng
// loop(), hàng chục request mỗi giây, làm cạn hạn mức tải xuống của gói Firebase miễn phí.
void readCommandStream() {
  if (!Firebase.ready()) return;

  if (!streamStarted) {
    if (lastStreamTry != 0 && millis() - lastStreamTry < 5000) return;
    lastStreamTry = millis();
    streamStarted = Firebase.RTDB.beginStream(&stream, COMMAND_PATH);
    if (!streamStarted) {
      Serial.print("Lỗi mở stream: ");
      Serial.println(stream.errorReason());
    }
    return;
  }

  if (!Firebase.RTDB.readStream(&stream)) {
    Serial.print("Lỗi đọc stream: ");
    Serial.println(stream.errorReason());
  }
  if (stream.streamAvailable() && stream.dataType() == "string") {
    pendingCommand = stream.stringData();
  }
}

void startRun(int direction, const String &cmd, bool button) {
  if (direction == 1) rollUp();
  else rollDown();
  startTime = millis();
  isRunning = true;
  currentDirection = direction;
  lastCommand = cmd;
  fromButton = button;
}

void loop() {
  readCommandStream();

  // ==== Lệnh từ web ====
  if (!isRunning && pendingCommand.length() > 0) {
    String cmd = pendingCommand;
    pendingCommand = "";
    if (cmd != lastCommand) {
      if (cmd == "open") startRun(1, cmd, false);
      else if (cmd == "close") startRun(2, cmd, false);
    }
  }

  // ==== Kiểm tra nút nhấn vật lý ====
  if (!isRunning) {
    if (digitalRead(BUTTON_UP) == LOW) {
      startRun(1, "open", true);
    } else if (digitalRead(BUTTON_DOWN) == LOW) {
      startRun(2, "close", true);
    }
  }

  // ==== Dừng sau 300ms ====
  if (isRunning && millis() - startTime >= 300) {
    stopMotor();
    isRunning = false;
    currentDirection = 0;

    // Cập nhật trạng thái cửa lên Firebase sau khi motor đã dừng, trong một request.
    // Bấm nút tại chỗ thì ghi cả lệnh, để web hiện đúng và lệnh kế tiếp từ web vẫn là một thay đổi.
    FirebaseJson json;
    json.set("status", lastCommand == "open" ? "open" : "closed");
    if (fromButton) json.set("command", lastCommand);
    if (!Firebase.RTDB.updateNode(&fbdo, "/home/floor_1/garage", &json)) {
      Serial.print("Lỗi ghi trạng thái cửa: ");
      Serial.println(fbdo.errorReason());
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
