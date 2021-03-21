#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include "bitmap.h"
#include "webpage.h"

#define __DEBUG__ true // true to print debug info to use debuger

/* 本代码适用于ESP8266 NodeMCU + 12864显示屏
7pin SPI引脚，正面看，从左到右依次为GND、VCC、D0、D1、RES、DC、CS
   ESP8266 ---  OLED
     3V    ---  VCC
     G     ---  GND
     D7    ---  D1
     D5    ---  D0
     D2orD8---  CS
     D1    ---  DC
     RST   ---  RES
4pin IIC引脚，正面看，从左到右依次为GND、VCC、SCL、SDA
     OLED  ---  ESP8266
     VCC   ---  3.3V
     GND   ---  G (GND)
     SCL   ---  D1(GPIO5)
     SDA   ---  D2(GPIO4)
*/

U8G2_SSD1305_128X64_ADAFRUIT_1_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 4, /* dc=*/ 5, /* reset=*/ 3); // working
// U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 4, /* dc=*/ 5, /* reset=*/ 3);	// Arduboy (Production, Kickstarter Edition)

const char* AP_NAME = "SubMonitor";//wifi名字
//暂时存储wifi账号密码
String sta_ssid = String();
String sta_password = String();

const byte DNS_PORT = 53;//DNS端口号
IPAddress apIP(192, 168, 4, 1);//esp8266-AP-IP地址
DNSServer dnsServer;//创建dnsServer实例
ESP8266WebServer server(80);//创建WebServer

const unsigned long HTTP_TIMEOUT = 5000;
HTTPClient http;
String response;
String biliuid = "435203110";
int follower = 0;
uint8 index_counter = 0;

void handleRoot();        //访问主页回调函数
void handleRootPost();    //Post回调函数
void initBasic(void);     //初始化基础
void initSoftAP(void);    //初始化AP模式
void initWebServer(void); //初始化WebServer
void initDNS(void);       //初始化DNS服务器
void connectNewWifi(void);
bool getJson();
bool parseJson(String json);
String readFile(fs::FS &fs, const char * path);
bool writeFile(fs::FS &fs, const char * path, const char * message);
bool readFileCheck();
void PrintWiFiStatus();
void darwBilibili(uint8 index);
void drawSubs();

template<typename T>
void DEBUG(const T& s);

template<typename T>
void DEBUGLN(const T& s);


void setup() {
  initBasic();
  connectNewWifi();
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();

  if (WiFi.status() == WL_CONNECTED){
    if (getJson()){
      if (parseJson(response))
          DEBUGLN("follower: " +  String(follower));
    }
    drawSubs();
    delay(2800);
  }else{
    DEBUGLN("[WiFi] Waiting to reconnect...");
    PrintWiFiStatus();
  }
  delay(200);

}

void handleRoot() {//访问主页回调函数
  server.send(200, "text/html", page_html);
}

void handleRootPost() {//Post回调函数
  DEBUGLN("handleRootPost");
  if (server.hasArg("ssid")) {//判断是否有账号参数
    DEBUG("got ssid:");
    // strcpy(sta_ssid, server.arg("ssid").c_str());//将账号参数拷贝到sta_ssid中
    sta_ssid = server.arg("ssid");//将账号参数拷贝到sta_ssid中
    DEBUGLN("Writing ssid to file ssid.txt" + sta_ssid);
    if (writeFile(SPIFFS, "/ssid.txt", sta_ssid.c_str()))
      DEBUGLN("handleRootPost: successfully write ssid.txt");
    else
      DEBUGLN("handleRootPost: fail to write ssid.txt");
  } else {//没有参数
    DEBUGLN("error, not found ssid");
    server.send(200, "text/html", "<meta charset='UTF-8'>error, not found ssid");//返回错误页面
    return;
  }
  //密码与账号同理
  if (server.hasArg("password")) {
    DEBUG("got password:");
    // strcpy(sta_password, server.arg("password").c_str());
    sta_password = server.arg("password");
    DEBUGLN("Writing ssid to file password.txt" + sta_password);
    if (writeFile(SPIFFS, "/password.txt", sta_password.c_str()))
      DEBUGLN("handleRootPost: successfully write password.txt");
    else
      DEBUGLN("handleRootPost: fail to write password.txt");
  } else {
    DEBUGLN("error, not found password");
    server.send(200, "text/html", "<meta charset='UTF-8'>error, not found password");
    return;
  }

  // temporarily hardcode biliuid.
  // TBD: pull avatar icon and name and fix the issue of 
  // some chinese characters cannot display (missing unicode mapping)

  // if (server.hasArg("BilibiliID")) {
  //   DEBUG("got Bilibili ID:");
  //   biliuid = server.arg("BilibiliID");
  //   DEBUGLN("Writing ssid to file biliuid.txt" + biliuid);
  //   if (writeFile(SPIFFS, "/biliuid.txt", biliuid.c_str()))
  //     DEBUGLN("handleRootPost: successfully write biliuid.txt");
  //   else
  //     DEBUGLN("handleRootPost: successfully write biliuid.txt");
  // } else {
  //   DEBUGLN("error, not found biliuid");
  //   server.send(200, "text/html", "<meta charset='UTF-8'>error, not found Bilibili ID");
  //   return;
  // }

  server.send(200, "text/html", "<meta charset='UTF-8'>保存成功");//返回保存成功页面
  delay(2000);
  //连接wifi
  connectNewWifi();
}

void initBasic(void){//初始化基础
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_wqy12_t_chinese1);
  WiFi.hostname("Smart-ESP8266");//设置ESP8266设备名
  if(!SPIFFS.begin()){
    DEBUGLN("An Error has occurred while mounting SPIFFS");
    return;
  }
}

void initSoftAP(void){//初始化AP模式
  PrintWiFiStatus();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if(WiFi.softAP(AP_NAME)){
    DEBUGLN("ESP8266 SoftAP is right");
  }
}

void initWebServer(void){//初始化WebServer
  //server.on("/",handleRoot);
  //上面那行必须以下面这种格式去写否则无法强制门户
  server.on("/", HTTP_GET, handleRoot);//设置主页回调函数
  server.onNotFound(handleRoot);//设置无法响应的http请求的回调函数
  server.on("/", HTTP_POST, handleRootPost);//设置Post请求回调函数
  server.begin();//启动WebServer
  DEBUGLN("WebServer started!");
}

void initDNS(void){//初始化DNS服务器
  if(dnsServer.start(DNS_PORT, "*", apIP)){//判断将所有地址映射到esp8266的ip上是否成功
    DEBUGLN("start dnsserver success.");
  }
  else DEBUGLN("start dnsserver failed.");
}

void connectNewWifi(void){
  if (readFileCheck()) {
    WiFi.mode(WIFI_STA);//切换为STA模式
    WiFi.setAutoConnect(true);//设置自动连接
    WiFi.begin(sta_ssid, sta_password);//连接上一次连接成功的wifi
    DEBUG("\nConnect to wifi");
    int count = 0;
    while (WiFi.status() != WL_CONNECTED) {
      // unsigned long connect_time = millis();
      darwBilibili(index_counter++ % 5);
      delay(200);
      count++;
      if(count > 100){//如果20秒内没有连上，就开启Web配网 可适当调整这个时间
        initSoftAP();
        initWebServer();
        initDNS();
        break;//跳出 防止无限初始化
      }
      DEBUG(".");
    }
    DEBUGLN("");
    if(WiFi.status() == WL_CONNECTED){//如果连接上 就输出IP信息 防止未连接上break后会误输出
      DEBUGLN("WIFI Connected!");
      DEBUG("IP address: ");
      DEBUGLN(WiFi.localIP());//打印esp8266的IP地址
      server.stop();
    }
  } else {
    initSoftAP();
    initWebServer();
    initDNS();
  }
}

bool getJson()
{
  bool r = false;
  http.setTimeout(HTTP_TIMEOUT);
  http.begin("http://api.bilibili.com/x/relation/stat?vmid=" + biliuid);
  int httpCode = http.GET();
  if (httpCode > 0){
      if (httpCode == HTTP_CODE_OK){
          response = http.getString();
          //DEBUGLN(response);
          r = true;
      }
  }else{
      Serial.printf("[HTTP] GET JSON failed, error: %s\n", http.errorToString(httpCode).c_str());
      r = false;
  }
  http.end();
  return r;
}

bool parseJson(String json)
{
    const size_t capacity = JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 70;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, json);

    int code = doc["code"];
    const char *message = doc["message"];

    if (code != 0){
        DEBUG("[API]Code:");
        DEBUG(code);
        DEBUG(" Message:");
        DEBUGLN(message);
        return false;
    }

    JsonObject data = doc["data"];
    unsigned long data_mid = data["mid"];
    int data_follower = data["follower"];
    if (data_mid == 0){
        DEBUGLN("[JSON] FORMAT ERROR");
        return false;
    }
    DEBUG("UID: ");
    DEBUG(data_mid);
    DEBUG(" follower: ");
    DEBUGLN(data_follower);

    follower = data_follower;
    return true;
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    DEBUGLN("- empty file or failed to open file");
    return String();
  }
  DEBUGLN("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  DEBUGLN(fileContent);
  return fileContent;
}

bool writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    DEBUGLN("- failed to open file for writing");
    return false;
  }
  if(file.print(message)){
    DEBUGLN("- file written");
    return true;
  } else {
    DEBUGLN("- write failed");
  }
  return false;
}

bool readFileCheck() {// return ture if all properties has value
  sta_ssid = readFile(SPIFFS, "/ssid.txt");
  sta_password = readFile(SPIFFS, "/password.txt");
  // biliuid = readFile(SPIFFS, "/biliuid.txt");
  // if (!sta_ssid.isEmpty() && !sta_password.isEmpty() && !biliuid.isEmpty()) {
    if (!sta_ssid.isEmpty() && !sta_password.isEmpty()) {
    Serial.printf("readFileCheck: Pass\nsta_ssid: %s\nsta_password: %s\nbiliuid: %s\n", sta_ssid.c_str(), sta_password.c_str(), biliuid.c_str());
    return true;
  } else {
    Serial.printf("readFileCheck: Fail\nsta_ssid: %s\nsta_password: %s\nbiliuid: %s\n", sta_ssid.c_str(), sta_password.c_str(), biliuid.c_str());
  }
  return false;
}

void drawSubs(){
  DEBUGLN("drawSubs: entry");
  u8g2.firstPage();
  u8g2.setFont(u8g2_font_wqy12_t_chinese1);
  do {
    u8g2.drawXBMP(0+5,4,40,40,icon40x40);
    u8g2.drawXBMP(64+5,4,16,16,name[0]);
    u8g2.drawXBMP(64+24+4,5,16,16,name[1]);
    u8g2.drawXBMP(64-12+4,24+5,16,16,name[2]);
    u8g2.drawXBMP(64+24-12+4,24+5,16,16,name[3]);
    u8g2.drawXBMP(64+48-12+4,24+5,16,16,name[4]);
    u8g2.drawStr(10, 60, String("Subscribers: " + String(follower)).c_str());
  } while (u8g2.nextPage());
  DEBUGLN("drawSubs: end");
}

void PrintWiFiStatus() {
  u8g2.firstPage();
    u8g2.setFont(u8g2_font_wqy12_t_chinese1);
  do {
    u8g2.drawStr(2, 11, "Connect to this WiFi");
    u8g2.drawStr(2, 23, "WiFi SSID:");
    u8g2.drawStr(2, 35, String(String("  ") + String(AP_NAME)).c_str()); // print AP name
    u8g2.drawStr(2, 47, "Website should pop up");
    u8g2.drawStr(2, 59, "  automatically");
  } while ( u8g2.nextPage() );
}

void darwBilibili(uint8 index) {
  u8g2.firstPage();
  do {
    u8g2.drawXBMP(0, 0, 128, 64, bilibiliFaces[index]);
  } while (u8g2.nextPage());
}

template<typename T>
void DEBUG(const T& s) {
  if (__DEBUG__) {
    Serial.print(s);
  }
}

template<typename T>
void DEBUGLN(const T& s) {
  if (__DEBUG__) {
    Serial.println(s);
  }
}