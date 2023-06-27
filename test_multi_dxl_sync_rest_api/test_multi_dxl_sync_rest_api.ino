#include <stdlib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include <Dynamixel2Arduino.h>

const char *SSID = "NETGEAR16";
const char *PWD = "cloudywindow854";

#define DXL_SERIAL   Serial1
const uint8_t BROADCAST_ID = 254;                       //broadcast address
const int DXL_DIR_PIN = 2;                              // DYNAMIXEL Shield DIR PIN
const uint8_t DXL_ID_CNT = 6;                           // Number of motors
const uint8_t DXL_ID_LIST[DXL_ID_CNT] = {1,2,3,4,5,6};  // List of addresses
const uint16_t user_pkt_buf_cap = 128;                  // length of buffer of transmission
uint8_t user_pkt_buf[user_pkt_buf_cap];                 // buffer of transmission
const float DXL_PROTOCOL_VERSION = 2.0;                 // version of protocol (2.0 for XL-320)

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
float order[7] = {200,512,512,512,512,512,512};
long t,t0;
float pos0[6] = {512,512,512,512,512,512};
float posm[6] = {512,512,512,512,512,512};
float pos_t[6] = {512,512,512,512,512,512};
uint8_t i;
uint8_t recv_cnt;

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
  if (server.hasArg("plain") == true) {
    String body = server.arg("plain");
    for(i = 0; i < DXL_ID_CNT; i++){
      pos0[i]=posm[i];
    }
    t0=millis();
    stringToFloatArray(body, order, 7);
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
  dxl.begin(115200);
  dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);
  
  for(i = 0; i < DXL_ID_CNT; i++){
    //dxl.reboot(DXL_ID_LIST[i], 10);
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
  sw_infos.is_info_changed = false;
  for(i = 0; i < DXL_ID_CNT; i++){
    posm[i] = dxl.getPresentPosition(DXL_ID_LIST[i]);
    //Serial.print(DXL_ID_LIST[i]);Serial.print(":M:");Serial.println(posm[i]);
    if(t<t0+order[0]*10){
      pos_t[i] = pos0[i]+(t-t0)*(order[i+1]-pos0[i])/(order[0]*10);
    }else{
      pos_t[i] = order[i+1];
    }
    dxl.setGoalPosition(DXL_ID_LIST[i], pos_t[i]);
    //Serial.print(DXL_ID_LIST[i]);Serial.print(":C:");Serial.println(pos_t[i]);
//    sw_data[i].goal_position = pos_t[i];
//    sw_infos.is_info_changed = true;
  }

  // Update the SyncWrite packet status
//  sw_infos.is_info_changed = true;
//  if(pos_t>0){
//    dxl.syncWrite(&sw_infos);
//  }
//  else{
//    dxl.torqueOff(BROADCAST_ID);
//  }
  //Serial.print(DXL_ID_LIST[i]);Serial.print(":M:");Serial.println(posm[i]);
  //delay(50);
//  recv_cnt = dxl.syncRead(&sr_infos);
//  if(recv_cnt > 0) {
//    for(i = 0; i<recv_cnt; i++){
//        posm[i] = sr_data[i].present_position;
//        //Serial.print(DXL_ID_LIST[i]);Serial.print(":M:");Serial.println(posm[i]);
//    }
//  }
//  delay(10);
}
