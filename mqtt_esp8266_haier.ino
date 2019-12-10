#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

const char* ssid = "ssid"; // WIFI SSID
const char* password = "password"; // WIFI Password
const char* mqtt_server = "mosquittoip"; // MQTT Server
const char* mqtt_user = "mosquitto"; // MQTT user
const char* mqtt_password = "mosquitto"; // MQTT password

IPAddress ip(10,0,0,202); // Static IP
IPAddress gateway(10,0,0,1); // Gateway
IPAddress subnet(255,255,255,0); // Subnet Mask

WiFiClient espClient;
PubSubClient client(espClient);

#define Name_ID "Haier_Camera"
#define LED     2

#define B_CUR_TMP   13  // current temperature
#define B_SET_TMP   35  // set temperature
#define B_CMD       17  // 00 - command, 7F - reply
#define B_MODE      23  // 00 - smart, 01 - cool, 02 - heat, 03 - fan_only, 04 - dry
#define B_FAN_SPD   25  // 00 - max, 01 - mid, 02 - min, 03 - silent
#define B_SWING     27  // 00 - off, 01 - vertical,  02 - horizontal, 03 - on
#define B_LOCK_REM  28  // 00 - unlocked, 80 locked
#define B_POWER     29  // 00 - off, 01 - on, 09 - ionizer, 10 11 - kondensor 
#define B_FRESH     31  // 00 - off, 01 - on 

int fresh; // not on Nebula
int power;
int swing;
int lock_rem;
int cur_tmp;
int set_tmp;
int fan_spd;
int Mode;
int noise;
int light;
int health;
int health_airflow;
int compressor;

bool firsttransfer = true;
bool transferinprog = false;
int unclean = 0;
long prev = 0;

byte data[37] = {};
byte olddata[37] = {};
byte packdata[37] = {};
byte qstn[] = {255,255,10,0,0,0,0,0,1,1,77,1,90}; // polling command
byte on[]   = {255,255,10,0,0,0,0,0,1,1,77,2,91}; // turn on AC
byte off[]  = {255,255,10,0,0,0,0,0,1,1,77,3,92}; // turn off AC
byte lock[] = {255,255,10,0,0,0,0,0,1,3,0,0,14};  // remote lock

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED, !digitalRead(LED));
  }
  digitalWrite(LED, HIGH);
}

void reconnect() {
  while (!client.connected()) {
    digitalWrite(LED, !digitalRead(LED));
    if (client.connect(Name_ID, mqtt_user, mqtt_password)) {
      client.subscribe("Haier/Camera/set/set_tmp");
      client.subscribe("Haier/Camera/set/Mode");
      client.subscribe("Haier/Camera/set/fan_spd");
      client.subscribe("Haier/Camera/set/swing");
      client.subscribe("Haier/Camera/set/lock_rem");
      client.subscribe("Haier/Camera/set/power");
      client.subscribe("Haier/Camera/set/ionizer");
      client.subscribe("Haier/Camera/set/fresh");
      client.subscribe("Haier/Camera/set/light");
      client.subscribe("Haier/Camera/set/airflow");
      client.subscribe("Haier/Camera/set/turbo");
      client.subscribe("Haier/Camera/set/quiet");
      
    } else {
      delay(5000);
    }
  }
  digitalWrite(LED, HIGH);
}

void InsertData(byte olddata[], size_t size){
  byte mqdata[37] = {};
  for(int bajt = 0; bajt < size; bajt++){
    mqdata[bajt] = olddata[bajt];
  }
    set_tmp = mqdata[B_SET_TMP]+16;
    cur_tmp = mqdata[B_CUR_TMP];
    Mode = mqdata[B_MODE];
    fan_spd = mqdata[B_FAN_SPD];
    swing = mqdata[B_SWING];
    power = (int)bitRead( mqdata[B_POWER], 0);
    lock_rem = mqdata[B_LOCK_REM];
    fresh = (int)bitRead( mqdata[B_FRESH], 0);
    light = (int)bitRead( mqdata[B_FRESH], 5);
    noise = (int)bitRead( mqdata[B_FRESH], 1) + 2*(int)bitRead( mqdata[B_FRESH], 2);
    health = (int)bitRead( mqdata[B_POWER], 3);
    health_airflow = (int)bitRead( mqdata[B_FRESH], 3) + 2*(int)bitRead( mqdata[B_FRESH], 4);
    compressor = (int)bitRead( mqdata[B_POWER], 4);
    
    
  /////////////////////////////////
  if (fresh == 0x00){
      client.publish("Haier/Camera/get/fresh", "off");
  }
  if (fresh == 0x01){
      client.publish("Haier/Camera/get/fresh", "on");
  }
  /////////////////////////////////
  if (light == 0x00){
      client.publish("Haier/Camera/get/light", "on");
  }
  if (light == 0x01){
      client.publish("Haier/Camera/get/light", "off");
  }
  /////////////////////////////////
  if (noise == 0x00){
      client.publish("Haier/Camera/get/quiet", "off");
      client.publish("Haier/Camera/get/turbo", "off");
  }
  if (noise == 0x01){
      client.publish("Haier/Camera/get/quiet", "off");
      client.publish("Haier/Camera/get/turbo", "on");
  }
  if (noise == 0x02){
      client.publish("Haier/Camera/get/quiet", "on");
      client.publish("Haier/Camera/get/turbo", "off");
  }
  if (noise == 0x03){ //WTF??
      client.publish("Haier/Camera/get/quiet", "on");
      client.publish("Haier/Camera/get/turbo", "on");
  }  
  /////////////////////////////////
  if (health == 0x00){
      client.publish("Haier/Camera/get/ionizer", "off");
  }
  if (health == 0x01){
      client.publish("Haier/Camera/get/ionizer", "on");
  }
  /////////////////////////////////
  if (health_airflow == 0x00){
      client.publish("Haier/Camera/get/airflow", "off");
  }
  if (health_airflow == 0x01){
      client.publish("Haier/Camera/get/airflow", "up");
  }
  if (health_airflow == 0x02){
      client.publish("Haier/Camera/get/airflow", "down");
  }
  if (health_airflow == 0x03){ // WTF??
      client.publish("Haier/Camera/get/airflow", "unknown");
  }
  /////////////////////////////////
  if (lock_rem == 0x80){
      client.publish("Haier/Camera/get/lock_rem", "true");
  }
  if (lock_rem == 0x00){
      client.publish("Haier/Camera/get/lock_rem", "false");
  }
  /////////////////////////////////
  if (compressor == 0x00){
      client.publish("Haier/Camera/get/Compressor", "off");
  }
  if (compressor == 0x01){
      client.publish("Haier/Camera/get/Compressor", "on");
  }
  /////////////////////////////////
  if (power == 0x01){
      client.publish("Haier/Camera/get/power", "on");
  }
  if (power == 0x00){
      client.publish("Haier/Camera/get/power", "off");
  }
  /////////////////////////////////
  if (swing == 0x00){
      client.publish("Haier/Camera/get/swing", "off");
  }
  if (swing == 0x01){
      client.publish("Haier/Camera/get/swing", "vertical");
  }
  if (swing == 0x02){
      client.publish("Haier/Camera/get/swing", "horizontal");
  }
  if (swing == 0x03){
      client.publish("Haier/Camera/get/swing", "on");
  }
  /////////////////////////////////  
  if (fan_spd == 0x00){
      client.publish("Haier/Camera/get/fan_spd", "high");
  }
  if (fan_spd == 0x01){
      client.publish("Haier/Camera/get/fan_spd", "medium");
  }
  if (fan_spd == 0x02){
      client.publish("Haier/Camera/get/fan_spd", "low");
  }
  if (fan_spd == 0x03){
      client.publish("Haier/Camera/get/fan_spd", "auto");
  }
  /////////////////////////////////
  char b[5]; 
  String char_set_tmp = String(set_tmp);
  char_set_tmp.toCharArray(b,5);
  client.publish("Haier/Camera/get/set_tmp", b);
  ////////////////////////////////////
  String char_cur_tmp = String(cur_tmp);
  char_cur_tmp.toCharArray(b,5);
  client.publish("Haier/Camera/get/cur_tmp", b);
  ////////////////////////////////////
  if (power == 0x00){
    client.publish("Haier/Camera/get/Mode", "off");
  }
  else{
    if (Mode == 0x00){
        client.publish("Haier/Camera/get/Mode", "smart");
    }
    if (Mode == 0x01){
        client.publish("Haier/Camera/get/Mode", "cool");
    }
    if (Mode == 0x02){
        client.publish("Haier/Camera/get/Mode", "heat");
    }
    if (Mode == 0x03){
        client.publish("Haier/Camera/get/Mode", "fan_only");
    }
    if (Mode == 0x04){
        client.publish("Haier/Camera/get/Mode", "dry");
    }
  }
  
  String raw_str;
  char raw[75];
  for (int i=0; i < 37; i++){
     if (mqdata[i] < 0x10){
       raw_str += "0";
       raw_str += String(mqdata[i], HEX);
     } else {
      raw_str += String(mqdata[i], HEX);
     }    
  }
  raw_str.toUpperCase();
  raw_str.toCharArray(raw,75);
  client.publish("Haier/Camera/get/RAW", raw);
  
///////////////////////////////////
}

byte getCRC(byte req[], size_t size){
  byte crc = 0;
  for (int i=2; i < size; i++){
      crc += req[i];
  }
  return crc;
}

void SendData(byte req[], size_t size){
  //Serial.write(start, 2);
  Serial.write(req, size - 1);
  Serial.write(getCRC(req, size-1));
}

inline unsigned char toHex( char ch ){
   return ( ( ch >= 'A' ) ? ( ch - 'A' + 0xA ) : ( ch - '0' ) ) & 0x0F;
}

void callback(char* topic, byte* payload, unsigned int length) {
  bool kontynnuj = false;
  if (firsttransfer){
    return;
  }
  if(unclean == 0){
  for (int iter = 0; iter < 6; iter++){
    for (int bajt = 0; bajt < 37; bajt++){
      packdata[bajt] = olddata[bajt];
    }
    if(getCRC(packdata, (sizeof(packdata)/sizeof(byte))-1) == packdata[36]){
      kontynnuj = true;
    }
    if(kontynnuj){
      break;
    }
  }
  }
  else{
    kontynnuj = true;
  }
  if(kontynnuj){
  payload[length] = '\0';
  String strTopic = String(topic);
  String strPayload = String((char*)payload);
  ///////////
  if (strTopic == "Haier/Camera/set/set_tmp"){
    if ((strPayload.toInt()-16) >= 0 && (strPayload.toInt()-16) <= 30){
      packdata[B_SET_TMP] = strPayload.toInt()-16;      
    }
  }
  //////////
  if (strTopic == "Haier/Camera/set/Mode"){
    int powerstate = power = (int)bitRead( packdata[B_POWER], 0);
    if (strPayload == "off"){
      SendData(off, sizeof(off)/sizeof(byte));
      return;
    }
    if (strPayload == "smart"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 0; 
    }
    if (strPayload == "cool"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 1;
    }
    if (strPayload == "heat"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 2; 
    }
    if (strPayload == "fan_only"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 3;
    }
    if (strPayload == "dry"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 4;
    }
  }
  //////////
  if (strTopic == "Haier/Camera/set/fan_spd"){
    if (strPayload == "high"){
        packdata[B_FAN_SPD] = 0; 
        bitClear( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
    }
    if (strPayload == "medium"){
        packdata[B_FAN_SPD] = 1;
        bitClear( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
    }
    if (strPayload == "low"){
        packdata[B_FAN_SPD] = 2; 
        bitClear( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
    }
    if (strPayload == "auto"){
        packdata[B_FAN_SPD] = 3; 
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/swing"){
    if (strPayload == "off"){
        packdata[B_SWING] = 0; 
    }
    if (strPayload == "vertical"){
        packdata[B_SWING] = 1;
        bitClear( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
    }
    if (strPayload == "horizontal"){
        packdata[B_SWING] = 2; 
    }
    if (strPayload == "on"){
        packdata[B_SWING] = 3; 
        bitClear( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/lock_rem"){
    if (strPayload == "true"){
        packdata[B_LOCK_REM] = 80;
    }
    if (strPayload == "false"){
        packdata[B_LOCK_REM] = 0;
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/power"){
    if (strPayload == "off" || strPayload == "false" || strPayload == "0"){
      SendData(off, sizeof(off)/sizeof(byte));
      return;
    }
    if (strPayload == "on" || strPayload == "true" || strPayload == "1"){
      SendData(on, sizeof(on)/sizeof(byte));
      return;
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/ionizer"){
    if (strPayload == "on"){
        bitSet( packdata[B_POWER], 3);
    }
    if (strPayload == "off"){
        bitClear( packdata[B_POWER], 3);
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/fresh"){
    if (strPayload == "on"){
        bitSet( packdata[B_FRESH], 0);
    }
    if (strPayload == "off"){
        bitClear( packdata[B_FRESH], 0);
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/light"){
    if (strPayload == "off"){
      bitSet( packdata[B_FRESH], 5);
    }
    if (strPayload == "on"){
      bitClear( packdata[B_FRESH], 5);
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/airflow"){
    if (strPayload == "off"){
        bitClear( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
    }
    if (strPayload == "up"){
        bitClear( packdata[B_FRESH], 4);
        bitSet( packdata[B_FRESH], 3);
        if(packdata[B_SWING] == 1){
          packdata[B_SWING] = 0;
        }
        if(packdata[B_SWING] == 3){
          packdata[B_SWING] = 2;
        }
    }
    if (strPayload == "down"){
        bitSet( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
        if(packdata[B_SWING] == 1){
          packdata[B_SWING] = 0;
        }
        if(packdata[B_SWING] == 3){
          packdata[B_SWING] = 2;
        }
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/turbo"){
    if (strPayload == "on"){
        bitSet( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
        packdata[B_FAN_SPD] = 3;
    }
    if (strPayload == "off"){
      
        bitClear( packdata[B_FRESH], 1);
    }
  }
  ////////
  if (strTopic == "Haier/Camera/set/quiet"){
    if (strPayload == "on"){
        bitSet( packdata[B_FRESH], 2);
        bitClear( packdata[B_FRESH], 1);
        packdata[B_FAN_SPD] = 3;
    }
    if (strPayload == "off"){
      
        bitClear( packdata[B_FRESH], 2);
    }
  }
  ////////
  bool wyslac = false;
  for (int bajt=0; bajt < 37; bajt++){
    if (packdata[bajt] != olddata[bajt]){
      wyslac = true;
    }
  }
  packdata[B_CMD] = 0;
  packdata[9] = 1;
  packdata[10] = 77;
  packdata[11] = 95;
  
  if(wyslac){
    SendData(packdata, sizeof(packdata)/sizeof(byte));
    if (transferinprog){
      unclean = 2;
    }
    else{
      unclean = 1;
    }
  }
  }
}

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  ArduinoOTA.setHostname(Name_ID);
  ArduinoOTA.onStart([]() {  });
  ArduinoOTA.onEnd([]() {  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {  });
  ArduinoOTA.onError([](ota_error_t error) {  });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  if(Serial.available() > 0){
    transferinprog = true;
    Serial.readBytes(data, 37);
    while(Serial.available()){
      delay(2);
      Serial.read();
    }
    transferinprog = false;
    if(getCRC(data, (sizeof(data)/sizeof(byte))-1) == data[36]){
      bool nowy = false;
      for(int bajt = 0; bajt < 37; bajt++){
        if(data[bajt] != olddata[bajt]){
          nowy = true;
        }
      }
      if(nowy){
        for(int bajt = 0; bajt < 37; bajt++){
          olddata[bajt] = data[bajt];
        }
        InsertData(olddata, 37);
      }
      firsttransfer = false;
      if (unclean == 2){
        unclean = 1;
      }
      if (unclean == 1){
        unclean = 0;
      }
    }
  }
  
  if (!client.connected()){
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - prev > 5000) {
    prev = now;
    SendData(qstn, sizeof(qstn)/sizeof(byte)); // polling
  }
  delay(500);
}
