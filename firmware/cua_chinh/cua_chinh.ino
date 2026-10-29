#include <Wire.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ==== Cấu hình phần cứng ====
#define KEYPAD_ADDR 0x20
#define LCD_ADDR 0x27
#define SERVO_PIN D5
#define BUZZER_PIN D6
#define LED_PIN D7  // Relay nối chân D7

// ==== Cấu hình WiFi + Firebase (project Smart House: smart-house-fcd4f) ====
#define WIFI_SSID "Tesla_physics"
#define WIFI_PASSWORD "top102102"
#define API_KEY "AIzaSyBom2FOJvMisw2NcE89Zkr-rP_dgOSqbMY"
#define DATABASE_URL "https://smart-house-fcd4f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "DIEN_EMAIL_TAI_KHOAN_MACH"
#define USER_PASSWORD "DIEN_MAT_KHAU_TAI_KHOAN_MACH" 

// ==== LCD & Servo ====
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
Servo doorServo;

// ==== Firebase ====
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==== Keypad cấu hình ====
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};
Keypad_I2C kpad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR, 1, &Wire);

// ==== Biến điều khiển ====
String password = "111#";
String input = "";
bool changingPass = false;
bool confirmedOld = false;
String newPass = "";
bool lastLedState = LOW;
char lastKey = 0;

// ==== Buzzer beep ====
void beep(int time_ms = 100) {
  tone(BUZZER_PIN, 1000);
  delay(time_ms);
  noTone(BUZZER_PIN);
}

// ==== WiFi ====
void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

// ==== Firebase ====
void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

// ==== Cập nhật trạng thái cửa ====
void updateFirebaseDoorState(bool state) {
  Firebase.RTDB.setBool(&fbdo, "/home/floor_1/door_opened_1", state);
}

// ==== Lấy lệnh từ Firebase để điều khiển đèn ====
void checkFirebaseLightCommand() {
  if (Firebase.RTDB.getBool(&fbdo, "/home/floor_1/light_room_1")) {
    bool firebaseValue = fbdo.boolData();
    bool ledControl = !firebaseValue;  // đảo ngược logic

    if (ledControl != lastLedState) {
      digitalWrite(LED_PIN, ledControl ? LOW : HIGH); // LOW = bật đèn (relay đóng)
      lastLedState = ledControl;
    }
  }
}

// ==== Setup ====
void setup() {
  Serial.begin(9600);
  Wire.begin(D2, D1);
  kpad.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Nhap mat khau:");

  doorServo.attach(SERVO_PIN);
  doorServo.write(100);  // Góc đóng cửa

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // HIGH = đèn tắt (relay mở)

  connectToWiFi();
  initFirebase();
}

// ==== Loop ====
void loop() {
  checkFirebaseLightCommand();

  char key = kpad.getKey();
  if (key && key != lastKey) {
    lastKey = key;
    beep();
    Serial.println(key);

    if (key == 'A') {
      if (!changingPass) {
        if (input == password) {
          lcd.clear();
          lcd.print("Mo cua...");
          doorServo.write(20);  // Mở cửa
          digitalWrite(LED_PIN, HIGH); // Bật đèn
          Firebase.RTDB.setBool(&fbdo, "/home/floor_1/light_room_1", false);  // Ghi false → bật đèn
          updateFirebaseDoorState(true);

          beep(300);
          delay(3000);

          doorServo.write(100); // Đóng cửa lại
          updateFirebaseDoorState(false);

          lcd.clear();
          lcd.print("Nhap mat khau:");
        } else {
          lcd.clear();
          lcd.print("Sai! Thu lai");
          for (int i = 0; i < 3; i++) {
            beep(200);
            delay(200);
          }
          lcd.clear();
          lcd.print("Nhap mat khau:");
        }
        input = "";
      } else {
        if (!confirmedOld) {
          if (input == password) {
            confirmedOld = true;
            lcd.clear();
            lcd.print("MK moi:");
          } else {
            lcd.clear();
            lcd.print("Sai MK cu!");
            changingPass = false;
            delay(2000);
            lcd.clear();
            lcd.print("Nhap mat khau:");
          }
          input = "";
        } else {
          newPass = input;
          password = newPass;
          lcd.clear();
          lcd.print("Da doi MK");
          delay(2000);
          lcd.clear();
          lcd.print("Nhap mat khau:");
          changingPass = false;
          confirmedOld = false;
          input = "";
        }
      }
    } else if (key == 'C') {
      input = "";
      lcd.setCursor(0, 1);
      lcd.print("Clear         ");
    } else if (key == 'D') {
      lcd.clear();
      lcd.print("MK cu:");
      changingPass = true;
      confirmedOld = false;
      input = "";
    } else {
      input += key;
      lcd.setCursor(0, 1);
      lcd.print("Nhap: ");
      for (int i = 0; i < input.length(); i++) {
    lcd.print('*');  
    }}
  } else if (!key) {
    lastKey = 0;
  }
}