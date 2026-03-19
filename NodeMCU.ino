#include <ESP8266WiFi.h>

const char* ssid = "Telecom_Network";
const char* password = "telecom123";

WiFiServer server(80);
const IPAddress apIP(192, 168, 4, 1);
const IPAddress apGateway(192, 168, 4, 1);
const IPAddress apSubnet(255, 255, 255, 0);
const unsigned long apHealthCheckIntervalMs = 5000;
unsigned long lastApHealthCheck = 0;

// LED Pins
int Ag = D1, Ay = D2, Ar = D3;
int Bg = D4, By = D5, Br = D6;
int Cg = D7, Cy = D8, Cr = D0;

// Tower loads
int towerA = 0, towerB = 0, towerC = 0;

bool autoMode = true;
unsigned long lastAutoUpdate = 0;
const unsigned long autoUpdateIntervalMs = 4000;

// -------- Utility --------
int clampUsers(int users)
{
  return constrain(users, 0, 9);
}

int parseTowerValue(const String& req, const String& key, int fallback)
{
  int start = req.indexOf(key);
  if (start == -1) return fallback;

  start += key.length();
  int end = req.indexOf('&', start);
  if (end == -1) end = req.indexOf(' ', start);
  if (end == -1) end = req.length();

  return clampUsers(req.substring(start, end).toInt());
}

String readHttpRequest(WiFiClient& client)
{
  String req = "";
  unsigned long start = millis();

  while (client.connected() && millis() - start < 1000)
  {
    while (client.available())
    {
      char c = client.read();
      req += c;

      if (req.endsWith("\r\n\r\n"))
        return req;
    }
    delay(1);
  }
  return req;
}

// -------- Core Logic --------
void setTower(int users, int g, int y, int r)
{
  digitalWrite(g, LOW);
  digitalWrite(y, LOW);
  digitalWrite(r, LOW);

  if (users <= 3) digitalWrite(g, HIGH);
  else if (users <= 6) digitalWrite(y, HIGH);
  else digitalWrite(r, HIGH);
}

void balanceLoad()
{
  while (towerA > 6 || towerB > 6 || towerC > 6)
  {
    if (towerA > 6)
    {
      towerA--;
      (towerB < towerC) ? towerB++ : towerC++;
    }
    if (towerB > 6)
    {
      towerB--;
      (towerA < towerC) ? towerA++ : towerC++;
    }
    if (towerC > 6)
    {
      towerC--;
      (towerA < towerB) ? towerA++ : towerB++;
    }
  }
}

void simulateTraffic()
{
  towerA = random(0, 10);
  towerB = random(0, 10);
  towerC = random(0, 10);
}

void applyTowerOutputs()
{
  setTower(towerA, Ag, Ay, Ar);
  setTower(towerB, Bg, By, Br);
  setTower(towerC, Cg, Cy, Cr);
}

void updateAutomaticMode(bool force = false)
{
  if (!autoMode) return;

  unsigned long now = millis();
  if (force || now - lastAutoUpdate >= autoUpdateIntervalMs)
  {
    simulateTraffic();
    balanceLoad();
    lastAutoUpdate = now;
  }
}

bool startSoftAP()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  bool configured = WiFi.softAPConfig(apIP, apGateway, apSubnet);
  bool started = WiFi.softAP(ssid, password);

  Serial.print("AP config status: ");
  Serial.println(configured ? "OK" : "FAILED");
  Serial.print("AP start status: ");
  Serial.println(started ? "OK" : "FAILED");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  return configured && started;
}

void ensureSoftAPRunning()
{
  unsigned long now = millis();
  if (now - lastApHealthCheck < apHealthCheckIntervalMs) return;

  lastApHealthCheck = now;
  if (WiFi.getMode() != WIFI_AP || WiFi.softAPIP() != apIP)
  {
    Serial.println("SoftAP stopped responding. Restarting Wi-Fi AP...");
    startSoftAP();
    server.begin();
  }
}

// -------- Setup --------
void setup()
{
  Serial.begin(115200);

  pinMode(Ag, OUTPUT); pinMode(Ay, OUTPUT); pinMode(Ar, OUTPUT);
  pinMode(Bg, OUTPUT); pinMode(By, OUTPUT); pinMode(Br, OUTPUT);
  pinMode(Cg, OUTPUT); pinMode(Cy, OUTPUT); pinMode(Cr, OUTPUT);

  randomSeed(analogRead(A0));

  startSoftAP();

  updateAutomaticMode(true);
  applyTowerOutputs();

  server.begin();
}

// -------- Loop --------
void loop()
{
  ensureSoftAPRunning();
  updateAutomaticMode();
  applyTowerOutputs();

  WiFiClient client = server.available();
  if (!client) return;

  String req = readHttpRequest(client);
  if (req.length() == 0)
  {
    client.stop();
    return;
  }

  // -------- AJAX ENDPOINT --------
  if (req.indexOf("GET /data") != -1)
  {
    if (req.indexOf("mode=manual") != -1) autoMode = false;

    if (req.indexOf("mode=auto") != -1)
    {
      autoMode = true;
      updateAutomaticMode(true);
    }

    if (!autoMode)
    {
      towerA = parseTowerValue(req, "A=", towerA);
      towerB = parseTowerValue(req, "B=", towerB);
      towerC = parseTowerValue(req, "C=", towerC);
      balanceLoad();
    }

    applyTowerOutputs();

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();

    client.print("{\"A\":"); client.print(towerA);
    client.print(",\"B\":"); client.print(towerB);
    client.print(",\"C\":"); client.print(towerC);
    client.print(",\"mode\":\""); client.print(autoMode ? "auto" : "manual"); client.print("\"}");

    delay(5);
    client.stop();
    return;
  }

  // -------- WEB PAGE --------
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");

  client.println("<style>");
  client.println("body{background:#020617;color:#e2e8f0;font-family:Arial;text-align:center}");
  client.println(".panel{background:#0f172a;padding:15px;margin:10px;border-radius:10px}");
  client.println(".bar{height:20px;background:#22c55e;border-radius:10px;margin-top:5px}");
  client.println("button{padding:10px;margin:5px;background:#2563eb;color:white;border:none}");
  client.println("input{padding:6px;margin:5px}");
  client.println("</style>");

  client.println("</head><body>");

  client.println("<h1>Telecom Control</h1>");
  client.println("<p id='modeText'></p>");

  client.println("<button onclick=\"setMode('auto')\">Auto</button>");
  client.println("<button onclick=\"setMode('manual')\">Manual</button>");

  client.println("<div>");
  client.println("A <input id='A' type='number'>");
  client.println("B <input id='B' type='number'>");
  client.println("C <input id='C' type='number'>");
  client.println("<button onclick='updateManual()'>Update</button>");
  client.println("</div>");

  client.println("<div class='panel'>A <span id='valA'></span><div class='bar' id='barA'></div></div>");
  client.println("<div class='panel'>B <span id='valB'></span><div class='bar' id='barB'></div></div>");
  client.println("<div class='panel'>C <span id='valC'></span><div class='bar' id='barC'></div></div>");

  client.println("<script>");
  client.println("function updateUI(d){");
  client.println("valA.innerText=d.A; valB.innerText=d.B; valC.innerText=d.C;");
  client.println("A.value=d.A; B.value=d.B; C.value=d.C;");
  client.println("barA.style.width=(d.A*20)+'px';");
  client.println("barB.style.width=(d.B*20)+'px';");
  client.println("barC.style.width=(d.C*20)+'px';");
  client.println("modeText.innerText='Mode: '+d.mode;");
  client.println("}");

  client.println("function fetchData(){fetch('/data').then(r=>r.json()).then(updateUI);}");  
  client.println("function setMode(m){fetch('/data?mode='+m).then(fetchData);}");  
  client.println("function updateManual(){fetch(`/data?mode=manual&A=${A.value}&B=${B.value}&C=${C.value}`).then(fetchData);}");  

  client.println("setInterval(fetchData,3000); fetchData();");
  client.println("</script>");

  client.println("</body></html>");

  delay(5);
  client.stop();
}
