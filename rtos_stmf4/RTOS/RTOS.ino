#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

// ===== CẤU HÌNH =====
const char* ssid        = "E=m.c^2";
const char* password    = "00010000";
const char* mqtt_server = "thingsboard.cloud";
const char* token       = "A4HQRh101VL4YBpK0kU8";
const int   mqtt_port   = 1883;

// ===== UART =====
#define RX_PIN 16
#define TX_PIN 17
#define UART_BAUD 115200

WiFiClient   espClient;
PubSubClient mqtt(espClient);
WebServer    server(80);

char uartBuf[256];
int  uartLen = 0;

// ===== TRẠNG THÁI HỆ THỐNG =====
struct {
    String lastEvent    = "—";
    String lastMethod   = "—";
    String lastUID      = "—";
    int    cardCount    = 0;
    int    totalGranted = 0;
    int    totalDenied  = 0;
    String lastTime     = "—";
} state;

// ===== LOG SỰ KIỆN (lưu 10 dòng gần nhất) =====
#define MAX_LOG 10
String eventLog[MAX_LOG];
int    logCount = 0;

void pushLog(String msg) {
    if (logCount < MAX_LOG) {
        eventLog[logCount++] = msg;
    } else {
        for (int i = 0; i < MAX_LOG - 1; i++)
            eventLog[i] = eventLog[i + 1];
        eventLog[MAX_LOG - 1] = msg;
    }
}

// ===== TRANG WEB =====
void handleRoot() {
    String logRows = "";
    for (int i = logCount - 1; i >= 0; i--) {
        logRows += "<tr><td>" + eventLog[i] + "</td></tr>";
    }

    String html = R"(
<!DOCTYPE html>
<html lang='vi'>
<head>
<meta charset='UTF-8'>
<meta http-equiv='refresh' content='3'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Khóa Cửa RFID</title>
<style>
  body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; margin: 0; padding: 20px; }
  h1 { color: #00d4ff; text-align: center; }
  .card { background: #16213e; border-radius: 10px; padding: 15px; margin: 10px 0; }
  .row { display: flex; justify-content: space-between; flex-wrap: wrap; gap: 10px; }
  .stat { background: #0f3460; border-radius: 8px; padding: 15px; flex: 1; text-align: center; min-width: 120px; }
  .stat .val { font-size: 2em; font-weight: bold; color: #00d4ff; }
  .granted { color: #00ff88; }
  .denied  { color: #ff4444; }
  table { width: 100%; border-collapse: collapse; }
  td { padding: 8px; border-bottom: 1px solid #333; font-size: 0.9em; }
  .badge { padding: 3px 8px; border-radius: 4px; font-size: 0.8em; }
  .bg-green { background: #00ff8833; color: #00ff88; }
  .bg-red   { background: #ff444433; color: #ff4444; }
  .bg-blue  { background: #00d4ff33; color: #00d4ff; }
</style>
</head>
<body>
<h1>🔐 Hệ Thống Khóa Cửa RFID</h1>

<div class='card'>
  <div class='row'>
    <div class='stat'>
      <div class='val'>)" + String(state.cardCount) + R"(</div>
      <div>Số thẻ</div>
    </div>
    <div class='stat'>
      <div class='val granted'>)" + String(state.totalGranted) + R"(</div>
      <div>Mở thành công</div>
    </div>
    <div class='stat'>
      <div class='val denied'>)" + String(state.totalDenied) + R"(</div>
      <div>Từ chối</div>
    </div>
  </div>
</div>

<div class='card'>
  <b>Sự kiện gần nhất:</b><br><br>
  Loại: <span class='badge bg-blue'>)" + state.lastEvent + R"(</span> &nbsp;
  Phương thức: <b>)" + state.lastMethod + R"(</b> &nbsp;
  UID: <code>)" + state.lastUID + R"(</code>
</div>

<div class='card'>
  <b>Lịch sử sự kiện (10 gần nhất):</b>
  <table>)" + logRows + R"(</table>
</div>

<p style='text-align:center; color:#555; font-size:0.8em'>Tự động làm mới sau 3 giây &nbsp;|&nbsp; IP: )" + WiFi.localIP().toString() + R"(</p>
</body>
</html>
)";

    server.send(200, "text/html; charset=utf-8", html);
}

// ===== KẾT NỐI WIFI =====
void connectWiFi() {
    Serial.print("Đang kết nối WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi đã kết nối: " + WiFi.localIP().toString());
}

// ===== KẾT NỐI MQTT =====
void connectMQTT() {
    while (!mqtt.connected()) {
        Serial.print("Đang kết nối ThingsBoard...");
        if (mqtt.connect("ESP32_DoorLock", token, NULL)) {
            Serial.println("Thành công!");
        } else {
            Serial.printf("Thất bại, mã lỗi: %d. Thử lại sau 3s\n", mqtt.state());
            delay(3000);
        }
    }
}

// ===== GỬI TELEMETRY =====
void sendTelemetry(const char* jsonStr) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.println("JSON lỗi: " + String(err.c_str()));
        return;
    }

    const char* event = doc["event"];
    StaticJsonDocument<256> tb;
    String logMsg = "";

    if (strcmp(event, "ACCESS_GRANTED") == 0) {
        tb["access_granted"] = 1;
        tb["access_denied"]  = 0;
        tb["method"]         = doc["method"];
        if (doc.containsKey("uid")) tb["uid"] = doc["uid"];

        state.lastEvent  = "MỞ CỬA";
        state.lastMethod = doc["method"] | "—";
        state.lastUID    = doc.containsKey("uid") ? (const char*)doc["uid"] : "—";
        state.totalGranted++;
        logMsg = "✅ Mở cửa | " + state.lastMethod + " | UID: " + state.lastUID;

    } else if (strcmp(event, "ACCESS_DENIED") == 0) {
        tb["access_granted"] = 0;
        tb["access_denied"]  = 1;
        tb["method"]         = doc["method"];
        if (doc.containsKey("uid")) tb["uid"] = doc["uid"];

        state.lastEvent  = "TỪ CHỐI";
        state.lastMethod = doc["method"] | "—";
        state.lastUID    = doc.containsKey("uid") ? (const char*)doc["uid"] : "—";
        state.totalDenied++;
        logMsg = "❌ Từ chối | " + state.lastMethod + " | UID: " + state.lastUID;

    } else if (strcmp(event, "CARD_ADDED") == 0) {
        tb["card_added"]  = 1;
        tb["uid"]         = doc["uid"];
        tb["card_count"]  = doc["card_count"];

        state.lastEvent  = "THÊM THẺ";
        state.lastUID    = doc["uid"] | "—";
        state.cardCount  = doc["card_count"];
        logMsg = "➕ Thêm thẻ | UID: " + state.lastUID + " | Tổng: " + String(state.cardCount);

    } else if (strcmp(event, "CARD_DELETED") == 0) {
        tb["card_deleted"] = 1;
        tb["uid"]          = doc["uid"];
        tb["card_count"]   = doc["card_count"];

        state.lastEvent  = "XÓA THẺ";
        state.lastUID    = doc["uid"] | "—";
        state.cardCount  = doc["card_count"];
        logMsg = "➖ Xóa thẻ | UID: " + state.lastUID + " | Tổng: " + String(state.cardCount);

    } else if (strcmp(event, "PASSWORD_CHANGED") == 0) {
        tb["password_changed"] = 1;

        state.lastEvent  = "ĐỔI MẬT KHẨU";
        state.lastMethod = "—";
        state.lastUID    = "—";
        logMsg = "🔑 Đổi mật khẩu";
    }

    // Gửi ThingsBoard
    char payload[256];
    serializeJson(tb, payload);
    Serial.println("Gửi ThingsBoard: " + String(payload));
    mqtt.publish("v1/devices/me/telemetry", payload);

    // Lưu log
    if (logMsg != "") pushLog(logMsg);
}

// ===== ĐỌC UART =====
void handleUART() {
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            uartBuf[uartLen] = '\0';
            if (uartLen > 0) {
                Serial.println("Nhận từ STM32: " + String(uartBuf));
                sendTelemetry(uartBuf);
            }
            uartLen = 0;
        } else {
            if (uartLen < 255) uartBuf[uartLen++] = c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

    connectWiFi();

    mqtt.setServer(mqtt_server, mqtt_port);
    connectMQTT();

    server.on("/", handleRoot);
    server.begin();
    Serial.println("Web server chạy tại: http://" + WiFi.localIP().toString());
}

void loop() {
    if (!mqtt.connected()) connectMQTT();
    mqtt.loop();
    handleUART();
    server.handleClient();
}