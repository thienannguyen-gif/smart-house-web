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

1. Trong Firebase → Authentication → Utilisateurs → **Ajouter un utilisateur**: tạo tài khoản cho mạch. Nếu đây là tài khoản mới (không phải tài khoản đang có), thêm UID của nó vào Rules, xem mục [Bảo mật và hạn mức Firebase](#bảo-mật-và-hạn-mức-firebase).
2. Trong 6 file dưới đây, thay `DIEN_MAT_KHAU_WIFI`, `DIEN_EMAIL_TAI_KHOAN_MACH` và `DIEN_MAT_KHAU_TAI_KHOAN_MACH` bằng mật khẩu Wi-Fi và tài khoản vừa tạo; sửa `WIFI_SSID` nếu đổi Wi-Fi. **Không commit file đã điền mật khẩu lên GitHub.**
3. Arduino IDE → Library Manager cài: **Firebase Arduino Client Library for ESP8266 and ESP32** (Mobizt), **LiquidCrystal I2C**, **Keypad**, **DHT sensor library**, **Adafruit Unified Sensor**, **Adafruit Fingerprint Sensor Library**, **Adafruit SSD1306**, **Adafruit GFX**. **Keypad_I2C** không có trong Library Manager: tải [joeyoung/arduino_keypads](https://github.com/joeyoung/arduino_keypads), chép thư mục `Keypad_I2C` vào `Documents/Arduino/libraries`. Board: NodeMCU 1.0 (ESP-12E).

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

## Bảo mật và hạn mức Firebase

- **Rules** chỉ cho đúng tài khoản của mạch đọc/ghi nhánh `/home`. Thêm tài khoản khác (vd. cho web ở chế độ Firebase) thì vào Realtime Database → Rules, thêm `|| auth.uid === '<UID>'` vào cả `.read` và `.write`. Web ở chế độ API dùng service account nên không bị Rules chặn.
- **Tự đăng ký tài khoản đã tắt** (Authentication → Settings → User actions). API key nằm công khai trong code, nên nếu bật lại thì ai cũng tạo được tài khoản.
- **Hạn mức gói Spark miễn phí: tải xuống 10 GB/tháng**, tính cả header HTTP và mã hóa. Vì vậy firmware nhận lệnh qua **stream** (giữ một kết nối, chỉ nhận dữ liệu khi giá trị đổi) và chỉ ghi khi giá trị đổi, không đọc/ghi Firebase ở mỗi vòng `loop()`. Khi sửa firmware, giữ nguyên cách này. Theo dõi mức dùng ở Realtime Database → Usage.
- Trang web ở chế độ API hỏi `/api/devices` mỗi 1,5 giây và tự ngừng khi tab bị ẩn.

## Đưa lên mạng (tùy chọn)

Đẩy thư mục lên GitHub (file `.env` đã nằm trong `.gitignore`), tạo project trên Vercel, rồi vào Settings → Environment Variables chép từng dòng trong `.env` vào.
