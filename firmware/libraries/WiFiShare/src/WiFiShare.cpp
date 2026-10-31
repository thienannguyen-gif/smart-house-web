#include "WiFiShare.h"
#include "WiFiSharePage.h"

#include <stdarg.h>

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#else
#include <HTTPClient.h>
#include <Preferences.h>
#endif

// ESP8266 để chuỗi thường trong RAM, nên chuỗi thông báo để trong flash (PSTR, F). Trên ESP32 hai
// macro này không làm gì.
#define LOG(format, ...) logP(PSTR(format), ##__VA_ARGS__)

namespace {

// Thời gian, tính bằng mili giây
const uint32_t HOME_RETRY_TIMEOUT = 20000;   // mạch chủ, mạch con thử vào lại mạng đã lưu
const uint32_t JOIN_TIMEOUT = 15000;         // vào Wi-Fi cài đặt
const uint32_t PHASE_DELAY = 4000;           // mạch chủ báo bận cho các mạch con trước khi đổi kênh
const uint32_t VERIFY_TIMEOUT = 20000;       // mạch chủ thử mạng mới
const uint32_t POLL_INTERVAL = 3000;         // mạch con hỏi mạch chủ
// Mạch con hỏi trong khoảng này mới tính là đang chờ. Mạch con có thể rời đi gần một phút để thử
// vào lại mạng cũ rồi mới quay lại.
const uint32_t PEER_WINDOW = 90000;
const uint32_t LOST_REPLY = 20000;           // mạch con hỏi lại sớm hơn chừng này: lần gửi trước không tới
const uint32_t REJOIN_INTERVAL = 10000;      // online mà mất mạng: vào lại mạng sau mỗi khoảng này
const uint32_t SEND_MIN = 10000;             // mạch chủ giữ Wi-Fi cài đặt ít nhất chừng này để mạch con kịp hỏi...
const uint32_t SEND_MAX = 5UL * 60000;       // ...và không quá chừng này, dù còn mạch chưa báo
const uint32_t DONE_WAIT = 120000;           // gửi mạng xong, chờ mạch con báo đã vào mạng
const uint8_t MAX_SENDS = 3;                 // gửi một mạch chừng này lần vẫn không vào được thì thôi
const uint32_t REPORT_INTERVAL = 2000;       // mạch con báo về mạch chủ...
const uint8_t REPORT_TRIES = 5;              // ...tối đa chừng này lần
const uint32_t MASTER_FIRST_SCAN = 3000;     // quét sớm để phát hiện hai mạch cùng làm mạch chủ
const uint32_t MASTER_SCAN_INTERVAL = 30000;
const uint32_t SLAVE_SCAN_INTERVAL = 60000;
const uint32_t PORTAL_QUIET = 60000;         // người dùng mở trang trong khoảng này thì không tự đổi kênh
const uint32_t LOST_MASTER = 10000;          // mạch con mất mạch chủ quá lâu thì thôi chờ...
const uint32_t LOST_MASTER_BUSY = 45000;     // ...lâu hơn nếu mạch chủ vừa báo đang đổi kênh
const uint32_t BUSY_MEMORY = 60000;
const uint32_t RETRY_MIN = 60000;
const uint32_t RETRY_MAX = 16UL * 60000;
const uint32_t BAD_MEMORY = 30UL * 60000;    // bỏ qua Wi-Fi cài đặt vào không được trong chừng này
const uint32_t SCAN_TIMEOUT = 15000;
const uint16_t HTTP_TIMEOUT = 3000;
const char* const EXAMPLE_PASSWORD = "doi-mat-khau-nay";  // mật khẩu trong các ví dụ

#if defined(ESP8266)
const char* const FILE_PATH = "/wifishare.txt";
#else
const char* const NVS_NAMESPACE = "wifishare";
#endif

const char* const STATE_NAMES[] = {"connecting", "online", "electing", "master", "joining", "slave"};

uint32_t since(uint32_t t) { return millis() - t; }

// millis() có thể bằng 0 lúc mới khởi động, mà 0 dùng để báo "chưa có"
uint32_t stamp() {
  uint32_t t = millis();
  return t ? t : 1;
}

const char* passOrNull(const String& pass) { return pass.length() ? pass.c_str() : nullptr; }

bool lessMac(const uint8_t* a, const uint8_t* b) { return memcmp(a, b, 6) < 0; }

// Tên mạng và mật khẩu đi qua HTTP và lưu vào file dưới dạng hex, để khỏi lo ký tự đặc biệt
String toHex(const String& s) {
  static const char digits[] = "0123456789abcdef";
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool fromHex(const String& hex, String& out) {
  if (hex.length() % 2) return false;
  out = "";
  out.reserve(hex.length() / 2);
  for (size_t i = 0; i < hex.length(); i += 2) {
    int hi = hexValue(hex[i]), lo = hexValue(hex[i + 1]);
    if (hi < 0 || lo < 0 || (hi == 0 && lo == 0)) return false;
    out += (char)(hi * 16 + lo);
  }
  return true;
}

void addJson(String& out, const String& s) {
  out += '"';
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof buf, "\\u%04x", (unsigned)c);
      out += buf;
    } else {
      out += c;
    }
  }
  out += '"';
}

bool isOpen(int i) {
#if defined(ESP8266)
  return WiFi.encryptionType(i) == ENC_TYPE_NONE;
#else
  return WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
#endif
}

}  // namespace

// ================== Dùng từ sketch ==================

void WiFiShare::begin(const char* deviceName, const char* setupSsid, const char* setupPassword) {
  _name = deviceName;
  _apSsid = setupSsid;
  _apPass = setupPassword;
  if (_apPass.length() < 8) LOG("Mật khẩu Wi-Fi cài đặt phải có ít nhất 8 ký tự, mạch này sẽ không làm mạch chủ được");
  if (_apPass == EXAMPLE_PASSWORD) {
    // Ai biết mật khẩu này thì vào được Wi-Fi cài đặt lúc các mạch đang chờ, và nhận được mạng mới
    LOG("Đang dùng mật khẩu Wi-Fi cài đặt của ví dụ, ai cũng biết: hãy đổi");
  }

  // Không để SDK tự lưu mạng vào flash, vì mạch còn vào cả Wi-Fi cài đặt. Thư viện tự lưu mạng nhà.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(_id, sizeof _id, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  _jitter = (uint16_t)(mac[4] << 8 | mac[5]);
  _homeBackoff = RETRY_MIN;

  if (!loadNetwork()) {
    _ssid = _defSsid;
    _pass = _defPass;
  }
  _begun = true;
  LOG("Mạch %s (%s)", _name.c_str(), _id);
  if (_ssid.length()) {
    connectHome(_connectTimeout);
  } else {
    LOG("Chưa có mạng nào");
    startElection();
  }
}

void WiFiShare::setDefaultNetwork(const char* ssid, const char* password) {
  _defSsid = ssid ? ssid : "";
  _defPass = password ? password : "";
}

bool WiFiShare::online() const { return _state == WiFiShareState::Online && WiFi.status() == WL_CONNECTED; }

const char* WiFiShare::stateName() const { return STATE_NAMES[(uint8_t)_state]; }

void WiFiShare::forgetNetwork() {
  _ssid = "";
  _pass = "";
#if defined(ESP8266)
  if (!_fsOk) _fsOk = LittleFS.begin();  // có thể được gọi trước begin()
  if (_fsOk) LittleFS.remove(FILE_PATH);
#else
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.clear();
    prefs.end();
  }
#endif
  LOG("Đã xóa mạng đã lưu");
  if (_begun) startElection();
}

void WiFiShare::loop() {
  if (!_begun) return;
  if (_web) {
    _dns->processNextRequest();
    _web->handleClient();
  }

  bool scanned = scanFinished();
  switch (_state) {
    case WiFiShareState::Connecting: loopConnecting(); break;
    case WiFiShareState::Online: loopOnline(); break;
    case WiFiShareState::Electing: loopElecting(scanned); break;
    case WiFiShareState::Master: loopMaster(scanned); break;
    case WiFiShareState::Joining: loopJoining(); break;
    case WiFiShareState::Slave: loopSlave(scanned); break;
  }
}

// ================== Chuyển trạng thái ==================

void WiFiShare::enter(WiFiShareState state) {
  _state = state;
  _stateAt = millis();
  LOG("-> %s", stateName());
  if (_onState) _onState(state);
}

void WiFiShare::connectHome(uint32_t timeout) {
  if (_ssid.isEmpty()) {
    startElection();
    return;
  }
  stopPortal();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.setAutoReconnect(true);
  WiFi.begin(_ssid.c_str(), passOrNull(_pass));
  _timeout = timeout;
  _homeTimer = millis();
  LOG("Vào mạng %s", _ssid.c_str());
  enter(WiFiShareState::Connecting);
}

void WiFiShare::goOnline() {
  _homeBackoff = RETRY_MIN;
  _lostAt = 0;
  _reportTries = 0;
  _reportTimer = millis() - REPORT_INTERVAL;  // báo mạch chủ ngay
  LOG("Đã vào mạng %s, IP %s", _ssid.c_str(), WiFi.localIP().toString().c_str());
  enter(WiFiShareState::Online);
}

void WiFiShare::startElection() {
  stopPortal();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  _reportPending = false;  // không vào được mạng mạch chủ gửi; vào lại Wi-Fi cài đặt là mạch chủ biết
  _homeTimer = millis();
  // Mỗi mạch chờ một khoảng khác nhau (0–15 giây) rồi mới quét, để thường chỉ một mạch làm mạch chủ
  _wait = (uint32_t)(_jitter % 16) * 1000;
  LOG("Tìm mạch chủ sau %u giây", (unsigned)(_wait / 1000));
  enter(WiFiShareState::Electing);
}

void WiFiShare::startMaster(uint8_t channel) {
  stopPortal();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  if (channel < 1 || channel > 11) channel = 1;  // kênh 12, 13 không phải mạch/điện thoại nào cũng thấy
  bool ok = _apPass.length() >= 8 && WiFi.softAP(_apSsid.c_str(), _apPass.c_str(), channel, 0, 8);
  if (!ok && channel != 1) {
    channel = 1;
    ok = WiFi.softAP(_apSsid.c_str(), _apPass.c_str(), channel, 0, 8);
  }
  if (!ok) {
    LOG("Không phát được Wi-Fi cài đặt");
    connectHome(HOME_RETRY_TIMEOUT);  // không có mạng đã lưu thì connectHome chuyển sang tìm mạch chủ
    return;
  }
  WiFi.softAPmacAddress(_apBssid);
  _dns.reset(new DNSServer());
  _dns->start(53, "*", WiFi.softAPIP());  // mọi tên miền trỏ về mạch chủ, điện thoại tự mở trang cài đặt
  _web.reset(new WebServerType(80));
  setupRoutes();
  _web->begin();

  _phase = Phase::Idle;
  _peerCount = 0;
  _portalAt = 0;
  if (_ssid.isEmpty()) _message = F("Các mạch chưa có Wi-Fi.");
  else _message = String(F("Các mạch không vào được mạng ")) + _ssid + '.';
  _scanTimer = millis();
  _scanWait = MASTER_FIRST_SCAN;
  LOG("Phát Wi-Fi cài đặt \"%s\" ở kênh %u, trang cài đặt: http://%s", _apSsid.c_str(), (unsigned)channel,
      WiFi.softAPIP().toString().c_str());
  enter(WiFiShareState::Master);
}

void WiFiShare::joinMaster() {
  stopPortal();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.setAutoReconnect(true);
  WiFi.begin(_apSsid.c_str(), _apPass.c_str(), _setupChannel, _setupBssid);
  LOG("Vào Wi-Fi cài đặt của mạch chủ (kênh %u)", (unsigned)_setupChannel);
  enter(WiFiShareState::Joining);
}

void WiFiShare::stopPortal() {
  if (_web) {
    _web->stop();
    _web.reset();
  }
  if (_dns) {
    _dns->stop();
    _dns.reset();
  }
  if (WiFi.getMode() & WIFI_AP) WiFi.softAPdisconnect(true);
  _phase = Phase::Idle;
}

// ================== Từng trạng thái ==================

void WiFiShare::loopConnecting() {
  // So cả tên mạng: trên ESP32 trạng thái cập nhật qua sự kiện, ngay sau begin() có thể vẫn báo mạng cũ
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == _ssid) {
    goOnline();
  } else if (since(_stateAt) >= _timeout) {
    LOG("Không vào được mạng %s", _ssid.c_str());
    if (_homeBackoff < RETRY_MAX) _homeBackoff *= 2;
    startElection();
  }
}

void WiFiShare::loopOnline() {
  // Thư viện khác có thể tắt việc tự nối lại của Wi-Fi (vd. Firebase.reconnectNetwork(false)). Lúc
  // online thì WiFiShare lo việc này, nên bật lại.
  if (!WiFi.getAutoReconnect()) WiFi.setAutoReconnect(true);
  if (WiFi.status() != WL_CONNECTED) {
    if (!_lostAt) {
      _lostAt = stamp();
      _rejoinAt = _lostAt;
      LOG("Mất mạng, chờ nối lại");
    } else if (since(_lostAt) >= _connectTimeout) {
      LOG("Mất mạng quá lâu");
      startElection();
    } else if (since(_rejoinAt) >= REJOIN_INTERVAL) {
      _rejoinAt = stamp();  // Wi-Fi tự nối lại chưa được: vào lại từ đầu
      WiFi.begin(_ssid.c_str(), passOrNull(_pass));
    }
    return;
  }
  _lostAt = 0;
  if (_reportPending && since(_reportTimer) >= REPORT_INTERVAL) {
    _reportTimer = millis();
    reportDone();
  }
}

void WiFiShare::loopElecting(bool scanned) {
  if (scanned) {
    if (!_scanOk) {
      _wait = since(_stateAt) + 2000;  // quét lỗi, thử lại
      return;
    }
    const Net* home = findNet(_ssid);
    _homeSeenBefore = home != nullptr;
    if (_setupFound) {
      joinMaster();
    } else if (_canMaster && _apPass.length() >= 8) {
      // Phát cùng kênh với mạng nhà (nếu thấy): khi mạng nhà quay lại, vào lại không phải đổi kênh
      startMaster(home ? home->channel : 1);
    } else if (_ssid.length()) {
      connectHome(HOME_RETRY_TIMEOUT);
    } else {
      _wait = since(_stateAt) + 10000;  // chờ mạch khác làm mạch chủ
    }
    return;
  }
  if (!_scanning && since(_stateAt) >= _wait) {
    startScan();
    if (!_scanning) _wait = since(_stateAt) + 2000;  // không bắt đầu quét được, thử lại sau
  }
}

bool WiFiShare::onSetupAp() const {
  if (WiFi.status() != WL_CONNECTED) return false;
  const uint8_t* bssid = WiFi.BSSID();
  return bssid && memcmp(bssid, _setupBssid, 6) == 0;
}

void WiFiShare::loopJoining() {
  if (onSetupAp()) {
    _joinFails = 0;
    _pollTimer = millis() - POLL_INTERVAL;  // hỏi mạch chủ ngay
    _scanTimer = millis();
    _masterLostAt = 0;
    _masterBusyAt = 0;
    enter(WiFiShareState::Slave);
  } else if (since(_stateAt) >= JOIN_TIMEOUT) {
    LOG("Không vào được Wi-Fi cài đặt, kiểm tra mật khẩu nhóm");
    noteJoinFailure();
    startElection();
  }
}

void WiFiShare::loopSlave(bool scanned) {
  bool busy = _masterBusyAt && since(_masterBusyAt) < BUSY_MEMORY;
  if (WiFi.status() != WL_CONNECTED) {
    if (!_masterLostAt) _masterLostAt = stamp();
    if (since(_masterLostAt) >= (busy ? LOST_MASTER_BUSY : LOST_MASTER)) {
      LOG("Mất mạch chủ");
      connectHome(HOME_RETRY_TIMEOUT);  // thường là mạch chủ đã về mạng nhà
    }
    return;
  }
  _masterLostAt = 0;

  if (scanned && _scanOk) {
    // Mạch con tự thử về mạng đã lưu, không chờ mạch chủ (mạng đã lưu của hai mạch có thể khác nhau)
    const Net* home = findNet(_ssid);
    if (!home) _homeSeenBefore = false;
    bool fresh = home && !_homeSeenBefore;
    uint32_t idle = since(_homeTimer);
    if (_ssid.length() && !busy && ((fresh && idle >= RETRY_MIN) || idle >= _homeBackoff)) {
      _homeSeenBefore = home != nullptr;
      if (home) LOG("Thấy mạng %s, thử vào lại", _ssid.c_str());
      else LOG("Thử vào lại mạng %s", _ssid.c_str());
      connectHome(HOME_RETRY_TIMEOUT);
      return;
    }
  }
  if (!_scanning && since(_pollTimer) >= POLL_INTERVAL) {  // đang quét thì chưa vào mạng mới được
    _pollTimer = millis();
    pollMaster();
    if (_state != WiFiShareState::Slave) return;
  }
  if (!_scanning && since(_scanTimer) >= SLAVE_SCAN_INTERVAL) {
    _scanTimer = millis();
    startScan();
  }
}

void WiFiShare::loopMaster(bool scanned) {
  if (_phase == Phase::Verifying || _phase == Phase::TryHome) {
    loopConnectPhase();
    return;
  }
  if (_phase == Phase::Sending) {
    loopSending();
    return;
  }

  bool quiet = !_portalAt || since(_portalAt) >= PORTAL_QUIET;
  if (scanned && _scanOk) {
    // Hai mạch cùng làm mạch chủ: mạch có BSSID lớn hơn nhường
    if (_setupFound && lessMac(_setupBssid, _apBssid) && quiet) {
      LOG("Có mạch chủ khác, chuyển sang làm mạch con");
      joinMaster();
      return;
    }
    // Mạng đã lưu vừa hiện lại (vd. router vừa bật lại) thì thử sớm. Còn lại thử thưa dần, kể cả
    // khi quét không thấy, vì mạng ẩn hoặc quá nhiều mạng xung quanh thì quét có thể không ra
    const Net* home = findNet(_ssid);
    if (!home) _homeSeenBefore = false;
    bool fresh = home && !_homeSeenBefore;
    uint32_t idle = since(_homeTimer);
    if (_ssid.length() && quiet && ((fresh && idle >= RETRY_MIN) || idle >= _homeBackoff)) {
      _homeSeenBefore = home != nullptr;
      if (home) LOG("Thấy mạng %s, thử vào lại", _ssid.c_str());
      else LOG("Thử vào lại mạng %s", _ssid.c_str());
      startPhase(Phase::TryHome, _ssid, _pass);
      return;
    }
  }
  if (!_scanning && since(_scanTimer) >= _scanWait) {
    _scanTimer = millis();
    _scanWait = MASTER_SCAN_INTERVAL;
    startScan();
  }
}

// ================== Mạch chủ ==================

// Verifying: thử mạng người dùng nhập. TryHome: thử vào lại mạng đã lưu. Vào được thì gửi mạng đó
// cho các mạch con. Cả hai làm mạch chủ đổi kênh nên trước đó báo "bận" cho các mạch con vài giây.
void WiFiShare::startPhase(Phase phase, const String& ssid, const String& pass) {
  _phase = phase;
  _phaseAt = millis();
  _phaseBegun = false;
  _newSsid = ssid;
  _newPass = pass;
  if (phase == Phase::TryHome) {
    _homeTimer = millis();
    _message = String(F("Đang thử vào lại mạng ")) + ssid + F("…");
  } else {
    _message = String(F("Đang thử vào mạng ")) + ssid + F("…");
  }
}

void WiFiShare::loopConnectPhase() {
  if (!_phaseBegun) {
    if (since(_phaseAt) < PHASE_DELAY || _scanning) return;
    const Net* net = findNet(_newSsid);
    WiFi.setAutoReconnect(true);
    if (net) WiFi.begin(_newSsid.c_str(), passOrNull(_newPass), net->channel, net->bssid);
    else WiFi.begin(_newSsid.c_str(), passOrNull(_newPass));  // mạng ẩn hoặc chưa quét thấy
    _phaseBegun = true;
    _phaseAt = millis();
    return;
  }

  wl_status_t status = WiFi.status();
  uint32_t t = since(_phaseAt);
  if (status == WL_CONNECTED) {
    startSending();
    return;
  }

  bool wrongPass = false;
#if defined(ESP8266)
  wrongPass = status == WL_WRONG_PASSWORD;
#endif
  if (t < VERIFY_TIMEOUT && !(wrongPass && t > 5000)) return;

  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  if (_phase == Phase::TryHome) {
    if (_homeBackoff < RETRY_MAX) _homeBackoff *= 2;
    _message = String(F("Các mạch không vào được mạng ")) + _ssid + '.';
    LOG("Chưa vào lại được mạng %s, thử lại sau %u phút", _ssid.c_str(), (unsigned)(_homeBackoff / 60000));
  } else {
    if (status == WL_NO_SSID_AVAIL) _message = String(F("Không thấy mạng ")) + _newSsid + '.';
    else if (wrongPass) _message = String(F("Sai mật khẩu mạng ")) + _newSsid + '.';
    else _message = String(F("Không vào được mạng ")) + _newSsid + F(". Kiểm tra lại mật khẩu.");
    LOG("%s", _message.c_str());
  }
  _phase = Phase::Idle;
}

// Mạch chủ đã vào được mạng: giữ Wi-Fi cài đặt để gửi mạng cho các mạch con. Mạch con nhận xong
// thì rời Wi-Fi cài đặt, tự vào mạng rồi báo về mạch chủ qua mạng đó (/_g/done).
void WiFiShare::startSending() {
  LOG("Vào được mạng %s, gửi cho các mạch con", _newSsid.c_str());
  for (uint8_t i = 0; i < _peerCount; i++) {
    _peers[i].sends = 0;
    _peers[i].sentAt = 0;
    _peers[i].done = false;
  }
  _phase = Phase::Sending;
  _phaseAt = millis();
  _staLostAt = 0;
}

void WiFiShare::loopSending() {
  // Mạch chủ rớt mạng: dừng gửi (mạch con được báo BUSY), lâu quá thì thôi
  if (WiFi.status() != WL_CONNECTED) {
    if (!_staLostAt) {
      _staLostAt = stamp();
    } else if (since(_staLostAt) >= VERIFY_TIMEOUT) {
      _message = String(F("Mạch chủ bị rớt khỏi mạng ")) + _newSsid + F(". Hãy lưu lại.");
      LOG("%s", _message.c_str());
      WiFi.setAutoReconnect(false);
      WiFi.disconnect();
      _phase = Phase::Idle;
    }
    return;
  }
  _staLostAt = 0;

  uint8_t pending = 0, done = 0, failed = 0;
  for (uint8_t i = 0; i < _peerCount; i++) {
    const Peer& p = _peers[i];
    if (p.done) done++;
    else if (p.sends == 0) pending += since(p.seen) < PEER_WINDOW;  // chưa nhận mạng, vẫn đang hỏi
    else if (since(p.sentAt) < DONE_WAIT) pending++;                 // đã nhận, đang vào mạng
    else failed++;
  }
  uint32_t t = since(_phaseAt);
  if (!((t >= SEND_MIN && pending == 0) || t >= SEND_MAX)) return;

  LOG("%u mạch con đã vào mạng, %u mạch chưa báo", (unsigned)done, (unsigned)(pending + failed));
  _ssid = _newSsid;
  _pass = _newPass;
  saveNetwork();
  connectHome(_connectTimeout);  // tắt Wi-Fi cài đặt; nối lại không khóa vào một điểm phát
}

void WiFiShare::setupRoutes() {
  _web->on("/", HTTP_GET, [this]() {
    if (_web->hostHeader() != WiFi.softAPIP().toString()) {
      handleNotFound();  // về đúng địa chỉ mạch chủ, để các yêu cầu sau qua được hostOk()
      return;
    }
    _portalAt = stamp();
#if defined(ESP8266)
    _web->send_P(200, "text/html; charset=utf-8", WIFISHARE_PAGE);
#else
    _web->send(200, "text/html; charset=utf-8", WIFISHARE_PAGE);
#endif
  });
  _web->on("/status", HTTP_GET, [this]() { handleStatus(); });
  _web->on("/save", HTTP_POST, [this]() { handleSave(); });
  _web->on("/_g/poll", HTTP_GET, [this]() { handlePoll(); });
  _web->on("/_g/done", HTTP_GET, [this]() { handleDone(); });
  _web->onNotFound([this]() { handleNotFound(); });
}

String WiFiShare::statusText() const {
  if (_phase != Phase::Sending) return _message;
  uint8_t total = 0, done = 0;
  for (uint8_t i = 0; i < _peerCount; i++) {
    const Peer& p = _peers[i];
    if (p.done) done++;
    if (p.done || p.sends || since(p.seen) < PEER_WINDOW) total++;
  }
  String s = String(F("Đã vào được mạng ")) + _newSsid + '.';
  if (total) s += String(F(" Mạch con đã vào mạng: ")) + done + '/' + total + '.';
  return s;
}

void WiFiShare::handleStatus() {
  if (_web->arg("v") == "1") _portalAt = stamp();  // trang đang mở trên màn hình
  const __FlashStringHelper* sub;
  switch (_phase) {
    case Phase::Verifying:
    case Phase::TryHome:
      sub = F("Điện thoại có thể bị ngắt khỏi Wi-Fi cài đặt vài giây. Nếu bị ngắt, hãy vào lại rồi mở 192.168.4.1.");
      break;
    case Phase::Sending:
      sub = F("Mỗi mạch con tự vào mạng rồi báo lại. Mọi mạch đã báo thì Wi-Fi cài đặt tắt.");
      break;
    default:
      sub = F("Chọn mạng, nhập mật khẩu rồi bấm Lưu. Mạch chủ thử mạng trước rồi mới gửi cho các mạch khác.");
  }

  String j;
  j.reserve(768);
  j += "{\"name\":";
  addJson(j, _name);
  j += ",\"msg\":";
  addJson(j, statusText());
  j += ",\"sub\":";
  addJson(j, String(sub));
  j += ",\"busy\":";
  j += _phase == Phase::Idle ? "false" : "true";
  bool sending = _phase == Phase::Sending;
  j += ",\"boards\":[{\"n\":";
  addJson(j, _name + " (mạch chủ)");
  j += sending ? ",\"t\":\"đã vào mạng\",\"ok\":true}" : ",\"t\":\"\",\"ok\":false}";
  for (uint8_t i = 0; i < _peerCount; i++) {
    const Peer& p = _peers[i];
    if (!p.done && !p.sends && since(p.seen) >= PEER_WINDOW) continue;
    const __FlashStringHelper* t = F("");
    if (sending) {
      if (p.done) t = F("đã vào mạng");
      else if (p.sends >= MAX_SENDS) t = F("không vào được mạng");
      else if (p.sends > 1) t = F("chưa vào được, gửi lại");
      else if (p.sends) t = F("đang vào mạng…");
    }
    j += ",{\"n\":";
    addJson(j, p.name);
    j += ",\"t\":";
    addJson(j, String(t));
    j += ",\"ok\":";
    j += p.done ? "true" : "false";
    j += '}';
  }
  j += "],\"nets\":[";
  for (uint8_t i = 0; i < _netCount; i++) {
    if (i) j += ',';
    j += "{\"s\":";
    addJson(j, _nets[i].ssid);
    j += ",\"r\":";
    j += String(_nets[i].rssi);
    j += ",\"o\":";
    j += _nets[i].open ? "true" : "false";
    j += '}';
  }
  j += "]}";
  _web->sendHeader("Cache-Control", "no-store");
  _web->send(200, "application/json", j);
}

// Chỉ nhận yêu cầu gửi thẳng tới địa chỉ mạch chủ: chặn trang web lạ dùng trình duyệt của điện thoại
// để gửi lệnh (DNS rebinding)
bool WiFiShare::hostOk() {
  if (_web->hostHeader() == WiFi.softAPIP().toString()) return true;
  _web->send(403, "text/plain", "");
  return false;
}

void WiFiShare::handleSave() {
  if (!hostOk()) return;
  _portalAt = stamp();
  const char* text = "text/plain; charset=utf-8";
  if (_phase != Phase::Idle) {
    _web->send(409, text, String(F("Mạch chủ đang bận, chờ một chút rồi lưu lại.")));
    return;
  }
  String ssid = _web->arg("ssid"), pass = _web->arg("pass");
  if (ssid.isEmpty() || ssid.length() > 32) {
    _web->send(400, text, String(F("Tên Wi-Fi phải từ 1 đến 32 ký tự.")));
    return;
  }
  if (pass.length() && (pass.length() < 8 || pass.length() > 64)) {
    _web->send(400, text, String(F("Mật khẩu Wi-Fi phải từ 8 đến 63 ký tự.")));
    return;
  }
  startPhase(Phase::Verifying, ssid, pass);
  _web->send(200, text, "OK");
}

// Mạch con hỏi 3 giây một lần. Trả lời: WAIT (chờ), HOLD (người dùng đang mở trang cài đặt, đừng
// rời đi), BUSY (mạch chủ sắp đổi kênh), NET <tên> <mật khẩu> <IP> (tên, mật khẩu dạng hex; IP là
// địa chỉ của mạch chủ trong mạng đó, để mạch con báo về khi đã vào mạng).
void WiFiShare::handlePoll() {
  if (!hostOk()) return;
  Peer* p = findPeer(_web->arg("id"), true);
  if (p) {
    p->seen = millis();
    String name;
    if (fromHex(_web->arg("n"), name)) strlcpy(p->name, name.c_str(), sizeof p->name);
  }
  bool portalOpen = _portalAt && since(_portalAt) < PORTAL_QUIET;
  if (_phase == Phase::Sending) {
    if (WiFi.status() != WL_CONNECTED) {  // mạch chủ đang rớt mạng, chưa có địa chỉ để mạch con báo về
      _web->send(200, "text/plain", "BUSY");
      return;
    }
    // Mạch con hỏi lại ngay sau khi được gửi: trả lời trước không tới nơi (vd. mạch chủ bận quá lâu).
    // Hỏi lại muộn hơn: nó đã thử mà không vào được mạng.
    bool lostReply = p && p->sends && since(p->sentAt) < LOST_REPLY;
    if (!p || (p->sends >= MAX_SENDS && !lostReply)) {
      _web->send(200, "text/plain", "WAIT");
      return;
    }
    if (!lostReply) p->sends++;
    p->sentAt = stamp();
    p->done = false;
    _web->send(200, "text/plain",
               "NET " + toHex(_newSsid) + " " + toHex(_newPass) + " " + WiFi.localIP().toString());
  } else if (_phase != Phase::Idle) {
    _web->send(200, "text/plain", "BUSY");
  } else {
    _web->send(200, "text/plain", portalOpen ? "HOLD" : "WAIT");
  }
}

// Mạch con báo đã vào được mạng. Yêu cầu đến qua mạng nhà, gửi thẳng tới địa chỉ của mạch chủ trong
// mạng đó.
void WiFiShare::handleDone() {
  if (_web->hostHeader() != WiFi.localIP().toString()) {
    _web->send(403, "text/plain", "");
    return;
  }
  Peer* p = findPeer(_web->arg("id"), false);
  if (p && _phase == Phase::Sending) {
    p->done = true;
    LOG("Mạch %s đã vào mạng", p->name);
  }
  _web->send(200, "text/plain", "OK");
}

// Điện thoại kiểm tra mạng bằng các địa chỉ như connectivitycheck.gstatic.com: chuyển hết về trang cài đặt
void WiFiShare::handleNotFound() {
  if (_web->uri().startsWith("/_g/")) {
    _web->send(404, "text/plain", "");
    return;
  }
  _web->sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  _web->send(302, "text/plain", "");
}

WiFiShare::Peer* WiFiShare::findPeer(const String& id, bool create) {
  if (id.length() != 6) return nullptr;
  for (uint8_t i = 0; i < _peerCount; i++) {
    if (id == _peers[i].id) return &_peers[i];
  }
  if (!create) return nullptr;
  uint8_t slot = _peerCount;
  if (slot == MAX_PEERS) {  // đầy: dùng chỗ của mạch hỏi lâu nhất
    slot = 0;
    for (uint8_t i = 1; i < MAX_PEERS; i++) {
      if (since(_peers[i].seen) > since(_peers[slot].seen)) slot = i;
    }
  } else {
    _peerCount++;
  }
  Peer& p = _peers[slot];
  strlcpy(p.id, id.c_str(), sizeof p.id);
  strlcpy(p.name, p.id, sizeof p.name);
  p.seen = millis();
  p.sentAt = 0;
  p.sends = 0;
  p.done = false;
  return &p;
}

// ================== Mạch con ==================

void WiFiShare::noteJoinFailure() {
  if (_joinFails && memcmp(_joinFailBssid, _setupBssid, 6) == 0) {
    _joinFails++;
  } else {
    memcpy(_joinFailBssid, _setupBssid, 6);
    _joinFails = 1;
  }
  if (_joinFails >= 3) {
    LOG("Vào 3 lần không được, bỏ qua Wi-Fi cài đặt này 30 phút (có thể là của nhà khác đặt trùng tên)");
    memcpy(_badBssid, _setupBssid, 6);
    _hasBad = true;
    _badAt = millis();
    _joinFails = 0;
  }
}

void WiFiShare::pollMaster() {
  String reply = callBoard(WiFi.gatewayIP(), "/_g/poll?id=" + String(_id) + "&n=" + toHex(_name));
  if (reply == "BUSY" || reply == "HOLD") {
    _masterBusyAt = stamp();  // ở lại chờ, không tự đi thử mạng cũ
    return;
  }
  if (!reply.startsWith("NET ")) return;
  int a = reply.indexOf(' ', 4);
  int b = a < 0 ? -1 : reply.indexOf(' ', a + 1);
  String ssid, pass;
  IPAddress ip;
  if (b < 0 || !fromHex(reply.substring(4, a), ssid) || !fromHex(reply.substring(a + 1, b), pass) ||
      ssid.isEmpty() || !ip.fromString(reply.substring(b + 1)))
    return;

  // Mạch chủ đã vào được mạng này. Rời Wi-Fi cài đặt, tự vào mạng rồi báo về mạch chủ qua mạng đó.
  LOG("Mạch chủ gửi mạng mới: %s", ssid.c_str());
  _ssid = ssid;
  _pass = pass;
  saveNetwork();
  _reportIp = ip;
  _reportPending = true;
  _homeBackoff = RETRY_MIN;
  connectHome(_connectTimeout);
}

void WiFiShare::reportDone() {
  if (callBoard(_reportIp, "/_g/done?id=" + String(_id)) == "OK") {
    LOG("Đã báo mạch chủ");
    _reportPending = false;
  } else if (++_reportTries >= REPORT_TRIES) {
    LOG("Không báo được mạch chủ, mạch chủ sẽ tự tắt Wi-Fi cài đặt sau vài phút");
    _reportPending = false;
  }
}

String WiFiShare::callBoard(const IPAddress& ip, const String& pathAndQuery) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, "http://" + ip.toString() + pathAndQuery)) return "";
  http.setTimeout(HTTP_TIMEOUT);
#if defined(ESP32)
  http.setConnectTimeout(HTTP_TIMEOUT);
#endif
  int code = http.GET();
  String body = code == 200 ? http.getString() : String();
  http.end();
  return body;
}

// ================== Quét, lưu mạng ==================

void WiFiShare::startScan() {
  if (_scanning) return;
  int r = WiFi.scanNetworks(true, false);
  if (r == WIFI_SCAN_RUNNING || r >= 0) {
    _scanning = true;
    _scanAt = millis();
    _scanOwner = _state;
  }
}

// true khi vừa quét xong một lượt do trạng thái hiện tại yêu cầu (_scanOk cho biết quét có lỗi không)
bool WiFiShare::scanFinished() {
  if (!_scanning) return false;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING && since(_scanAt) < SCAN_TIMEOUT) return false;
  _scanning = false;
  _scanOk = n >= 0;
  if (_scanOk) readScan(n);
  else WiFi.scanDelete();
  return _scanOwner == _state;
}

void WiFiShare::readScan(int count) {
  _netCount = 0;
  _setupFound = false;
  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    const uint8_t* bssid = WiFi.BSSID(i);
    int32_t rssi = WiFi.RSSI(i);
    uint8_t channel = (uint8_t)WiFi.channel(i);

    if (ssid == _apSsid) {  // Wi-Fi cài đặt: giữ mạch chủ có BSSID nhỏ nhất, không hiện cho người dùng
      if (_hasBad && since(_badAt) < BAD_MEMORY && memcmp(bssid, _badBssid, 6) == 0) continue;
      if (!_setupFound || lessMac(bssid, _setupBssid)) {
        _setupFound = true;
        _setupChannel = channel;
        memcpy(_setupBssid, bssid, 6);
      }
      continue;
    }

    uint8_t k = 0;
    while (k < _netCount && _nets[k].ssid != ssid) k++;
    if (k == _netCount) {
      if (_netCount < MAX_NETS) {
        _netCount++;
      } else {  // đầy: thay mạng yếu nhất nếu mạng này mạnh hơn. Không bao giờ bỏ mạng đã lưu.
        k = _nets[0].ssid == _ssid ? 1 : 0;
        for (uint8_t m = k + 1; m < MAX_NETS; m++) {
          if (_nets[m].ssid != _ssid && _nets[m].rssi < _nets[k].rssi) k = m;
        }
        if (ssid != _ssid && _nets[k].rssi >= rssi) continue;
      }
      _nets[k].ssid = ssid;
      _nets[k].rssi = -1000;
    }
    if (rssi > _nets[k].rssi) {
      _nets[k].rssi = rssi;
      _nets[k].channel = channel;
      memcpy(_nets[k].bssid, bssid, 6);
      _nets[k].open = isOpen(i);
    }
  }
  WiFi.scanDelete();

  // Sắp theo độ mạnh để trang cài đặt hiện mạng mạnh trước
  for (uint8_t i = 1; i < _netCount; i++) {
    for (uint8_t k = i; k > 0 && _nets[k].rssi > _nets[k - 1].rssi; k--) std::swap(_nets[k], _nets[k - 1]);
  }
}

const WiFiShare::Net* WiFiShare::findNet(const String& ssid) const {
  if (ssid.isEmpty()) return nullptr;
  for (uint8_t i = 0; i < _netCount; i++) {
    if (_nets[i].ssid == ssid) return &_nets[i];
  }
  return nullptr;
}

bool WiFiShare::loadNetwork() {
#if defined(ESP8266)
  _fsOk = LittleFS.begin();
  if (!_fsOk) {
    LOG("Không mở được LittleFS, mạng mới sẽ không được lưu (chọn Flash Size có FS)");
    return false;
  }
  File f = LittleFS.open(FILE_PATH, "r");
  if (!f) return false;
  String a = f.readStringUntil('\n'), b = f.readStringUntil('\n');
  f.close();
  String ssid, pass;
  if (!fromHex(a, ssid) || !fromHex(b, pass) || ssid.isEmpty()) return false;
#else
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  String ssid = prefs.isKey("ssid") ? prefs.getString("ssid") : String();
  String pass = prefs.isKey("pass") ? prefs.getString("pass") : String();
  prefs.end();
  if (ssid.isEmpty()) return false;
#endif
  _ssid = ssid;
  _pass = pass;
  LOG("Mạng đã lưu: %s", _ssid.c_str());
  return true;
}

void WiFiShare::saveNetwork() {
#if defined(ESP8266)
  if (!_fsOk) return;
  File f = LittleFS.open(FILE_PATH, "w");
  if (!f) {
    LOG("Không ghi được file lưu mạng");
    return;
  }
  f.print(toHex(_ssid));
  f.print('\n');
  f.print(toHex(_pass));
  f.print('\n');
  f.close();
#else
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    LOG("Không ghi được NVS");
    return;
  }
  prefs.putString("ssid", _ssid);
  prefs.putString("pass", _pass);
  prefs.end();
#endif
  LOG("Đã lưu mạng %s", _ssid.c_str());
}

void WiFiShare::logP(PGM_P format, ...) {
  if (!_logging) return;
  char buf[192];
  va_list args;
  va_start(args, format);
  vsnprintf_P(buf, sizeof buf, format, args);
  va_end(args);
  Serial.print(F("[WiFiShare] "));
  Serial.println(buf);
}
