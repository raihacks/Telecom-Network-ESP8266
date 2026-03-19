#include <ESP8266WiFi.h>
const char* ssid="Telecom_NOC_Mobile";
const char* password="telecom123";
WiFiServer server(80);
IPAddress apIP(192,168,4,1);
IPAddress apGateway(192,168,4,1);
IPAddress apSubnet(255,255,255,0);
const int Ag=D1,Ay=D2,Ar=D3;
const int Bg=D4,By=D5,Br=D6;
const int Cg=D7,Cy=D8,Cr=D0;
int towers[3]={0,0,0};
bool autoMode=true;
bool criticalAlarm=false;
String lastEvent="System Ready";
void setLED(int users,int g,int y,int r){
digitalWrite(g,users<=3);
digitalWrite(y,(users>3 && users<=6));
digitalWrite(r,(users>6));
}
void balanceTraffic(){
int total=towers[0]+towers[1]+towers[2];
criticalAlarm=(total>18);
for(int i=0;i<3;i++){
if(towers[i]>6){
int excess=towers[i]-6;
towers[i]=6;
int n1=(i+1)%3;
int n2=(i+2)%3;
towers[n1]+=excess/2;
towers[n2]+=excess-excess/2;
lastEvent="Balancing Tower "+String(char('A'+i));
}
}
for(int i=0;i<3;i++)
towers[i]=constrain(towers[i],0,9);
}
void simulateTraffic(){
for(int i=0;i<3;i++)
towers[i]=random(2,10);
balanceTraffic();
}
void updateLEDs(){
setLED(towers[0],Ag,Ay,Ar);
setLED(towers[1],Bg,By,Br);
setLED(towers[2],Cg,Cy,Cr);
}
void updateSystem(){
if(autoMode)
simulateTraffic();
updateLEDs();
}
int getVal(String req,String key){
int s=req.indexOf(key);
if(s==-1) return 0;
s+=key.length();
int e=req.indexOf('&',s);
if(e==-1) e=req.length();
return req.substring(s,e).toInt();
}
void handleClient(WiFiClient client){
String req=client.readStringUntil('\r');
if(req.indexOf("/data")!=-1){
if(req.indexOf("mode=auto")!=-1) autoMode=true;
if(req.indexOf("mode=manual")!=-1) autoMode=false;
if(req.indexOf("A=")!=-1){
autoMode=false;
towers[0]=constrain(getVal(req,"A="),0,9);
towers[1]=constrain(getVal(req,"B="),0,9);
towers[2]=constrain(getVal(req,"C="),0,9);
balanceTraffic();
lastEvent="Manual Load Applied";
}
updateSystem();
int total=towers[0]+towers[1]+towers[2];
client.println("HTTP/1.1 200 OK");
client.println("Content-Type: application/json");
client.println();
client.printf("{\"A\":%d,\"B\":%d,\"C\":%d,\"total\":%d,\"alarm\":%d,\"msg\":\"%s\"}",
towers[0],towers[1],towers[2],total,criticalAlarm,lastEvent.c_str());
}
else{
client.println("HTTP/1.1 200 OK");
client.println("Content-Type: text/html");
client.println();
client.println(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{
background:#020617;
color:white;
font-family:sans-serif;
padding:10px;
}
.card{
background:#0f172a;
padding:15px;
border-radius:10px;
margin-bottom:10px;
}
button{
padding:10px;
margin:4px;
border:none;
border-radius:6px;
background:#2563eb;
color:white;
}
input{width:50px}
.alert{
display:none;
background:#ef4444;
padding:10px;
text-align:center;
font-weight:bold;
animation:blink 1s infinite;
border-radius:8px;
margin-bottom:10px;
}
@keyframes blink{
0%{opacity:1}
50%{opacity:0.3}
100%{opacity:1}
}
.status{
display:inline-block;
width:12px;
height:12px;
border-radius:50%;
margin-right:6px;
}
</style>
</head>
<body>
<h2>Smart Load Balancing</h2>
<div id="alert" class="alert">
NETWORK OVERLOAD DETECTED
</div>
<div class="card">
<button onclick="mode('auto')">Auto</button>
<button onclick="mode('manual')">Manual</button>
</div>
<div class="card">
<h3>Manual Load</h3>
A <input id="A" type="number" min="0" max="9">
B <input id="B" type="number" min="0" max="9">
C <input id="C" type="number" min="0" max="9">
<br><br>
<button onclick="setLoad()">Apply Load</button>
</div>
<div class="card">
<p id="msg"></p>
<p>Total Users: <span id="total"></span></p>
<h3>Network Health</h3>
<div id="health"></div>
</div>
<div class="card">
<h3>Live Tower Load</h3>
<div>
<span id="sA" class="status"></span>
Tower A : <span id="vA"></span>
</div>
<br>
<div>
<span id="sB" class="status"></span>
Tower B : <span id="vB"></span>
</div>
<br>
<div>
<span id="sC" class="status"></span>
Tower C : <span id="vC"></span>
</div>
</div>
<script>
function cmd(p){
fetch("/data?"+p)
.then(r=>r.json())
.then(update)
}
function mode(m){
fetch("/data?mode="+m)
.then(r=>r.json())
.then(update)
}
function setLoad(){
let a=document.getElementById("A").value
let b=document.getElementById("B").value
let c=document.getElementById("C").value
fetch(`/data?mode=manual&A=${a}&B=${b}&C=${c}`)
.then(r=>r.json())
.then(update)
}
function statusColor(load){
if(load<=3) return "#22c55e"
if(load<=6) return "#eab308"
return "#ef4444"
}
function update(d){
document.getElementById("msg").innerText=d.msg
document.getElementById("total").innerText=d.total
document.getElementById("vA").innerText=d.A+" users"
document.getElementById("vB").innerText=d.B+" users"
document.getElementById("vC").innerText=d.C+" users"
document.getElementById("sA").style.background=statusColor(d.A)
document.getElementById("sB").style.background=statusColor(d.B)
document.getElementById("sC").style.background=statusColor(d.C)
let health="Healthy"
if(d.total>18) health="Congested"
else if(d.total>10) health="Moderate"
document.getElementById("health").innerText=health
if(d.alarm==1){
document.getElementById("alert").style.display="block"
}
else{
document.getElementById("alert").style.display="none"
}}
setInterval(()=>cmd(""),3000)
cmd("")
</script>
</body>
</html>
)rawliteral");
}}
void setup(){
Serial.begin(115200);
int p[]={Ag,Ay,Ar,Bg,By,Br,Cg,Cy,Cr};
for(int i=0;i<9;i++)
pinMode(p[i],OUTPUT);
WiFi.softAPConfig(apIP,apGateway,apSubnet);
WiFi.softAP(ssid,password);
server.begin();
randomSeed(analogRead(A0));
}
void loop(){
static unsigned long last=0;
if(millis()-last>4000){
updateSystem();
last=millis();
}
WiFiClient client=server.available();
if(client)
handleClient(client);
}
