#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// ==== Trạng thái ====
int state = 0; // 0: Loading, 1: Smart Home, 2+: Câu nói
unsigned long stateStartTime = 0;
int progress = 0;
int quoteIndex = 0;

// ==== Danh sách câu nói ====
const char* quotes[] = {
  "Chao mung ban den voi khong gian song ly tuong.",
  "Hay yeu ngoi nha cua ban nhu yeu chinh ban than minh."
};
const int quoteCount = sizeof(quotes) / sizeof(quotes[0]);

void setup() {
  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED khong ket noi"));
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  stateStartTime = millis();
}

void loop() {
  display.clearDisplay();

  switch (state) {
    case 0: // Loading
      drawLoadingBar();
      if (progress > 100) {
        progress = 0;
        state = 1;
        stateStartTime = millis();
      }
      break;

    case 1: // Smart Home Welcome
      showSmartHome();
      if (millis() - stateStartTime > 4000) {
        state = 2;
        stateStartTime = millis();
      }
      break;

    default: // Các câu nói
      showQuote(quotes[quoteIndex]);
      if (millis() - stateStartTime > 4000) {
        quoteIndex++;
        if (quoteIndex >= quoteCount) {
          quoteIndex = 0;
          state = 0; // Quay lại loading
        } else {
          state++;
        }
        stateStartTime = millis();
      }
      break;
  }

  display.display();
  delay(50);
}

// ==== Loading "Building..." + chỉ dấu % ====
void drawLoadingBar() {
  int barWidth = 100;
  int barHeight = 10;
  int barX = (SCREEN_WIDTH - barWidth) / 2;
  int barY = 30;

  display.setCursor((SCREEN_WIDTH - 60) / 2, 12);
  display.setTextSize(1);
  display.print("Building...");

  display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
  int filled = map(progress, 0, 100, 0, barWidth - 2);
  display.fillRect(barX + 1, barY + 1, filled, barHeight - 2, SSD1306_WHITE);

  // Chỉ hiện dấu %
  display.setCursor(barX + barWidth + 4, barY);
  display.print("%");

  progress++;
  delay(3); // Tăng tốc độ chạy %
}

// ==== Hiệu ứng gõ chữ Smart Home ====
void showSmartHome() {
  String line1 = "Smart Home";
  String line2 = "WELCOME!";
  int len1 = (millis() - stateStartTime) / 150;
  int len2 = (millis() - stateStartTime - 800) / 150;

  if (len1 > line1.length()) len1 = line1.length();
  if (len2 < 0) len2 = 0;
  if (len2 > line2.length()) len2 = line2.length();

  display.setTextSize(2);
  int16_t x1, y1;
  uint16_t w1, h1, w2, h2;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
  display.getTextBounds(line2, 0, 0, &x1, &y1, &w2, &h2);

  int yStart = (SCREEN_HEIGHT - (h1 + h2 + 6)) / 2;
  int x1c = (SCREEN_WIDTH - w1) / 2;
  int x2c = (SCREEN_WIDTH - w2) / 2;

  display.setCursor(x1c, yStart);
  display.print(line1.substring(0, len1));
  display.setCursor(x2c, yStart + h1 + 6);
  display.print(line2.substring(0, len2));
}

// ==== Hiển thị từng câu nói (căn giữa, xuống dòng theo từ) ====
void showQuote(const char* quote) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  String q = String(quote);
  const int maxCharsPerLine = 21;
  int lineHeight = 10;
  int y = 8;

  // Tách câu thành từ
  String word, line = "";
  for (int i = 0; i <= q.length(); i++) {
    char c = q.charAt(i);
    if (c == ' ' || c == '\0') {
      if ((line.length() + word.length()) <= maxCharsPerLine) {
        line += word + ' ';
      } else {
        // In dòng hiện tại
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
        int x = (SCREEN_WIDTH - w) / 2;
        display.setCursor(x, y);
        display.print(line);
        y += lineHeight;

        // Bắt đầu dòng mới
        line = word + ' ';
      }
      word = "";
    } else {
      word += c;
    }
  }

  // In dòng cuối cùng nếu còn
  if (line.length() > 0) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x, y);
    display.print(line);
  }
}
