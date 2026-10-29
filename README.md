# Smart House – Nhà IoT 3 Tầng

Web điều khiển và mô hình 3D cho mô hình nhà thông minh: các mạch ESP8266 ghi/đọc Firebase Realtime Database (project `smart-house-fcd4f`), web đọc/ghi qua API riêng.

```
Trang web (public/nha-iot.html)  →  /api/devices (Next.js)  →  Firebase /home  ←  mạch ESP8266 (firmware/)
```

## Thư mục

| Đường dẫn | Nội dung |
|---|---|
| `public/nha-iot.html` | Trang điều khiển + mô hình 3D (mô phỏng, đi vào nhà, kết nối API hoặc Firebase) |
| `src/app/api/devices/` | API: `GET /api/devices`, `GET` và `POST /api/devices/{id}` |
| `src/lib/devices.js` | Danh mục 11 thiết bị, đường dẫn Firebase, cách dịch lệnh |
| `src/lib/firebaseAdmin.js` | Kết nối Firebase bằng khóa service account |
| `src/lib/apiAuth.js` | Kiểm tra khóa API, CORS |
| `.env` | Cấu hình Firebase, khóa service account, khóa API. **Bí mật, không chia sẻ, không đưa lên GitHub** |
| `firmware/` | Code Arduino cho các mạch (đã trỏ sang project mới) |

## Chạy web

Cần Node.js 18.18 trở lên.

```
npm install
npm run dev
```

Mở http://localhost:3000 (tự chuyển tới trang điều khiển). Bấm **Mạch thật → Qua API riêng**, để trống địa chỉ, nhập khóa API (dòng `SMART_HOME_API_KEY` trong `.env`), bấm **Kết nối**.

Kiểm tra nhanh API: mở http://localhost:3000/api/devices. Thấy "Sai hoặc thiếu khóa API" là server đã chạy đúng.

## Nạp firmware

1. Trong Firebase → Authentication → Utilisateurs → **Ajouter un utilisateur**: tạo tài khoản cho mạch.
2. Trong 6 file dưới đây, thay `DIEN_EMAIL_TAI_KHOAN_MACH` và `DIEN_MAT_KHAU_TAI_KHOAN_MACH` bằng tài khoản vừa tạo; sửa `WIFI_SSID`, `WIFI_PASSWORD` nếu đổi Wi-Fi.
3. Arduino IDE → Library Manager cài: **Firebase Arduino Client Library for ESP8266 and ESP32** (Mobizt), **LiquidCrystal I2C**, **Keypad**, **Keypad_I2C**, **DHT sensor library**, **Adafruit Unified Sensor**, **Adafruit Fingerprint Sensor Library**, **Adafruit SSD1306**, **Adafruit GFX**. Board: NodeMCU 1.0 (ESP-12E).

| Firmware | Mạch | Thiết bị |
|---|---|---|
| `cua_chinh` | ESP8266 | Tầng 1: cửa chính (bàn phím, LCD, servo), đèn tầng 1 |
| `nha_xe_moi_nhat` | ESP8266 | Tầng 1: cửa cuốn nhà xe |
| `den_wc` | ESP8266 | Tầng 1: 2 đèn WC tự bật bằng PIR |
| `bao_trom_va_do_am` | ESP8266 | Tầng 1: báo trộm PIR, DHT11, LCD |
| `baochay_denl2` | ESP8266 | Tầng 2: cảm biến khói, còi, đèn, nút nhấn |
| `van_tay` | ESP8266 | Tầng 3: cửa vân tay, đèn tầng 3 |
| `thang_may` | Arduino Uno | Thang máy 3 tầng (không nối mạng) |
| `led_oled` | ESP8266 | Màn hình chào OLED (không nối mạng) |

## Đưa lên mạng (tùy chọn)

Đẩy thư mục lên GitHub (file `.env` đã nằm trong `.gitignore`), tạo project trên Vercel, rồi vào Settings → Environment Variables chép từng dòng trong `.env` vào.
