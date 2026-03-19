#include <ESP8266WiFi.h>

/* --- Configuration --- */
const char* ssid = "Telecom_Network";
const char* password = "telecom123";

WiFiServer server(80);
const IPAddress apIP(192, 168, 4, 1);
const IPAddress apGateway(192, 168, 4, 1);
const IPAddress apSubnet(255, 255, 255, 0);

/* --- Pin Definitions --- */
// Tower A
const int Ag = D1, Ay = D2, Ar = D3;
// Tower B
const int Bg = D4, By = D5, Br = D6;
// Tower C
const int Cg = D7, Cy = D8, Cr = D0;

/* --- Variables --- */
int towerA = 0, towerB = 0, towerC = 0;
bool autoMode = true;
unsigned long lastAutoUpdate = 0;
const unsigned long autoUpdateIntervalMs = 4000;

/* --- Logic Functions --- */
int clampUsers(int users) { 
  return constrain(users, 0, 9); 
}

int parseTowerValue(const String& req, const String& key, int fallback) {
  int start = req.indexOf(key);
  if (start == -1) return fallback;
  start += key.length();
  int end = req.indexOf('&', start);
  if (end == -1) end = req.indexOf(' ', start);
  return clampUsers(req.substring(start, end).toInt());
}

void setTower(int users, int g, int y, int r) {
  digitalWrite(g, users <= 3 ? HIGH : LOW);
  digitalWrite(y, (users > 3 && users <= 6) ? HIGH : LOW);
  digitalWrite(r, users > 6 ? HIGH : LOW);
}

void balanceLoad() {
  int maxLoad = max(towerA, max(towerB, towerC));
  if (maxLoad <= 6) return; // No balancing needed if no tower is "Red"
  
  int excess = maxLoad - 6;
  if (towerA == maxLoad) {
    towerA -= excess;
    towerB += excess / 2;
    towerC += excess - (excess / 2);
  } else if (towerB == maxLoad) {
    towerB -= excess;
    towerA += excess / 2;
    towerC += excess - (excess / 2);
  } else {
    towerC -= excess;
    towerA += excess / 2;
    towerB += excess - (excess / 2);
  }
}

void simulateTraffic() {
  towerA = random(1, 10);
  towerB = random(1, 10);
  towerC = random(1, 10);
}

void applyTowerOutputs() {
  setTower(towerA, Ag, Ay, Ar);
  setTower(towerB, Bg, By, Br);
  setTower(towerC, Cg, Cy, Cr);
}

void updateAutomaticMode(bool force = false) {
  if (!autoMode && !force) return;
  unsigned long now = millis();
  if (force || now - lastAutoUpdate >= autoUpdateIntervalMs) {
    simulateTraffic();
    balanceLoad();
    lastAutoUpdate = now;
    applyTowerOutputs();
  }
}

/* --- Web Server --- */
void handleClient(WiFiClient client) {
  String req = "";
  unsigned long timeout = millis();
  // Read request
  while (client.connected() && millis() - timeout < 1000) {
    if (client.available()) {
      char c = client.read();
      req += c;
      if (req.endsWith("\r\n\r\n")) break;
    }
  }

  if (req.indexOf("GET /data") != -1) {
    // Handle AJAX Data Requests
    if (req.indexOf("mode=manual") != -1) autoMode = false;
    if (req.indexOf("mode=auto") != -1) { 
      autoMode = true; 
      updateAutomaticMode(true); 
    }
    if (!autoMode && req.indexOf("A=") != -1) {
      towerA = parseTowerValue(req, "A=", towerA);
      towerB = parseTowerValue(req, "B=", towerB);
      towerC = parseTowerValue(req, "C=", towerC);
      balanceLoad();
      applyTowerOutputs();
    }
    
    client.println("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n");
    client.printf("{\"A\":%d,\"B\":%d,\"C\":%d,\"mode\":\"%s\"}", 
                  towerA, towerB, towerC, autoMode ? "auto" : "manual");
  } 
  else {
    // Send Main HTML Page
    client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n");
    client.println(F("<!DOCTYPE html><html><head><title>Telecom Control</title>"));
    client.println(F("<meta name='viewport' content='width=device-width, initial-scale=1'>"));
    client.println(F("<style>body{background:#020617;color:#e2e8f0;font-family:sans-serif;text-align:center}"));
    client.println(F(".panel{background:#0f172a;padding:15px;margin:10px auto;border-radius:10px;max-width:400px}"));
    client.println(F(".bar{height:20px;background:#22c55e;border-radius:10px;transition:0.5s;width:0%}"));
    client.println(F("button{padding:10px 20px;margin:5px;background:#2563eb;color:white;border:none;border-radius:5px;cursor:pointer}"));
    client.println(F("input{width:50px;padding:8px;border-radius:5px;border:none;margin:5px}</style></head><body>"));
    client.println(F("<h1>Telecom Load Balancer</h1><p id='modeText'>Loading...</p>"));
    client.println(F("<button onclick=\"setMode('auto')\">Auto Mode</button>"));
    client.println(F("<button onclick=\"setMode('manual')\">Manual Mode</button>"));
    client.println(F("<div class='panel'><h3>Manual Input</h3>"));
    client.println(F("A <input id='A' type='number' min='0' max='9'> "));
    client.println(F("B <input id='B' type='number' min='0' max='9'> "));
    client.println(F("C <input id='C' type='number' min='0' max='9'>"));
    client.println(F("<br><button onclick='updateManual()'>Apply & Balance</button></div>"));
    client.println(F("<div class='panel'>Tower A: <span id='valA'>0</span>/9<div class='bar' id='barA'></div></div>"));
    client.println(F("<div class='panel'>Tower B: <span id='valB'>0</span>/9<div class='bar' id='barB'></div></div>"));
    client.println(F("<div class='panel'>Tower C: <span id='valC'>0</span>/9<div class='bar' id='barC'></div></div>"));
    client.println(F("<script>function updateUI(d){"));
    client.println(F("document.getElementById('valA').innerText=d.A;document.getElementById('valB').innerText=d.B;document.getElementById('valC').innerText=d.C;"));
    client.println(F("document.getElementById('barA').style.width=(d.A*11.1)+'%';document.getElementById('barB').style.width=(d.B*11.1)+'%';document.getElementById('barC').style.width=(d.C*11.1)+'%';"));
    client.println(F("document.getElementById('modeText').innerText='Current Mode: '+d.mode.toUpperCase();}"));
    client.println(F("function fetchData(){fetch('/data').then(r=>r.json()).then(updateUI);}"));
    client.println(F("function setMode(m){fetch('/data?mode='+m).then(fetchData);}"));
    client.println(F("function updateManual(){let a=document.getElementById('A').value, b=document.getElementById('B').value, c=document.getElementById('C').value;"));
    client.println(F("fetch(`/data?mode=manual&A=${a}&B=${b}&C=${c}`).then(fetchData);}"));
    client.println(F("setInterval(fetchData,3000);fetchData();</script></body></html>"));
  }
  delay(1);
  client.stop();
}

/* --- Core Setup & Loop --- */
void setup() {
  Serial.begin(115200);
  
  // Set all pins as output
  int pins[] = {Ag, Ay, Ar, Bg, By, Br, Cg, Cy, Cr};
  for (int i = 0; i < 9; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }

  randomSeed(analogRead(A0));
  
  // Start Access Point
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP(ssid, password);
  
  server.begin();
  Serial.println("Server Started. Connect to: " + String(ssid));
  
  updateAutomaticMode(true); // Initial state
}

void loop() {
  updateAutomaticMode();
  
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
}
