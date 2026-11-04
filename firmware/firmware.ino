#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <ESPmDNS.h>

// Stage base v0.1: RGB only. NO relay / mains outputs.
const char *VERSION = "0.1.1";
Preferences prefs;
WebServer web(80);
String ssid, wifiPass, adminPass, fleetKey, peers, role, hostname, apName;
int nodeId = 1, ledPin = 48;
uint8_t red = 16, green = 0, blue = 0;
bool blinking = true, apRunning = false, otaActive = false, otaOK = false;
uint32_t restartAt = 0, lastRetry = 0, disconnectedAt = 0;

String esc(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
  s.replace("\"", "&quot;"); s.replace("'", "&#39;"); return s;
}
// Bench mode: web UI and OTA intentionally require no login.
// The token below still protects browser forms against cross-origin POSTs.
bool auth() { return true; }
bool apiAuth() {
  if (web.header("X-Stage-Key") == fleetKey) return true;
  web.send(401, "text/plain", "Invalid key"); return false;
}
bool csrf() {
  if (web.arg("csrf") == adminPass) return true;
  web.send(403, "text/plain", "Invalid form token"); return false;
}
String token() { return "<input type='hidden' name='csrf' value='" + esc(adminPass) + "'>"; }
void paint(bool on) {
  rgbLedWrite(ledPin, on ? red : 0, on ? green : 0, on ? blue : 0);
}
String apPassword() { return "debug123"; }
void startAP() {
  if (apRunning) return;
  apRunning = WiFi.softAP(apName.c_str(), apPassword().c_str());
  Serial.printf("Setup AP: %s  password: %s  URL: http://192.168.4.1\n", apName.c_str(), apPassword().c_str());
}
String field(const char *label, const char *name, String value, const char *type = "text") {
  return String("<label>") + label + "<input name='" + name + "' type='" + type + "' value='" + esc(value) + "'></label>";
}
String peerIP(int n) {
  int pos = 0;
  for (int i = 1; i <= 10; i++) {
    int end = peers.indexOf('\n', pos); if (end < 0) end = peers.length();
    if (i == n) { String ip = peers.substring(pos, end); ip.trim(); return ip; }
    pos = end + 1;
  }
  return "";
}
bool validPeers(String text) {
  int pos = 0, count = 0;
  while (pos < (int)text.length()) {
    int end = text.indexOf('\n', pos); if (end < 0) end = text.length();
    String item = text.substring(pos, end); item.trim(); IPAddress ip;
    if (item.length() && !ip.fromString(item)) return false;
    if (++count > 10) return false;
    pos = end + 1;
  }
  return true;
}
void home() {
  if (!auth()) return;
  String h = R"HTML(<!doctype html><html lang='ru'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Stage ESP32</title><style>body{font:17px system-ui;max-width:780px;margin:24px auto;padding:16px;background:#17191d;color:#eee}label{display:block;margin:12px 0}input,textarea,select,button{font:inherit;padding:8px;max-width:95%}input{display:block}button{margin:5px;cursor:pointer}section{border-top:1px solid #555;margin-top:24px;padding-top:16px}a{color:#8cf}</style><h1>Stage ESP32</h1>)HTML";
  h += "<p>Версия " + String(VERSION) + " | " + esc(hostname) + " | " + role + "</p><p>IP: " + WiFi.localIP().toString() + " | Wi-Fi: " + (WiFi.status() == WL_CONNECTED ? "online" : "offline") + " | Flash: " + String(ESP.getFlashChipSize()/1048576) + " MB | PSRAM: " + String(ESP.getPsramSize()/1048576) + " MB</p>";
  h += "<section><h2>RGB этой платы</h2><form method='post' action='/led'>" + token();
  h += "<select name='color'><option value='red'>Красный</option><option value='green'>Зелёный</option><option value='blue'>Синий</option><option value='off'>Выключить</option></select> <select name='blink'><option value='1'>Мигать</option><option value='0'>Постоянно</option></select><button>Применить</button></form></section>";
  if (role == "master") {
    h += "<section><h2>Проверка 10 ведомых</h2><p>Номер строки IP соответствует номеру ведомого. Ключ группы должен совпадать. Это тест связи, не синхронный запуск сцены.</p>";
    for (int i = 1; i <= 10; i++) {
      h += "<form method='post' action='/dispatch'>" + token() + "<input type='hidden' name='node' value='" + String(i) + "'>№" + String(i) + " " + esc(peerIP(i));
      h += " <select name='color'><option value='red'>Красный</option><option value='green'>Зелёный</option><option value='blue'>Синий</option><option value='off'>Выкл.</option></select><button>Отправить</button></form>";
    }
    h += "</section>";
  }
  h += "<section><h2>Настройки</h2><form method='post' action='/config'>" + token();
  h += "<label>Роль <select name='role'><option value='node'" + String(role == "node" ? " selected" : "") + ">Ведомый</option><option value='master'" + String(role == "master" ? " selected" : "") + ">Мастер</option></select></label>";
  h += field("Номер (1–10)", "id", String(nodeId), "number");
  h += "<label>GPIO RGB <select name='pin'><option value='48'" + String(ledPin == 48 ? " selected" : "") + ">48</option><option value='38'" + String(ledPin == 38 ? " selected" : "") + ">38</option></select></label>";
  h += field("Wi-Fi SSID (2.4 ГГц)", "ssid", ssid);
  h += field("Пароль Wi-Fi (пусто = сохранить прежний)", "wifi", "", "password");
  h += field("Новый ключ группы, минимум 12 символов (пусто = не менять)", "key", "", "password");
  h += "<label>IP ведомых, до 10 строк (только для мастера)<br><textarea name='peers' rows='10'>" + esc(peers) + "</textarea></label><button>Сохранить и перезагрузить</button></form></section>";
  h += "<section><h2>Обновление по Wi-Fi</h2><p>Выберите только firmware.ino.bin (НЕ merged / bootloader / partitions). Настройки сохраняются. Не отключайте питание.</p><form method='post' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button>Установить прошивку</button></form></section></html>";
  web.sendHeader("Cache-Control", "no-store"); web.send(200, "text/html; charset=utf-8", h);
}
bool setLED() {
  String c = web.arg("color");
  if (c != "red" && c != "green" && c != "blue" && c != "off") return false;
  red = c == "red" ? 16 : 0; green = c == "green" ? 16 : 0; blue = c == "blue" ? 16 : 0;
  blinking = web.arg("blink") != "0"; paint(true); return true;
}
void redirectHome() { web.sendHeader("Location", "/"); web.send(303); }
void setupRoutes() {
  const char *headers[] = {"X-Stage-Key", "Origin"}; web.collectHeaders(headers, 2);
  web.on("/", HTTP_GET, home);
  web.on("/led", HTTP_POST, [] {
    if (!auth() || !csrf()) return;
    if (!setLED()) { web.send(400, "text/plain", "Invalid color"); return; }
    redirectHome();
  });
  web.on("/api/led", HTTP_POST, [] {
    if (!apiAuth()) return;
    if (!setLED()) { web.send(400, "text/plain", "Invalid color"); return; }
    web.send(200, "application/json", "{\"ok\":true,\"id\":" + String(nodeId) + "}");
  });
  web.on("/api/status", HTTP_GET, [] {
    if (!apiAuth()) return;
    web.send(200, "application/json", "{\"version\":\"" + String(VERSION) + "\",\"id\":" + String(nodeId) + ",\"role\":\"" + role + "\"}");
  });
  web.on("/dispatch", HTTP_POST, [] {
    if (!auth() || !csrf()) return;
    int n = web.arg("node").toInt();
    if (role != "master" || n < 1 || n > 10 || peerIP(n).isEmpty()) { web.send(400, "text/plain", "Configure node IP first"); return; }
    String c = web.arg("color");
    if (c != "red" && c != "green" && c != "blue" && c != "off") { web.send(400); return; }
    HTTPClient http; WiFiClient client;
    http.setConnectTimeout(800); http.setTimeout(1000);
    http.begin(client, "http://" + peerIP(n) + "/api/led");
    http.addHeader("X-Stage-Key", fleetKey); http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST("color=" + c + "&blink=1"); String response = http.getString(); http.end();
    web.send(code == 200 ? 200 : 502, "text/plain; charset=utf-8", "Node " + String(n) + ": HTTP " + String(code) + "\n" + response + "\nВернитесь назад к пульту.");
  });
  web.on("/config", HTTP_POST, [] {
    if (!auth() || !csrf()) return;
    int id = web.arg("id").toInt(), pin = web.arg("pin").toInt();
    String r = web.arg("role"), key = web.arg("key"), list = web.arg("peers");
    if (id < 1 || id > 10 || (pin != 38 && pin != 48) || (r != "master" && r != "node") ||
        (key.length() && (key.length() < 12 || key.length() > 64)) || web.arg("ssid").length() > 32 ||
        web.arg("wifi").length() > 63 || !validPeers(list)) { web.send(400, "text/plain", "Invalid configuration"); return; }
    prefs.putInt("id", id); prefs.putInt("pin", pin); prefs.putString("role", r);
    prefs.putString("ssid", web.arg("ssid")); prefs.putString("peers", list);
    if (web.arg("wifi").length()) prefs.putString("wifi", web.arg("wifi"));
    if (key.length()) prefs.putString("key", key);
    web.send(200, "text/plain; charset=utf-8", "Сохранено. Перезагрузка; адрес в сети смотрите на роутере или USB Serial 115200."); restartAt = millis() + 1000;
  });
  web.on("/update", HTTP_POST, [] {
    if (!auth()) return;
    if (!otaOK) { web.send(400, "text/plain", "Update failed. Use application .bin for this board."); return; }
    web.send(200, "text/plain", "Update OK. Rebooting."); restartAt = millis() + 1000;
  }, [] {
    HTTPUpload &u = web.upload();
    if (u.status == UPLOAD_FILE_START) {
      otaOK = false; otaActive = false;
      // Reject browser cross-origin uploads; curl without Origin remains supported.
      String origin = web.header("Origin");
      if (origin.length() && origin != "http://" + web.hostHeader()) return;
      otaActive = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
    } else if (u.status == UPLOAD_FILE_WRITE && otaActive) {
      if (Update.write(u.buf, u.currentSize) != u.currentSize) { Update.abort(); otaActive = false; }
    } else if (u.status == UPLOAD_FILE_END && otaActive) {
      otaOK = Update.end(true); otaActive = false;
    } else if (u.status == UPLOAD_FILE_ABORTED) {
      Update.abort(); otaActive = false; otaOK = false;
    }
  });
  web.onNotFound([] { web.send(404, "text/plain", "Not found"); }); web.begin();
}
void setup() {
  Serial.begin(115200); delay(500);
  prefs.begin("stage", false);
  adminPass = prefs.getString("admin", "");
  if (adminPass.isEmpty()) {
    char pass[25]; snprintf(pass, sizeof(pass), "%08lx%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
    adminPass = pass; prefs.putString("admin", adminPass);
  }
  fleetKey = prefs.getString("key", adminPass);
  ssid = prefs.getString("ssid", ""); wifiPass = prefs.getString("wifi", "");
  role = prefs.getString("role", "node"); nodeId = prefs.getInt("id", 1); ledPin = prefs.getInt("pin", 48);
  peers = prefs.getString("peers", "");
  uint64_t mac = ESP.getEfuseMac(); char suffix[7]; snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)(mac & 0xffffff));
  hostname = "stage-" + role + "-" + String(nodeId) + "-" + suffix;
  // New SSID avoids cached credentials from the earlier random-password build.
  apName = "Stage-Setup-" + String(suffix) + "-v2";
  WiFi.mode(WIFI_AP_STA); WiFi.setHostname(hostname.c_str()); WiFi.setAutoReconnect(true);
  Serial.printf("\nStage %s | %s\nWeb: no login\nAP password: %s\n", VERSION, hostname.c_str(), apPassword().c_str());
  if (ssid.length()) { WiFi.begin(ssid.c_str(), wifiPass.c_str()); disconnectedAt = millis(); }
  else startAP();
  setupRoutes(); paint(true);
}
void loop() {
  // USB-only diagnostics, usable after startup without resetting the board.
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'i') {
      Serial.printf("Stage %s uptime=%lus RGB=GPIO%d flash=%u PSRAM=%u\nAP: %s\nAP password: %s\nWeb: no login\nLAN: %s\n", VERSION, (unsigned long)(millis()/1000), ledPin, ESP.getFlashChipSize(), ESP.getPsramSize(), apName.c_str(), apPassword().c_str(), WiFi.localIP().toString().c_str());
    } else if (cmd == 't') {
      paint(false); ledPin = ledPin == 48 ? 38 : 48; paint(true);
      Serial.printf("RGB test: GPIO%d (not saved; save through web)\n", ledPin);
    }
  }
  web.handleClient();
  static uint32_t lastLED = 0; static bool on = false, connected = false;
  uint32_t now = millis();
  if (now - lastLED >= 500) { lastLED = now; on = !on; paint(!blinking || on); }
  if (WiFi.status() == WL_CONNECTED) {
    disconnectedAt = 0;
    if (!connected) {
      connected = true; Serial.printf("LAN: http://%s  (%s.local)\n", WiFi.localIP().toString().c_str(), hostname.c_str());
      MDNS.begin(hostname.c_str()); MDNS.addService("http", "tcp", 80);
      // Setup AP closes after successful connection. Returns after 60s offline.
      if (apRunning) { WiFi.softAPdisconnect(true); apRunning = false; }
    }
  } else {
    if (connected) { connected = false; MDNS.end(); }
    if (!disconnectedAt) disconnectedAt = now;
    if (ssid.length() && now - lastRetry > 15000) { lastRetry = now; WiFi.reconnect(); }
    if (now - disconnectedAt > 60000) startAP();
  }
  if (restartAt && (int32_t)(now - restartAt) >= 0) ESP.restart();
  delay(2);
}
