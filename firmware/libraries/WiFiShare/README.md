# WiFiShare

Thư viện Arduino cho ESP8266 và ESP32. Các mạch trong nhà dùng chung một mạng Wi-Fi. Khi không vào được mạng (đổi router, đổi mật khẩu), một mạch tự làm **mạch chủ** và phát Wi-Fi cài đặt. Bạn nhập mạng mới **một lần** trên điện thoại. Mạch chủ gửi mạng đó cho các mạch còn lại; mỗi mạch tự vào mạng mới rồi báo lại cho mạch chủ.

## Cách hoạt động

1. Mạch vào mạng đã lưu. Vào được thì chạy bình thường, không phát gì.
2. Sau 45 giây vẫn không vào được, hoặc mất mạng quá 45 giây, mạch đi tìm mạch chủ. Mỗi mạch chờ 0–15 giây (khác nhau theo địa chỉ MAC), rồi quét xem đã có Wi-Fi cài đặt chưa:
   - Có rồi: vào Wi-Fi đó làm **mạch con**, 3 giây hỏi mạch chủ một lần.
   - Chưa có: tự làm **mạch chủ**, phát Wi-Fi cài đặt. Nếu hai mạch cùng làm mạch chủ, mạch có địa chỉ lớn hơn sẽ nhường.

   Mạch chủ chỉ phát Wi-Fi cài đặt khi chính nó không vào được mạng. Mạch nào vẫn vào được mạng thì không làm mạch chủ, cũng không vào Wi-Fi cài đặt.
3. Bạn vào Wi-Fi cài đặt bằng điện thoại. Trang cài đặt tự mở; nếu không, mở `http://192.168.4.1`. Trang hiện các mạch đang chờ và các mạng xung quanh.
4. Chọn mạng, nhập mật khẩu, bấm **Lưu và chuyển mạng**. Mạch chủ thử vào mạng đó trước:
   - **Sai mật khẩu hoặc không thấy mạng:** trang báo lỗi, các mạch giữ nguyên.
   - **Vào được:** mạch chủ gửi mạng cho từng mạch con.
5. Mỗi mạch con nhận mạng xong thì lưu lại, **rời Wi-Fi cài đặt và tự vào mạng mới**. Vào được thì nó báo về mạch chủ qua chính mạng mới, và không quay lại Wi-Fi cài đặt nữa. Trang cài đặt hiện mạch nào đã vào mạng.
6. Mọi mạch con đã báo thì mạch chủ tắt Wi-Fi cài đặt và chạy bình thường. Mạch con nào chưa vào được thì quay lại Wi-Fi cài đặt, mạch chủ gửi lại, tối đa 3 lần. Mạch chủ chờ mỗi mạch tối đa 2 phút, và giữ Wi-Fi cài đặt không quá 5 phút.
7. Mạng mới được lưu trong mạch (ESP8266: LittleFS, ESP32: NVS), nên khởi động lại vẫn còn. Mạng đã lưu được ưu tiên hơn mạng viết trong code.

Mạch nhận ra mạng cũ quay lại (vd. router mất điện rồi bật lại) và tự về, không cần ai nhập gì:

- Mạch chủ và mạch con quét định kỳ. Thấy mạng đã lưu xuất hiện lại thì vào thử ngay.
- Mạng vẫn hiện mà vào không được (vd. mật khẩu đã đổi) thì thử thưa dần, mỗi lần cách gấp đôi lần trước, từ 1 phút lên tối đa 16 phút.
- Mạch chủ vào lại được mạng cũ thì cũng gửi mạng đó cho các mạch con như ở bước 5, rồi tắt Wi-Fi cài đặt.
- Lúc trang cài đặt đang mở trên điện thoại, mạch chủ không tự thử, để điện thoại khỏi bị ngắt.

**Mạch bị lạc:** lúc đổi mạng, một mạch có thể đang tắt. Bật lên sau, nó chỉ có mạng cũ, không vào được, nên tự làm mạch chủ và phát Wi-Fi cài đặt. Các mạch đã ở mạng mới không sang giúp. Bạn vào Wi-Fi cài đặt và nhập mạng mới thêm một lần cho mạch đó.

## Cài thư viện

Chọn một trong hai cách:

- Chép thư mục `WiFiShare` vào `Documents/Arduino/libraries`.
- Arduino IDE → Sketch → Include Library → **Add .ZIP Library** → chọn `WiFiShare-1.1.0.zip`.

Không cần cài thêm thư viện nào. Mọi thứ thư viện dùng đều có sẵn trong core ESP8266/ESP32.

## Dùng

```cpp
#include <WiFiShare.h>

WiFiShare wifiShare;

void setup() {
  wifiShare.begin("cua_chinh", "NhaToi-CaiDat", "mat-khau-nhom");
}

void loop() {
  wifiShare.loop();
  // phần không cần mạng (nút, cảm biến, còi)
  if (wifiShare.online()) {
    // phần cần mạng (Firebase, MQTT...)
  }
}
```

Mọi mạch nạp **cùng tên và mật khẩu Wi-Fi cài đặt**, chỉ khác tên mạch (tham số đầu). Một số lưu ý:

- **Đặt tên Wi-Fi cài đặt riêng cho nhà bạn**, vd. thêm số nhà, để không trùng với nhà hàng xóm cũng dùng thư viện này.
- **Mật khẩu Wi-Fi cài đặt:** từ 8 ký tự. Giữ kín (xem [Bảo mật](#bảo-mật)), **không commit lên GitHub**.
- **Lần nạp đầu:** không cần mật khẩu Wi-Fi nhà trong code. Mạch chưa có mạng sẽ mở Wi-Fi cài đặt, bạn nhập trên điện thoại. Muốn đặt sẵn thì gọi `setDefaultNetwork("TenWiFi", "MatKhau")` trước `begin()`.

Ví dụ trong `examples/`:

| Ví dụ | Nội dung |
|---|---|
| `CoBan` | Đèn trên mạch báo trạng thái |
| `VoiFirebase` | Ghép với thư viện Firebase của Mobizt: nhận lệnh đèn qua stream |

## Ghép vào firmware có sẵn

1. Bỏ đoạn tự vào Wi-Fi: `WiFi.begin(...)` và vòng `while (WiFi.status() != WL_CONNECTED)`.
2. Gọi `wifiShare.begin(...)` trong `setup()` và `wifiShare.loop()` ở đầu `loop()`.
3. **Không gọi `delay()` dài**, vì trang cài đặt và việc trao đổi giữa các mạch chạy trong `wifiShare.loop()`. Một `delay(3000)` lúc còi kêu vẫn chạy được, chỉ chậm hơn.
4. Chỉ gọi Firebase hay Internet khi `wifiShare.online()`. Lúc mạch đang làm mạch chủ hoặc mạch con, nó nối với Wi-Fi cài đặt chứ không phải mạng nhà.
5. **Thư viện Firebase của Mobizt:** gọi `Firebase.reconnectNetwork(false)` **trước** `Firebase.begin()`. Nếu để `true`, thư viện Firebase tự nối lại Wi-Fi và làm hỏng Wi-Fi cài đặt.

## Các hàm

| Hàm | Dùng để |
|---|---|
| `begin(tênMạch, tênWiFiCàiĐặt, mậtKhẩuWiFiCàiĐặt)` | Bắt đầu. Gọi một lần trong `setup()` |
| `loop()` | Gọi ở mỗi vòng `loop()` |
| `online()` | `true` khi đã vào mạng nhà |
| `state()`, `stateName()` | Trạng thái hiện tại, xem bảng dưới |
| `ssid()` | Tên mạng đang lưu |
| `forgetNetwork()` | Xóa mạng đã lưu rồi đi tìm mạch chủ |
| `onStateChange(hàm)` | Gọi hàm mỗi khi đổi trạng thái (vd. đổi kiểu nháy đèn) |

Các hàm cài đặt sau gọi **trước** `begin()`:

| Hàm | Mặc định | Ý nghĩa |
|---|---|---|
| `setDefaultNetwork(ssid, pass)` | không có | Mạng dùng khi chưa lưu mạng nào |
| `setCanBeMaster(false)` | `true` | Mạch này không bao giờ làm mạch chủ (vd. mạch ở chỗ sóng yếu). Mạch như vậy không vào được mạng thì chỉ nhận mạng mới khi có mạch khác đang làm mạch chủ; nếu chỉ mình nó bị lạc, phải nạp lại |
| `setConnectTimeout(ms)` | 45 giây | Không vào được (hoặc mất) mạng bao lâu thì đi tìm mạch chủ |
| `setLogging(false)` | bật | Tắt thông báo `[WiFiShare]` trên Serial |

| Trạng thái | Nghĩa |
|---|---|
| `Connecting` | Đang vào mạng đã lưu |
| `Online` | Đã vào mạng |
| `Electing` | Không vào được mạng, đang tìm mạch chủ |
| `Master` | Đang phát Wi-Fi cài đặt |
| `Joining` | Đang vào Wi-Fi cài đặt của mạch chủ |
| `Slave` | Đang chờ mạch chủ gửi mạng mới |

## Đổi sang mạng mới

- **Đổi mật khẩu router, hoặc thay router mới và tắt router cũ:** các mạch không vào được mạng, khoảng 1 phút sau Wi-Fi cài đặt hiện ra. Vào đó và nhập mạng mới một lần.
- **Mạng cũ vẫn chạy:** các mạch vẫn ở mạng cũ, vì mạch chủ chỉ xuất hiện khi không vào được mạng. Muốn chuyển thì tắt mạng cũ (hoặc đổi mật khẩu của nó) trước.

## Giới hạn

- **Wi-Fi 2,4 GHz:** ESP8266 và đa số ESP32 không vào được mạng 5 GHz.
- **Số mạch:** Wi-Fi cài đặt nhận tối đa 8 kết nối (giới hạn của ESP8266), tính cả điện thoại. Vì vậy dùng được tối đa 7 mạch con cùng lúc.
- **Router chặn các thiết bị nói chuyện với nhau** (cách ly máy khách, mạng khách): mạch con vẫn vào được mạng nhưng không báo về được. Mạch chủ chờ mỗi mạch 2 phút rồi tự tắt Wi-Fi cài đặt.
- **Mất Internet:** thư viện chỉ xét mạch có vào được Wi-Fi hay không. Router vẫn chạy mà mất Internet (nhà mạng lỗi) thì các mạch vẫn ở mạng đó, không mở Wi-Fi cài đặt, vì nhập lại Wi-Fi không sửa được lỗi này.
- **Đổi kênh khi thử mạng:** lúc mạch chủ thử mạng, Wi-Fi cài đặt có thể đổi kênh theo router. Điện thoại có thể bị ngắt vài giây; vào lại rồi mở `192.168.4.1` để xem kết quả.
- **Kênh 12, 13:** Wi-Fi cài đặt chỉ phát ở kênh 1–11. Nếu router nhà dùng kênh 12 hoặc 13, nên đổi router sang kênh 1–11, vì một số mạch ESP32 không thấy hai kênh này.
- **Mạng ẩn, chỗ có quá nhiều mạng:** mạch chủ và mạch con vẫn thử vào lại mạng đã lưu theo lịch thưa dần, kể cả khi quét không thấy mạng đó. Mỗi lần thử, Wi-Fi cài đặt bị gián đoạn khoảng 20 giây.
- **ESP8266:** mạng được lưu bằng LittleFS. Chọn Flash Size có FS (mặc định của NodeMCU có 2 MB). Nếu sketch đang dùng SPIFFS thì LittleFS sẽ định dạng lại vùng đó.
- **ESP32 kèm thư viện Firebase:** ví dụ `VoiFirebase` dùng 97% vùng chương trình mặc định. Nên chọn Partition Scheme **Huge APP** hoặc **Minimal SPIFFS**.

## Bảo mật

Mật khẩu Wi-Fi cài đặt bảo vệ việc đổi mạng:

- **Mạng đi qua Wi-Fi cài đặt:** mật khẩu mạng mới truyền từ mạch chủ sang mạch con qua Wi-Fi cài đặt (WPA2, mã hóa bằng mật khẩu Wi-Fi cài đặt).
- **Ai biết mật khẩu này có thể đổi mạng:** họ phát Wi-Fi cùng tên, cùng mật khẩu là đổi được mạng của các mạch đang chờ.
- **Ai biết mật khẩu này có thể nhận mạng mới:** lúc các mạch đang chờ, họ vào Wi-Fi cài đặt giả làm mạch con là nhận được mạng bạn vừa nhập, kể cả mật khẩu.

Hai việc trên chỉ làm được lúc các mạch đang không vào được mạng. Lúc các mạch đang chạy bình thường, không mạch nào phát hay vào Wi-Fi cài đặt. Vì vậy:

- **Giữ kín mật khẩu Wi-Fi cài đặt**, và không đưa lên GitHub. Firmware công khai thì để chỗ trống như `DIEN_MAT_KHAU_CAI_DAT` và chỉ điền lúc nạp.
- **Trang cài đặt và các lệnh giữa các mạch** chỉ nhận yêu cầu gửi thẳng tới địa chỉ của mạch chủ, để chặn trang web lạ gửi lệnh qua trình duyệt điện thoại.

Đã biên dịch thử trên ESP8266 core 3.1.2 (NodeMCU 1.0) và ESP32 core 3.3.12, nhưng chưa chạy trên mạch thật.
