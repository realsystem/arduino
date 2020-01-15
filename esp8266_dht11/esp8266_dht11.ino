#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "esp8266_dht11.h"

#define DHT_TYPE DHT11
#define FRIDGE_TEMP 5
#define FRIDGE_TEMP_MAX 8
#define FRIDGE_TEMP_MIN 2
#define SHELL_TEMP 30
#define SHELL_TEMP_MAX 40
#define WIFI_MAX_RETRY 20
#define TELE_HOST "api.telegram.org"
#define TELE_PORT 443
//#define BOT_TIMEOUT 10000
#define TEMP_TIMEOUT 600000 //10 min
#define ALARM_FRIDGE_MIN_MSG "Approached Fridge MIN temperature."
#define ALARM_FRIDGE_MAX_MSG "Approached Fridge MAX temperature."
#define ALARM_SHELL_MAX_MSG "Approached Shell MAX temperature."

uint8_t dhtFridgePin = D2;
uint8_t dhtShellPin = D1;
uint8_t fridgeFanPin = D6;
uint8_t fridgePumpPin = D7;

float fridgeTemp;
float fridgeHum;
float shellTemp;
float shellHum;

boolean alarmFridgeMin = false;
boolean alarmFridgeMax = false;
boolean alarmShellMax = false;

boolean fridgeFanEnabled = false;
boolean fridgePumpEnabled = false;

boolean offlineWork = false;

//uint32_t botLastTime;
unsigned long tempTimer = 0;

DHT dhtFridge(dhtFridgePin, DHT_TYPE);
DHT dhtShell(dhtShellPin, DHT_TYPE);

ESP8266WebServer server(80);
ESP8266WiFiMulti wifiMulti;

WiFiClientSecure client;

boolean sendReq(String reqType, String reqUrl, String reqData, unsigned int reqLen) {
  DeserializationError error;
  StaticJsonDocument<512> respData;

  if (!client.connect(TELE_HOST, TELE_PORT)) {
    Serial.println("Telegram server connection failed");
    return false;
  } else {
    Serial.println("Telegram bot is ready");
  }

  client.println(reqType + String(" /bot") + BOT_TOKEN + reqUrl + " HTTP/1.1");
  client.println(String("Host: ") + TELE_HOST);
  client.println("User-Agent: TacomaFridgeESP8266");
  client.println("Connection: close");
  if ((reqData != "") and (reqLen != 0)) {
    client.println("Content-Type: application/json");
    client.println(String("Content-Length: ") + reqLen);
    client.println();
    client.println(reqData);
    Serial.println(reqData);
  } else {
    client.println();
  }

  Serial.println("Request sent");
  delay(10);
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.println(line);
  error = deserializeJson(respData, line);
  if (error) {
    Serial.println("Error: deserializeJson failed");
    Serial.println(error.c_str());
    return false;
  }
  if (!respData["ok"]) {
    const int errCode = respData["error_code"];
    const char* errDesc = respData["description"];
    Serial.println(String("Error ") + errCode + ": " + errDesc);
  }
  return respData["ok"];
}

boolean checkBot(void) {
  return sendReq(String("GET"), "/getMe", "", 0);
}

void sendMsg(String message, boolean silent=true) {
  String req;
  StaticJsonDocument<256> reqData;

  reqData["chat_id"] = BOT_CHAT_ID;
  reqData["text"] = message;
  reqData["disable_notification"] = silent;
  serializeJson(reqData, req);
  sendReq(String("POST"), "/sendMessage", req, measureJson(reqData));
}

void fridgeFanEn() {
  if (!fridgeFanEnabled) {
    digitalWrite(fridgeFanPin, 0);
    fridgeFanEnabled = true;
    sendMsg(String("Shell Temp: ") + (int)shellTemp + "\nFridge Fan Enabled");
  }
}

void fridgeFanDis() {
  if (fridgeFanEnabled) {
    digitalWrite(fridgeFanPin, 1);
    fridgeFanEnabled = false;
    sendMsg(String("Shell Temp: ") + (int)shellTemp + "\nFridge Fan Disabled");
  }
}

void fridgePumpEn() {
  if (!fridgePumpEnabled) {
    digitalWrite(fridgePumpPin, 0);
    fridgePumpEnabled = true;
    sendMsg(String("Fridge Temp: ") + (int)fridgeTemp + "\nFridge Pump Enabled");
  }
}

void fridgePumpDis() {
  if (fridgePumpEnabled) {
    digitalWrite(fridgePumpPin, 1);
    fridgePumpEnabled = false;
    sendMsg(String("Fridge Temp: ") + (int)fridgeTemp + "\nFridge Pump Disabled");
  }
}

//String handleStartCommand() {
//  String response = "Available commands:";
//  response += "Yel_on Turns on the Yellow LED\n\r";
//  response += "Yel_off Turns off the Blue LED\n\r";
//  response += "Blue_on Turns on the Blue LED\n\r";
//  response += "Blue_off Turns off the Blue LED\n\r";
//  response += "Temperature Returns the current temperature\n";
//  response += "Humidity Returns the current humidity\n";
//  return response; 
//}
//
//void handleMessage(String message, String chatId) {
//  if (message.equalsIgnoreCase("/start")) 
//    bot.sendMessage(chatId, handleStartCommand(), "");
//  else if (message.equalsIgnoreCase("/fridge_on")) 
//     fridgePumpEn();
//  else if (message.equalsIgnoreCase("/fridge_off")) 
//     fridgePumpDis();
//  else if (message.equalsIgnoreCase("/fan_on")) 
//     fridgeFanEn();
//  else if (message.equalsIgnoreCase("/fan_off")) 
//     fridgeFanDis();
//   bot.message[0][0] = ""; 
//}
//
//void checkMessages() {
//   uint32_t now = millis();
//   if (now - botLastTime < BOT_TIMEOUT) {
//      Serial.println("Checking new messages....");   
//      bot.getUpdates(bot.message[0][1]);
//      int numNewMessages = bot.message[0][0].toInt() + 1;
//       for (int i = 1;  i < numNewMessages; i++) {
//        String chatId = bot.message[i][4];
//        String message = bot.message[i][5];
//        handleMessage(message, chatId);
//      }
//   }
//   botLastTime = now;
//}

void handle_OnConnect() {
  server.send(200, "text/html", MainPage(fridgeTemp, fridgeHum, shellTemp, shellHum, alarmFridgeMin, alarmFridgeMax, alarmShellMax, fridgeFanEnabled, fridgePumpEnabled));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String MainPage(float fridgeTempStat, float fridgeHumStat, float shellTempStat, float shellHumStat, boolean alarmFridgeMin, boolean alarmFridgeMax, boolean alarmShellMax, boolean fridgeFanEnabled, boolean fridgePumpEnabled) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Tacoma cargo area</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  
  ptr += "<h1>Fridge</h1>\n";
  if (fridgePumpEnabled) {
    ptr += "<p>Fridge Pump Enabled</p>";
  } else {
    ptr += "<p>Fridge Pump Disabled</p>";
  }
  if (alarmFridgeMin) {
    ptr += "<p>ALARM: Approached Fridge MIN Temperature</p>";
  } else if (alarmFridgeMax) {
    ptr += "<p>ALARM: Approached Fridge MAX Temperature</p>";
  }
  ptr += "<p>Temperature: ";
  ptr += (int)fridgeTempStat;
  ptr += "°C</p>";
  ptr += "<p>Humidity: ";
  ptr += (int)fridgeHumStat;
  ptr += "%</p>";

  ptr += "<h1>Shell</h1>\n";
  if (fridgeFanEnabled) {
    ptr += "<p>Fridge Fan Enabled</p>";
  } else {
    ptr += "<p>Fridge Fan Disabled</p>";
  }
  if (alarmShellMax) {
    ptr += "<p>ALARM: Approached Shell MAX Temperature</p>";
  }
  ptr += "<p>Temperature: ";
  ptr += (int)shellTempStat;
  ptr += "°C</p>";
  ptr += "<p>Humidity: ";
  ptr += (int)shellHumStat;
  ptr += "%</p>";
  
  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

void wifiConnect(void) {
  uint8_t cnt = 0;

  Serial.println("Connecting ...");

  while (wifiMulti.run() != WL_CONNECTED) {
    if (cnt >= WIFI_MAX_RETRY) {
      Serial.println("Can't connect to WiFi. Keep working offline.");
      offlineWork = true;
      break;
    }
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print(".");
    cnt ++;
  }
  Serial.println("");
  if (offlineWork) {
    return;
  }

  Serial.println("WiFi connected to:");
  Serial.println(WiFi.SSID());
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  WiFi.softAPdisconnect(true);

  Serial.begin(115200);
  delay(100);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(dhtFridgePin, INPUT);
  pinMode(dhtShellPin, INPUT);
  pinMode(fridgeFanPin, OUTPUT);
  pinMode(fridgePumpPin, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(fridgePumpPin, 1);
  digitalWrite(fridgeFanPin, 1);

  dhtFridge.begin();
  dhtShell.begin();

  wifiMulti.addAP(hubSsid, hubPassword);
  wifiMulti.addAP(homeSsid, homePassword);

  wifiConnect();

  if (!MDNS.begin("esp8266")) {
    Serial.println("Error setting up mDNS responder!");
  }
  Serial.println("mDNS responder started");

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

  if (checkBot()) {
    sendMsg("Start control fridge in Tacoma");
  }
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  unsigned long curMillis = millis();

  fridgeTemp = dhtFridge.readTemperature();
  fridgeHum = dhtFridge.readHumidity();
  shellTemp = dhtShell.readTemperature();
  shellHum = dhtShell.readHumidity();

  if ((int)fridgeTemp <= FRIDGE_TEMP_MIN) {
    alarmFridgeMin = true;
    alarmFridgeMax = false;
  } else if ((int)fridgeTemp >= FRIDGE_TEMP_MAX) {
    alarmFridgeMax = true;
    alarmFridgeMin = false;
  } else {
    alarmFridgeMax = false;
    alarmFridgeMin = false;
  }

  if ((int)fridgeTemp <= FRIDGE_TEMP - 1) {
    fridgePumpDis();
  } else if ((int)fridgeTemp >= FRIDGE_TEMP + 1) {
    fridgePumpEn();
  }

  if ((int)shellTemp >= SHELL_TEMP_MAX) {
    alarmShellMax = true;
  }

  if ((int)shellTemp <= SHELL_TEMP - 3) {
    fridgeFanDis();
  } else if ((int)shellTemp >= SHELL_TEMP) {
    fridgeFanEn();
  }

  if ((tempTimer == 0) or (curMillis - tempTimer >= TEMP_TIMEOUT)) {
    if (!checkBot()) {
      wifiConnect();
    }
    sendMsg(String("Fridge Temp: ") + (int)fridgeTemp +
            "\nShell Temp: " + (int)shellTemp);
    if (alarmFridgeMin) {
      Serial.println(ALARM_FRIDGE_MIN_MSG);
      sendMsg(ALARM_FRIDGE_MIN_MSG, false);
    }
    if (alarmFridgeMax) {
      Serial.println(ALARM_FRIDGE_MAX_MSG);
      sendMsg(ALARM_FRIDGE_MAX_MSG, false);
    }
    if (alarmShellMax) {
      Serial.println(ALARM_SHELL_MAX_MSG);
      sendMsg(ALARM_SHELL_MAX_MSG, false);
    }
    tempTimer = curMillis;
  }

  server.handleClient();

  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}
