#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Servo.h>

const char *SSID = "NETGEAR16";
const char *PWD = "cloudywindow854";

Servo myservo;

bool led = 1;
StaticJsonDocument<250> jsonDocument;
char buffer[250];
void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);  
}
 
void add_json_object(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}

WebServer server(8080);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  myservo.attach(17);
  myservo.writeMicroseconds(1500);
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(SSID);
  WiFi.setTimeout(15000);
  WiFi.begin(SSID, PWD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    delay(500);
  }
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, 1);
  
  server.enableCORS();
  server.on("/led", getLed);
  server.on("/setled", HTTP_POST, setLed);
  server.on("/setservo", HTTP_POST, setServo);
 
  // start server
  server.begin();
}

void getLed() {
  //Serial.println("Get LED state");
  create_json("st", digitalRead(LED_BUILTIN),"su");
  server.send(200, "application/json", buffer);
}

void setLed() {
  if (server.hasArg("plain") == false) {
    //Serial.println("no plain arg");
  }else{
    String body = server.arg("plain");
    //Serial.println(body);
    if(body=="1"){
      digitalWrite(LED_BUILTIN, 1);
    }else if(body=="0"){
      digitalWrite(LED_BUILTIN, 0);
    }
    server.send(200, "application/json", "{}");
  }
}

void setServo() {
  if (server.hasArg("plain") == false) {
    //Serial.println("no plain arg");
  }else{
    String body = server.arg("plain");
    
    server.send(200, "application/json", "{}");
  }
}

void read_sensor_data() {
     myservo.writeMicroseconds(1500+100*sin(millis()/1000));
}

void loop() {
  server.handleClient();
  read_sensor_data();
}
