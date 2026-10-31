// Ví dụ cơ bản cho WiFiShare.
//
// Nạp cùng một tên và mật khẩu Wi-Fi cài đặt cho mọi mạch, chỉ đổi DEVICE_NAME. Lần đầu (chưa có
// mạng) hoặc khi không vào được mạng, một mạch sẽ phát Wi-Fi "NhaToi-CaiDat": vào bằng điện thoại,
// trang cài đặt tự mở (hoặc mở http://192.168.4.1), chọn mạng và nhập mật khẩu.
//
// Đèn trên mạch: tắt = đã vào mạng, sáng = đang vào mạng, nháy nhanh = đang làm mạch chủ,
// nháy chậm = đang chờ mạch chủ.
#include <WiFiShare.h>

#define DEVICE_NAME "mach_thu"
#define SETUP_SSID "NhaToi-CaiDat"
#define SETUP_PASSWORD "doi-mat-khau-nay"  // từ 8 ký tự, đặt riêng cho nhà bạn

#if defined(ESP8266)
const int LED_PIN = 2;  // đèn xanh trên NodeMCU, sáng khi mức LOW
const bool LED_ON = LOW;
#else
const int LED_PIN = 2;  // đèn trên đa số bo ESP32 DevKit
const bool LED_ON = HIGH;
#endif

WiFiShare wifiShare;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Có thể đặt sẵn mạng cho lần nạp đầu, mạng nhập trên trang cài đặt sẽ thay nó:
  // wifiShare.setDefaultNetwork("TenWiFi", "MatKhauWiFi");
  wifiShare.onStateChange([](WiFiShareState state) {
    Serial.printf("Trạng thái: %s\n", wifiShare.stateName());
    if (state == WiFiShareState::Master) Serial.println("Vào Wi-Fi " SETUP_SSID " bằng điện thoại để cài mạng");
  });
  wifiShare.begin(DEVICE_NAME, SETUP_SSID, SETUP_PASSWORD);
}

void loop() {
  wifiShare.loop();

  // Đèn báo trạng thái
  unsigned long now = millis();
  bool on;
  switch (wifiShare.state()) {
    case WiFiShareState::Online: on = false; break;
    case WiFiShareState::Connecting: on = true; break;
    case WiFiShareState::Master: on = (now / 150) % 2; break;
    default: on = (now / 700) % 2;
  }
  digitalWrite(LED_PIN, on ? LED_ON : !LED_ON);

  if (wifiShare.online()) {
    // Phần cần mạng đặt ở đây
  }
}
