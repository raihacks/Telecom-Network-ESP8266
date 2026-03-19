#include <ESP8266WiFi.h>

const char* ssid = "Telecom_Network";
const char* password = "123456789";

WiFiServer server(80);

int Ag = D1;
int Ay = D2;
int Ar = D3;
int Bg = D4;
int By = D5;
int Br = D6;
int Cg = D7;
int Cy = D8;
int Cr = D0;

int towerA = 0;
int towerB = 0;
int towerC = 0;

bool autoMode = true;
unsigned long lastAutoUpdate = 0;
const unsigned long autoUpdateIntervalMs = 4000;

int clampUsers(int users)
{
  if (users < 0) return 0;
  if (users > 9) return 9;
  return users;
}

int parseTowerValue(const String& req, const String& key, int fallback)
{
  int start = req.indexOf(key);

  if (start == -1)
  {
    return fallback;
  }

  start += key.length();
  int end = req.indexOf('&', start);

  if (end == -1)
  {
    end = req.indexOf(' ', start);
  }

  if (end == -1)
  {
    end = req.length();
  }

  return clampUsers(req.substring(start, end).toInt());
}

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
      if (towerB < towerC) towerB++;
      else towerC++;
    }

    if (towerB > 6)
    {
      towerB--;
      if (towerA < towerC) towerA++;
      else towerC++;
    }

    if (towerC > 6)
    {
      towerC--;
      if (towerA < towerB) towerA++;
      else towerB++;
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

void updateAutomaticMode(bool forceUpdate = false)
{
  if (!autoMode)
  {
    return;
  }

  unsigned long now = millis();

  if (forceUpdate || now - lastAutoUpdate >= autoUpdateIntervalMs)
  {
    simulateTraffic();
    balanceLoad();
    lastAutoUpdate = now;
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(Ag, OUTPUT);
  pinMode(Ay, OUTPUT);
  pinMode(Ar, OUTPUT);

  pinMode(Bg, OUTPUT);
  pinMode(By, OUTPUT);
  pinMode(Br, OUTPUT);

  pinMode(Cg, OUTPUT);
  pinMode(Cy, OUTPUT);
  pinMode(Cr, OUTPUT);

  randomSeed(analogRead(0));

  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.println("Network Started");
  Serial.println(WiFi.softAPIP());

  updateAutomaticMode(true);
  applyTowerOutputs();
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  server.begin();
}

void loop()
{
  updateAutomaticMode();
  applyTowerOutputs();

  WiFiClient client = server.available();

  if (!client) return;

  String req = "";

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

  client.print("{");
  client.print("\"A\":"); client.print(towerA); client.print(",");
  client.print("\"B\":"); client.print(towerB); client.print(",");
  client.print("\"C\":"); client.print(towerC); client.print(",");
  client.print("\"mode\":\""); client.print(autoMode ? "auto" : "manual"); client.print("\"");
  client.print("}");

  delay(1);
  client.stop();
  return;
}

  if (req.indexOf("mode=manual") != -1)
  {
    autoMode = false;
  }

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
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println("Cache-Control: no-cache, no-store, must-revalidate");
  client.println("Pragma: no-cache");
  client.println("Expires: 0");
  client.println();

  client.println("<body>");
client.println("<h1>Telecom Network Control</h1>");

client.println("<p id='modeText'></p>");

client.println("<div class='mode'>");
client.println("<button onclick=\"setMode('auto')\">Automatic</button>");
client.println("<button onclick=\"setMode('manual')\">Manual</button>");
client.println("</div>");

client.println("<div class='box'>Tower A <input id='A' type='number' min='0' max='9'></div>");
client.println("<div class='box'>Tower B <input id='B' type='number' min='0' max='9'></div>");
client.println("<div class='box'>Tower C <input id='C' type='number' min='0' max='9'></div>");
client.println("<button onclick='updateManual()'>Update</button>");

client.println("<div class='panel'>A: <span id='valA'></span><div class='bar' id='barA'></div></div>");
client.println("<div class='panel'>B: <span id='valB'></span><div class='bar' id='barB'></div></div>");
client.println("<div class='panel'>C: <span id='valC'></span><div class='bar' id='barC'></div></div>");

client.println("<script>");

client.println("function updateUI(data){");
client.println("document.getElementById('valA').innerText=data.A;");
client.println("document.getElementById('valB').innerText=data.B;");
client.println("document.getElementById('valC').innerText=data.C;");

client.println("document.getElementById('barA').style.width=(data.A*20)+'px';");
client.println("document.getElementById('barB').style.width=(data.B*20)+'px';");
client.println("document.getElementById('barC').style.width=(data.C*20)+'px';");

client.println("document.getElementById('modeText').innerText='Mode: '+data.mode;");
client.println("}");

client.println("function fetchData(){");
client.println("fetch('/data').then(r=>r.json()).then(updateUI);");
client.println("}");

client.println("function setMode(m){");
client.println("fetch('/data?mode='+m).then(fetchData);");
client.println("}");

client.println("function updateManual(){");
client.println("let A=document.getElementById('A').value;");
client.println("let B=document.getElementById('B').value;");
client.println("let C=document.getElementById('C').value;");
client.println("fetch(`/data?A=${A}&B=${B}&C=${C}`).then(fetchData);");
client.println("}");

client.println("setInterval(fetchData,2000);");
client.println("fetchData();");

client.println("</script>");

client.println("</body>");
client.println("</html>");
  delay(1);
  client.stop();
}
