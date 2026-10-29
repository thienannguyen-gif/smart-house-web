#define BTN_TANG_1     2
#define BTN_TANG_2     3
#define BTN_TANG_3     4

#define LIMIT_TANG_1   6
#define LIMIT_TANG_2   8
#define LIMIT_TANG_3   13

#define MOTOR_ENA      9
// đổi chân
#define MOTOR_IN1 11
#define MOTOR_IN2 10

enum Floor { NONE = 0, TANG_1 = 1, TANG_2 = 2, TANG_3 = 3 };

Floor currentFloor = NONE;
Floor targetFloor = NONE;

bool lastBtn1 = HIGH;
bool lastBtn2 = HIGH;
bool lastBtn3 = HIGH;

void setup() {
  pinMode(BTN_TANG_1, INPUT_PULLUP);
  pinMode(BTN_TANG_2, INPUT_PULLUP);
  pinMode(BTN_TANG_3, INPUT_PULLUP); // Chân D7

  pinMode(LIMIT_TANG_1, INPUT_PULLUP);
  pinMode(LIMIT_TANG_2, INPUT_PULLUP);
  pinMode(LIMIT_TANG_3, INPUT_PULLUP);

  pinMode(MOTOR_ENA, OUTPUT);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.begin(9600);

  // Kiểm tra tầng hiện tại
  if (digitalRead(LIMIT_TANG_1) == LOW) {
    currentFloor = TANG_1;
    Serial.println(">> Khởi động tại TẦNG 1");
  } else if (digitalRead(LIMIT_TANG_2) == LOW) {
    currentFloor = TANG_2;
    Serial.println(">> Khởi động tại TẦNG 2");
  } else if (digitalRead(LIMIT_TANG_3) == LOW) {
    currentFloor = TANG_3;
    Serial.println(">> Khởi động tại TẦNG 3");
  } else {
    currentFloor = NONE;
    Serial.println(">> Chưa ở tầng nào (đang ở giữa)");
  }

  stopMotor();
}

void loop() {
  // Cập nhật công tắc hành trình
  if (digitalRead(LIMIT_TANG_1) == LOW) {
    currentFloor = TANG_1;
    stopMotor();
    Serial.println(">> Đã tới TẦNG 1");
  } else if (digitalRead(LIMIT_TANG_2) == LOW) {
    currentFloor = TANG_2;
    stopMotor();
    Serial.println(">> Đã tới TẦNG 2");
  } else if (digitalRead(LIMIT_TANG_3) == LOW) {
    currentFloor = TANG_3;
    stopMotor();
    Serial.println(">> Đã tới TẦNG 3");
  }

  // Đọc nút nhấn
  bool btn1 = digitalRead(BTN_TANG_1);
  bool btn2 = digitalRead(BTN_TANG_2);
  bool btn3 = digitalRead(BTN_TANG_3);

  if (targetFloor == NONE) {
    if (lastBtn1 == HIGH && btn1 == LOW && currentFloor != TANG_1) {
      targetFloor = TANG_1;
      Serial.println(">> GỌI TẦNG 1");
    }
    if (lastBtn2 == HIGH && btn2 == LOW && currentFloor != TANG_2) {
      targetFloor = TANG_2;
      Serial.println(">> GỌI TẦNG 2");
    }
    if (lastBtn3 == HIGH && btn3 == LOW && currentFloor != TANG_3) {
      targetFloor = TANG_3;
      Serial.println(">> GỌI TẦNG 3");
    }
  }

  lastBtn1 = btn1;
  lastBtn2 = btn2;
  lastBtn3 = btn3;

  // Điều khiển motor
  if (targetFloor != NONE && currentFloor != targetFloor) {
    if (targetFloor > currentFloor) {
      moveUp();
    } else {
      moveDown();
    }
  }

  if (targetFloor != NONE && currentFloor == targetFloor) {
    stopMotor();
    Serial.println(">> ĐÃ TỚI TẦNG MỤC TIÊU");
    targetFloor = NONE;
  }

  delay(10);
}

void moveUp() {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 200);  // Sửa từ 20 → 30
  Serial.println(">> ĐANG LÊN");
}

void moveDown() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  analogWrite(MOTOR_ENA, 70);  // Sửa từ 20 → 30
  Serial.println(">> ĐANG XUỐNG");
}

void stopMotor() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
  Serial.println(">> DỪNG MOTOR");
}
