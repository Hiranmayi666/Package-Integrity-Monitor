#include <Wire.h> // the ESP32 communicate with the MPU6050 using I2C protocol
#include <WiFi.h> //Helps ESP32 connect to WiFi
#include <WebServer.h> //Helps ESP32 become a website
#include <string.h>
#include <MPU6050.h>

const char* ssid     = "Galaxy M32"; //const char* - WiFi.begin() expects c style string
const char* password = "goodvibes";

WebServer server(80); //Control wifi requests
MPU6050 mpu; //Control sensor requests

const float IMPACT_THRESHOLD = 2.3;
const float FALL_THRESHOLD   = 0.5;
const float STILL_MIN        = 0.9;
const float STILL_MAX        = 1.1;

unsigned long alertTime = 0;
const unsigned long ALERT_HOLD = 2000;// 2 seconds

bool possibleFall = false;
unsigned long fallTime = 0;
const unsigned long FALL_WINDOW = 1000;   // milliseconds

float pitch = 0, roll = 0, raw = 0;
char orientation[20] = "UPRIGHT";
char motionState[20] = "STABLE"; 
/*char - fixed memory allocation
 *  string - dynamic memory allocation, can lead to heap fragmentation*/

void handleRoot(){
  server.sendHeader("Connection","close");
  const char* page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Package Integrity Monitor</title>

    <style>
* {
    box-sizing: border-box;
}

body {
    margin: 0;
    padding: 40px;

    font-family: Arial, sans-serif;

    background: #08162f;

    min-height: 100vh;

    display: flex;
    justify-content: center;
    align-items: center;
}

.container {
    width: 100%;
    max-width: 700px;
    position:relative;
    left=50%;
    transform = translateX(-50%);
}

h1 {
    margin: 0;
    margin-bottom: 10px;

    color: #ffffff;

    text-align: center;

    letter-spacing: 2px;

    font-size: 34px;
}

.subtitle {
    text-align: center;

    color: #c8d5ea;

    font-size: 17px;

    margin-bottom: 35px;
}

.dashboard {
    display: flex;

    flex-direction: column;

    gap: 18px;

    width: 100%;
}

.row {
    display: flex;

    gap: 18px;

    width: 100%;
}

.card {
    background: #eef3fb;

    border-radius: 20px;

    padding: 25px;

    text-align: center;

    box-shadow: 0 12px 28px rgba(0, 0, 0, 0.30);

    transition: 0.3s;

    width: 100%;
}

.card:hover {
    transform: translateY(-3px);

    box-shadow: 0 16px 35px rgba(0, 0, 0, 0.40);
}

.status-card {
    background: #edf6ff;

    padding: 35px;

    border: 4px solid #22c55e;

    transition: 0.3s;
}

.half-card {
    flex: 1;
    width: 50%;
}

h2 {
    margin: 0;

    margin-bottom: 15px;

    font-size: 18px;

    font-weight: 600;

    color: #56657c;
}

.status-value {
    margin: 0;

    font-size: 48px;

    font-weight: bold;

    color: #22c55e;
}

.sensor-value {
    margin: 0;

    font-size: 36px;

    font-weight: bold;

    color: #0077cc;
}
        </style>
   </head>
   <body>

<div class="container">

    <h1>PACKAGE INTEGRITY MONITOR</h1>
    <p class="subtitle">ESP32 • MPU6050 • Live Monitoring Dashboard</p>

    <div class="dashboard">

        <!-- Motion State -->
        <div class="card status-card" id="statusCard">
            <h2>Motion State</h2>
            <p id="status" class="status-value">--</p>
        </div>

        <!-- Pitch & Roll -->
        <div class="row">

            <div class="card half-card">
                <h2>Pitch</h2>
                <p id="pitch" class="sensor-value">--</p>
            </div>

            <div class="card half-card">
                <h2>Roll</h2>
                <p id="roll" class="sensor-value">--</p>
            </div>

        </div>

        <!-- Orientation -->
        <div class="card">
            <h2>Orientation</h2>
            <p id="orientation" class="sensor-value">--</p>
        </div>

        <!-- Magnitude -->
        <div class="card">
            <h2>Acceleration Magnitude</h2>
            <p id="magnitude" class="sensor-value">--</p>
        </div>

    </div>

</div>

<script>
   <!--JavaScript-->

     async function update(){

    const response = await fetch("/data");
    const d = await response.json();

    const statusCard = document.getElementById("statusCard");
    const status = document.getElementById("status");
    
    status.textContent = d.status;

    switch(d.status){

        case "STABLE":
            status.style.color = "#22c55e";
            statusCard.style.borderColor="#22c55e";
            break;

        case "IN TRANSIT":
            status.style.color = "#f59e0b";
            statusCard.style.borderColor="#f59e0b";
            break;

        case "IMPACT":
            status.style.color = "#ef4444";
            statusCard.style.borderColor="#ef4444";
            break;

        case "FALL":
            status.style.color = "#dc2626";
            statusCard.style.borderColor="#dc2626";
            break;

        default:
            status.style.color = "#0077cc";
            statusCard.style.borderColor="#0077cc";
    }

    document.getElementById("orientation").textContent = d.orientation;
    document.getElementById("pitch").textContent = d.pitch + "°";
    document.getElementById("roll").textContent = d.roll + "°";
    document.getElementById("magnitude").textContent = d.magnitude + " g";
}
     update(); /* To ensure that when the website 1st opens, it updates fast without waiting*/
     setInterval(update,200);
   </script>
   </body>
   
   </html>
   )rawliteral";
   server.send(200,"text/html",page);//Receptionist - tells the esp32 if any requests have come
}

void handleData(){
  /*JSON BASICS
   * json - form
   * sprintf - person filling the form
   * server.send()-person taking the form to the post office
   */
   
  //Create character array to store json string
  char json[200]; //create enough memory to store all variables
  //sprintf(destination, "template", values)
  sprintf(json,
  "{\"pitch\":%.1f," // \" is used to explicitly display the quotes
  "\"roll\":%.1f,"
  "\"magnitude\":%.2f,"
  "\"orientation\":\"%s\","
  "\"status\":\"%s\"}",pitch,roll,raw,orientation,motionState);
  //Close the connection after every response to avoid hanging of website
  server.sendHeader("Connection","close");
  //Sends json string to webpage
  //server.send(statusCode(200 for successfull request processing),contentType,data)
  server.send(200,"application/json",json);
}

void readSensor(){
  //_t - naming convention - is used to mention that it is a data type
  int16_t ax,ay,az; //MPU6050 stores 16bit signed integer values
  int16_t gx,gy,gz; //1g is stored as 16384
  mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz); //It fills 6 variables instead of just returning 6 values
  //reads all 6 values in 1 I2C connection
  //&ax provides memory address. It allows getmotion6() to directly store sensor readings into the variable instead of returning it
  float gX=ax/16384.0;
  float gY=-(ay/16384.0);
  float gZ=az/16384.0;
  raw=sqrt(gX*gX + gY*gY + gZ*gZ);//gives overall acceleration cause impact can occur from anyside
  
  pitch = atan2(gX,sqrt(gY*gY + gZ*gZ))*180.0/PI;
  roll = atan2(gY,sqrt(gX*gX + gZ*gZ))*180.0/PI;
  
  if(pitch>45)
  strcpy(orientation,"FRONT FACE DOWN");
  else if(pitch<-45)
  strcpy(orientation,"REAR FACE DOWN");
  else if(roll>45)
  strcpy(orientation,"RIGHT FACE DOWN");
  else if(roll<-45)
  strcpy(orientation,"LEFT FACE DOWN");
  else if(gZ<-0.5)
  strcpy(orientation,"UPSIDE DOWN");
  else
  strcpy(orientation,"UPRIGHT");

// 1. Detect possible free fall
if (raw < FALL_THRESHOLD) {
    possibleFall = true;
    fallTime = millis();
}

// 2. Detect impact
if (raw > IMPACT_THRESHOLD) {

    // Was there a possible fall recently?
    if (possibleFall && (millis() - fallTime <= FALL_WINDOW)) {

        strcpy(motionState, "FALL");
        alertTime=millis();

        // Reset so the same fall isn't detected again
        possibleFall = false;
    }
    else {

        strcpy(motionState, "IMPACT");
        alertTime = millis();
    }
}

// 3. If no impact, but an old possible fall expired
if (possibleFall && (millis() - fallTime > FALL_WINDOW)) {
    possibleFall = false;
}

// 4. Normal states (only if we're not holding IMPACT/FALL)
  if(millis() - alertTime > ALERT_HOLD){
    if (raw>= STILL_MIN && raw<= STILL_MAX)
    strcpy(motionState, "STABLE");
    else
    strcpy(motionState,"IN TRANSIT");
  }
}
 void setup() {
Serial.begin(115200);

WiFi.begin(ssid,password); //start connection, doesnt wait
Serial.print("Connecting to WiFi");
while(WiFi.status()!=WL_CONNECTED){
  delay(500);
  Serial.print(".");
}
Serial.println();
Serial.print("IP Address: ");
Serial.println(WiFi.localIP());

Wire.begin(21,22); //2 pins on esp32, sda-data, scl-clock
delay(200);//providing small break for the sensor b4 using it
mpu.initialize();
Serial.println("MPU6050 Initialized");

server.on("/",handleRoot);
server.on("/data",handleData);
/*If the web server ends in /, handleroot. 
If the web server ends in /data, handledata*/
server.begin(); //opens the server ie shop
Serial.println("Server started");
}

void loop() {
  readSensor();
Serial.print("Magnitude: ");
Serial.println(raw);
  server.handleClient();

 Serial.print("Pitch: ");
Serial.print(pitch);
Serial.print("  Roll: ");
Serial.print(roll);
Serial.print("  Raw: ");
Serial.println(raw);

delay(20);
}
