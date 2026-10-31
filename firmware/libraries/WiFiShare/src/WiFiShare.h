// WiFiShare: các mạch ESP8266/ESP32 trong nhà dùng chung một mạng Wi-Fi và tự chuyển mạng cùng nhau.
//
// Mạch nào không vào được mạng đã lưu thì đi tìm mạch chủ. Chưa có mạch chủ thì nó tự làm: phát
// Wi-Fi cài đặt (tên và mật khẩu đặt trong begin(), mọi mạch dùng chung). Mạch chủ chỉ phát khi chính
// nó không vào được mạng; các mạch cũng không vào được mạng thì vào Wi-Fi cài đặt làm mạch con và chờ.
//
// Người dùng vào Wi-Fi cài đặt bằng điện thoại, trang 192.168.4.1 tự mở, chọn mạng mới và nhập mật
// khẩu. Mạch chủ thử vào mạng mới trước, vào được thì gửi cho các mạch con. Mỗi mạch con lưu mạng,
// rời Wi-Fi cài đặt, tự vào mạng mới rồi báo về mạch chủ qua mạng mới, không quay lại Wi-Fi cài đặt
// nữa. Mọi mạch con đã báo thì mạch chủ tắt Wi-Fi cài đặt. Mạng lưu trong ESP8266: LittleFS, ESP32: NVS.
#pragma once

#include <Arduino.h>
#include <functional>
#include <memory>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#else
#error "WiFiShare chỉ chạy trên ESP8266 hoặc ESP32"
#endif
#include <DNSServer.h>

enum class WiFiShareState : uint8_t {
  Connecting,  // đang vào mạng đã lưu
  Online,      // đã vào mạng, chương trình chạy bình thường
  Electing,    // không vào được mạng, đang tìm mạch chủ
  Master,      // đang phát Wi-Fi cài đặt, chờ người dùng nhập mạng mới
  Joining,     // đang vào Wi-Fi cài đặt của mạch chủ
  Slave        // đã vào Wi-Fi cài đặt, chờ mạch chủ gửi mạng mới
};

class WiFiShare {
 public:
  // deviceName: tên mạch hiện trên trang cài đặt, vd. "cua_chinh".
  // setupSsid, setupPassword: Wi-Fi cài đặt mà mạch chủ phát. Mọi mạch trong nhà phải dùng cùng
  // giá trị. Mật khẩu từ 8 ký tự; ai biết mật khẩu này thì đổi được mạng của các mạch.
  void begin(const char* deviceName, const char* setupSsid, const char* setupPassword);

  // Gọi ở mỗi vòng loop(). Tránh delay() dài vì trang cài đặt và việc trao đổi giữa các mạch chạy ở đây.
  void loop();

  // true khi đã vào mạng nhà. Chỉ gọi Firebase/Internet khi online().
  bool online() const;
  WiFiShareState state() const { return _state; }
  const char* stateName() const;
  const String& ssid() const { return _ssid; }  // mạng đang lưu

  // Các hàm cài đặt dưới đây gọi trước begin().
  // Mạng dùng khi mạch chưa lưu mạng nào (lần nạp đầu). Có thể bỏ qua và nhập trên trang cài đặt.
  void setDefaultNetwork(const char* ssid, const char* password);
  // false: mạch này không bao giờ làm mạch chủ (vd. mạch đặt ở chỗ sóng yếu). Mặc định true.
  void setCanBeMaster(bool canBeMaster) { _canMaster = canBeMaster; }
  // Không vào được (hoặc mất) mạng quá thời gian này thì đi tìm mạch chủ. Mặc định 45 giây.
  void setConnectTimeout(uint32_t ms) { _connectTimeout = ms; }
  void setLogging(bool on) { _logging = on; }
  void onStateChange(std::function<void(WiFiShareState)> callback) { _onState = callback; }

  // Xóa mạng đã lưu rồi đi tìm mạch chủ.
  void forgetNetwork();

 private:
#if defined(ESP8266)
  using WebServerType = ESP8266WebServer;
#else
  using WebServerType = WebServer;
#endif
  enum class Phase : uint8_t { Idle, Verifying, Sending, TryHome };

  struct Net {  // một mạng thấy khi quét, giữ điểm phát mạnh nhất
    String ssid;
    int32_t rssi;
    uint8_t channel;
    uint8_t bssid[6];
    bool open;
  };
  struct Peer {  // mạch con đang hỏi mạch chủ
    char id[7];
    char name[24];
    uint32_t seen;    // lần cuối hỏi mạch chủ
    uint32_t sentAt;  // lần cuối mạch chủ gửi mạng cho nó
    uint8_t sends;    // số lần đã gửi; mạch con hỏi lại tức là chưa vào được mạng
    bool done;        // đã báo vào được mạng
  };
  static const uint8_t MAX_NETS = 16;
  static const uint8_t MAX_PEERS = 10;

  // Chuyển trạng thái
  void enter(WiFiShareState state);
  void connectHome(uint32_t timeout);
  void goOnline();
  void startElection();
  void startMaster(uint8_t channel);
  void joinMaster();
  void stopPortal();

  // Từng trạng thái
  void loopConnecting();
  void loopOnline();
  void loopElecting(bool scanned);
  void loopJoining();
  void loopSlave(bool scanned);
  void loopMaster(bool scanned);

  // Mạch chủ
  void startPhase(Phase phase, const String& ssid, const String& pass);
  void loopConnectPhase();
  void startSending();
  void loopSending();
  void setupRoutes();
  bool hostOk();
  void handleStatus();
  void handleSave();
  void handlePoll();
  void handleDone();
  void handleNotFound();
  Peer* findPeer(const String& id, bool create);
  String statusText() const;

  // Mạch con
  bool onSetupAp() const;  // đang nối đúng Wi-Fi cài đặt định vào
  void noteJoinFailure();
  void pollMaster();
  void reportDone();
  String callBoard(const IPAddress& ip, const String& pathAndQuery);

  // Quét, lưu mạng
  void startScan();
  bool scanFinished();
  void readScan(int count);
  const Net* findNet(const String& ssid) const;
  bool loadNetwork();
  void saveNetwork();
  void logP(PGM_P format, ...);  // dùng qua LOG() trong WiFiShare.cpp: chuỗi để trong flash

  bool _begun = false;
  bool _logging = true;
  bool _canMaster = true;
  uint32_t _connectTimeout = 45000;
  std::function<void(WiFiShareState)> _onState;

  String _name, _apSsid, _apPass;
  String _ssid, _pass;        // mạng đã lưu
  String _defSsid, _defPass;  // mạng mặc định trong code
  char _id[7] = "";           // 3 byte cuối MAC
  uint16_t _jitter = 0;       // lệch thời gian giữa các mạch, lấy từ MAC
  bool _fsOk = false;

  WiFiShareState _state = WiFiShareState::Connecting;
  uint32_t _stateAt = 0;
  uint32_t _timeout = 0;  // thời hạn của Connecting
  uint32_t _wait = 0;     // Electing: chờ bao lâu rồi mới quét

  // Thử vào lại mạng đã lưu (mạch chủ, mạch con): giãn dần từ 1 đến 16 phút
  uint32_t _homeTimer = 0;
  uint32_t _homeBackoff = 0;
  bool _homeSeenBefore = false;
  uint32_t _lostAt = 0;    // Online: lúc mất mạng
  uint32_t _rejoinAt = 0;  // Online: lần cuối tự vào lại mạng

  // Quét
  bool _scanning = false;
  bool _scanOk = false;
  WiFiShareState _scanOwner = WiFiShareState::Connecting;
  uint32_t _scanAt = 0;
  uint32_t _scanTimer = 0;
  uint32_t _scanWait = 0;
  Net _nets[MAX_NETS];
  uint8_t _netCount = 0;
  bool _setupFound = false;  // Wi-Fi cài đặt thấy khi quét (BSSID nhỏ nhất nếu có nhiều mạch chủ)
  uint8_t _setupChannel = 0;
  uint8_t _setupBssid[6] = {0};
  // Wi-Fi cài đặt vào 3 lần không được (thường là của nhà khác đặt trùng tên): bỏ qua khi quét
  uint8_t _joinFailBssid[6] = {0};
  uint8_t _joinFails = 0;
  uint8_t _badBssid[6] = {0};
  bool _hasBad = false;
  uint32_t _badAt = 0;

  // Mạch con
  uint32_t _pollTimer = 0;
  uint32_t _masterLostAt = 0;
  uint32_t _masterBusyAt = 0;
  // Vào được mạng mạch chủ gửi thì báo về mạch chủ (địa chỉ của nó trong mạng đó)
  bool _reportPending = false;
  IPAddress _reportIp;
  uint8_t _reportTries = 0;
  uint32_t _reportTimer = 0;

  // Mạch chủ
  std::unique_ptr<WebServerType> _web;
  std::unique_ptr<DNSServer> _dns;
  uint8_t _apBssid[6] = {0};
  Phase _phase = Phase::Idle;
  uint32_t _phaseAt = 0;
  bool _phaseBegun = false;
  uint32_t _staLostAt = 0;    // Sending: lúc mạch chủ rớt mạng
  String _newSsid, _newPass;  // mạng đang thử / đang gửi
  String _message;            // dòng trạng thái trên trang cài đặt
  uint32_t _portalAt = 0;     // lần cuối người dùng mở trang cài đặt
  Peer _peers[MAX_PEERS];
  uint8_t _peerCount = 0;
};
