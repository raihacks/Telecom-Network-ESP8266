#include <ESP8266WiFi.h>

const char* ssid = "Telecom_Network";
const char* password = "123456789";

WiFiServer server(80);

int Ag=D1; int Ay=D2; int Ar=D3;
int Bg=D4; int By=D5; int Br=D6;
int Cg=D7; int Cy=D8; int Cr=D0;

int towerA=0;
int towerB=0;
int towerC=0;

bool autoMode = true;

void setTower(int users,int g,int y,int r)
{
digitalWrite(g,LOW);
digitalWrite(y,LOW);
digitalWrite(r,LOW);

if(users<=3) digitalWrite(g,HIGH);
else if(users<=6) digitalWrite(y,HIGH);
else digitalWrite(r,HIGH);
}

void balanceLoad()
{
while(towerA>6 || towerB>6 || towerC>6)
{
if(towerA>6)
{
towerA--;
if(towerB<towerC) towerB++;
else towerC++;
}

if(towerB>6)
{
towerB--;
if(towerA<towerC) towerA++;
else towerC++;
}

if(towerC>6)
{
towerC--;
if(towerA<towerB) towerA++;
else towerB++;
}
}
}

void simulateTraffic()
{
towerA=random(0,10);
towerB=random(0,10);
towerC=random(0,10);
}

void setup()
{

Serial.begin(115200);

pinMode(Ag,OUTPUT);
pinMode(Ay,OUTPUT);
pinMode(Ar,OUTPUT);

pinMode(Bg,OUTPUT);
pinMode(By,OUTPUT);
pinMode(Br,OUTPUT);

pinMode(Cg,OUTPUT);
pinMode(Cy,OUTPUT);
pinMode(Cr,OUTPUT);

randomSeed(analogRead(0));

WiFi.softAP(ssid,password);

Serial.println("Network Started");
Serial.println(WiFi.softAPIP());

server.begin();
}

void loop()
{

if(autoMode)
{
simulateTraffic();
balanceLoad();
delay(4000);
}

setTower(towerA,Ag,Ay,Ar);
setTower(towerB,Bg,By,Br);
setTower(towerC,Cg,Cy,Cr);

WiFiClient client=server.available();

if(!client) return;

String req=client.readStringUntil('\r');
client.flush();

if(req.indexOf("mode=manual")!=-1) autoMode=false;
if(req.indexOf("mode=auto")!=-1) autoMode=true;

int posA=req.indexOf("A=");
int posB=req.indexOf("B=");
int posC=req.indexOf("C=");

if(posA!=-1) towerA=req.substring(posA+2).toInt();
if(posB!=-1) towerB=req.substring(posB+2).toInt();
if(posC!=-1) towerC=req.substring(posC+2).toInt();

balanceLoad();

client.println("HTTP/1.1 200 OK");
client.println("Content-type:text/html");
client.println("");

client.println("<html>");
client.println("<head>");

client.println("<style>");
client.println("body{background:#020617;color:#e2e8f0;font-family:Arial;text-align:center}");
client.println(".panel{background:#0f172a;padding:20px;margin:20px;border-radius:12px}");
client.println(".bar{height:20px;background:#22c55e;margin:5px}");
client.println(".warn{color:#ef4444;font-size:22px}");
client.println("button{padding:12px 20px;font-size:18px;background:#2563eb;color:white;border:none;border-radius:6px}");
client.println("</style>");

client.print("<div class='panel'>Tower A Users: ");
client.print(towerA);
client.println("<div class='bar' style='width:"+String(towerA*20)+"px'></div></div>");

client.print("<div class='panel'>Tower B Users: ");
client.print(towerB);
client.println("<div class='bar' style='width:"+String(towerB*20)+"px'></div></div>");

client.print("<div class='panel'>Tower C Users: ");
client.print(towerC);
client.println("<div class='bar' style='width:"+String(towerC*20)+"px'></div></div>");

client.println("</head>");
client.println("<body>");

client.println("<h1>Telecom Network Control</h1>");

client.println("<a href='/?mode=auto'><button>Automatic Mode</button></a>");
client.println("<a href='/?mode=manual'><button>Manual Mode</button></a>");

client.println("<form>");

client.println("<div class='box'>Tower A Users <input name='A'></div>");
client.println("<div class='box'>Tower B Users <input name='B'></div>");
client.println("<div class='box'>Tower C Users <input name='C'></div>");

client.println("<button type='submit'>Update</button>");

client.println("</form>");

client.print("<h2>Tower A: ");
client.print(towerA);
client.println("</h2>");

client.print("<h2>Tower B: ");
client.print(towerB);
client.println("</h2>");

client.print("<h2>Tower C: ");
client.print(towerC);
client.println("</h2>");

client.println("</body>");
client.println("</html>");

client.stop();
}
