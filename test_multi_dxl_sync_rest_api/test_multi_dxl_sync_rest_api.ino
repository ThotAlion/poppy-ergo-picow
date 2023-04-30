#include <stdlib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include <Dynamixel2Arduino.h>

const char *SSID = "NETGEAR16";
const char *PWD = "cloudywindow854";

#define DXL_SERIAL   Serial1
const uint8_t BROADCAST_ID = 254;
const int DXL_DIR_PIN = 2; // DYNAMIXEL Shield DIR PIN
const uint8_t DXL_ID_CNT = 1;
const uint8_t DXL_ID_LIST[DXL_ID_CNT] = {1};
const uint16_t user_pkt_buf_cap = 128;
uint8_t user_pkt_buf[user_pkt_buf_cap];
const float DXL_PROTOCOL_VERSION = 2.0;

// Starting address of the Data to read; Present Position = 37
const uint16_t SR_START_ADDR = 37;
// Length of the Data to read; Length of Position data of X series is 2 byte
const uint16_t SR_ADDR_LEN = 2;
// Starting address of the Data to write; Goal Position = 30
const uint16_t SW_START_ADDR = 30;
// Length of the Data to write; Length of Position data of X series is 2 byte
const uint16_t SW_ADDR_LEN = 2;
typedef struct sr_data{
  int32_t present_position;
} __attribute__((packed)) sr_data_t;
typedef struct sw_data{
  int32_t goal_position;
} __attribute__((packed)) sw_data_t;

sr_data_t sr_data[DXL_ID_CNT];
DYNAMIXEL::InfoSyncReadInst_t sr_infos;
DYNAMIXEL::XELInfoSyncRead_t info_xels_sr[DXL_ID_CNT];

sw_data_t sw_data[DXL_ID_CNT];
DYNAMIXEL::InfoSyncWriteInst_t sw_infos;
DYNAMIXEL::XELInfoSyncWrite_t info_xels_sw[DXL_ID_CNT];

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);
//This namespace is required to use DYNAMIXEL Control table item name definitions
using namespace ControlTableItem;

bool led = 1;
float pos[7];
long t,t0;
float pos0,posm;
float pos_t;
uint8_t i;
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

// Function to split a string and store the integers in an array
void stringToFloatArray(String inputString, float outputArray[], int arraySize) {
  int index = 0;
  String number = "";
  
  for (int i = 0; i < inputString.length(); i++) {
    char currentChar = inputString.charAt(i);
    
    if (currentChar == ',') {
      outputArray[index] = number.toFloat();
      number = "";
      index++;
    } else {
      number += currentChar;
    }
  }
  
  outputArray[index] = number.toFloat(); // Store the last number
}

void setServo() {
  if (server.hasArg("plain") == false) {
    //Serial.println("no plain arg");
  }else{
    String body = server.arg("plain");
    pos0=posm;
    stringToFloatArray(body, pos, 7);
    t0=millis();
    
    server.send(200, "application/json", "{}");
  }
}

void loop() {
  server.handleClient();
}

void setup1(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  Serial1.setRX(1);
  Serial1.setTX(0);
  dxl.begin(1000000);
  dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);
  for(i = 0; i < DXL_ID_CNT; i++){
    dxl.torqueOn(BROADCAST_ID);
    dxl.ping(DXL_ID_LIST[i]);
    dxl.torqueOff(DXL_ID_LIST[i]);
    dxl.setOperatingMode(DXL_ID_LIST[i], OP_POSITION);
    dxl.torqueOn(DXL_ID_LIST[i]);
    dxl.writeControlTableItem(MOVING_SPEED, DXL_ID_LIST[i], 0);
    dxl.writeControlTableItem(LED, DXL_ID_LIST[i], 4);
    dxl.writeControlTableItem(P_GAIN, DXL_ID_LIST[i], 32);
    dxl.writeControlTableItem(I_GAIN, DXL_ID_LIST[i], 0);
    dxl.writeControlTableItem(D_GAIN, DXL_ID_LIST[i], 0);
  }
  
  
  // Fill the members of structure to syncRead using external user packet buffer
  sr_infos.packet.p_buf = user_pkt_buf;
  sr_infos.packet.buf_capacity = user_pkt_buf_cap;
  sr_infos.packet.is_completed = false;
  sr_infos.addr = SR_START_ADDR;
  sr_infos.addr_length = SR_ADDR_LEN;
  sr_infos.p_xels = info_xels_sr;
  sr_infos.xel_count = 0;  

  for(i = 0; i < DXL_ID_CNT; i++){
    info_xels_sr[i].id = DXL_ID_LIST[i];
    info_xels_sr[i].p_recv_buf = (uint8_t*)&sr_data[i];
    sr_infos.xel_count++;
  }
  sr_infos.is_info_changed = true;

  // Fill the members of structure to syncWrite using internal packet buffer
  sw_infos.packet.p_buf = nullptr;
  sw_infos.packet.is_completed = false;
  sw_infos.addr = SW_START_ADDR;
  sw_infos.addr_length = SW_ADDR_LEN;
  sw_infos.p_xels = info_xels_sw;
  sw_infos.xel_count = 0;

  for(i = 0; i < DXL_ID_CNT; i++){
    info_xels_sw[i].id = DXL_ID_LIST[i];
    info_xels_sw[i].p_data = (uint8_t*)&sw_data[i].goal_position;
    sw_infos.xel_count++;
  }
  sw_infos.is_info_changed = true;
}

void loop1(){
  t = millis();
  if(t<t0+pos[0]*10){
    pos_t = pos0+(t-t0)*(pos[1]-pos0)/(pos[0]*10);
  }else{
    pos_t = pos[1];
  }
  // put your main code here, to run repeatedly:
  static uint32_t try_count = 0;
  uint8_t i, recv_cnt;
  
  // Insert a new Goal Position to the SyncWrite Packet
  for(i = 0; i < DXL_ID_CNT; i++){
    sw_data[i].goal_position = pos_t;
  }

  // Update the SyncWrite packet status
  sw_infos.is_info_changed = true;
  
  dxl.syncWrite(&sw_infos);
  recv_cnt = dxl.syncRead(&sr_infos);
  if(recv_cnt > 0) {
    for(i = 0; i<recv_cnt; i++){
        posm = sr_data[i].present_position;
    }
  }
  delay(10);
}
