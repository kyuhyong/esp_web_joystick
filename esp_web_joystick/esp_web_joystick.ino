/**
 * @file esp_web_joystick.ino
 * @author Kyuhyong You (kyuhyong@gmail.com)
 * @brief Web app for remotely controlling R1mini using ESP-12E module attached.
 * @version 0.1
 * @date 2022-03-25
 * 
 * @copyright Copyright (c) 2022 Kyuhyong You
 * 
 */

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <SoftwareSerial.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "pushButton.h"

#define LED_ONBOARD_ON     digitalWrite(PIN_LED_ONBOARD, LOW)
#define LED_ONBOARD_OFF    digitalWrite(PIN_LED_ONBOARD, HIGH)
#define LED_STATUS_ON      digitalWrite(PIN_LED_STATUS, LOW)
#define LED_STATUS_OFF     digitalWrite(PIN_LED_STATUS, HIGH)

// SoftwareSerial ---------------------------------
SoftwareSerial    swSer;                        // Serial port connected to R1mini
const int         SWSER_RX_PIN =          14;   // D5 Pin
const int         SWSER_TX_PIN =          12;   // D6 Pin
// Hardware Configurations  ------------------------------------------------------------
const int         PIN_LED_ONBOARD =       D4;   // LED onboard ESP-12E module
const int         PIN_LED_STATUS =        D2;   // D2 connected to ESPIO4
bool              ledState = false;
unsigned long     status_led_last_millis = 0; 
// Input button Pins ------------------------------------------
const int         PIN_INPUT_BTN = D1;           // Input pin GPIO5
PUSH_BUTTON       push_button(PIN_INPUT_BTN, INPUT_PULLUP);

/// Set Wifi ssid and password ----------------------------------
IPAddress         local_ip;       // Local IP address assigned by Router
bool              is_AP_mode_set =  false;
bool              is_wifi_on =      true;
WiFiManager       wm;
const char*       host =      "web_joy";
String            AP_SSID  =  "ESP_REMOTE";
String            AP_PASSWD = "password";

File              fsUploadFile;     // a File object to temporarily store the received file
/// For parsing json format
DynamicJsonDocument       d_doc(1024);     // For JSON 6.0 over
#define           DOC_SIZE   256   // JSON Document size For JSON 5.0
StaticJsonDocument<DOC_SIZE> doc;
JsonObject        root = doc.to<JsonObject>();

/// Set web server port number to 80
ESP8266WebServer  server(80);
WebSocketsServer  webSocket(81);    // create a websocket server on port 81

/**
 * @brief Callback function for any incomming websocket message
 * 
 * @param num 
 * @param type 
 * @param payload 
 * @param lenght 
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
   switch (type) {
      case WStype_DISCONNECTED:             // if the websocket is disconnected
         Serial.printf("[%u] Disconnected!\n", num);
         Serial.println();
         break;
      case WStype_CONNECTED: {              // if a new websocket connection is established
         IPAddress ip = webSocket.remoteIP(num);
         Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
         Serial.println();
         }
      break;
      case WStype_TEXT: {                    // if new text data is received
         //Serial.printf("[%u] get Text: %s\r\n", num, payload);
         auto error = deserializeJson(d_doc, payload);
         if(error) {
         Serial.print(F("deserializeJson() failed with code "));
         Serial.println(error.c_str());
         return;
         }
         const char* event = d_doc["event_name"];
         if(!strcmp(event, "Heartbeat")) {
         int hb = d_doc["intVal"];
         //Serial.printf("Heart Beat:%d\r\n", hb);
         }
         else if(!strcmp(event, "CMD")) {
         String cmd = d_doc["cmd"];
         int data1 = d_doc["data1"];
         char bufCmd[cmd.length()+1];
         cmd.toCharArray(bufCmd, cmd.length()+1);
         Serial.printf("[%d]$C%s,%d\r\n",num, bufCmd, data1);
         swSer.printf("$C%s,%d\r\n",bufCmd, data1);
         }
      }
      break;
   }
}
/**
 * @brief This function takes the parameters passed in the URL(the x and y coordinates of the joystick)
 * and sets the motor speed based on those parameters. 
 * 
 */
void handleJSData(){
   int type = server.arg(0).toInt();
   if(type == 1) {
      int x = server.arg(1).toInt();
      int y = server.arg(2).toInt();
      int vL = (y - x) * 5;
      int vR = (y + x) * 5;
      Serial.printf("Speed: %d, %d\r\n", vL, vR);
      swSer.printf("$CDIFFV,%d,%d\r\n",vL,vR);
      //return an HTTP 200
      server.send(200, "text/plain", "");   
   } else if(type == 2) {
      int r = server.arg(1).toInt() * 100/255;
      int g = server.arg(2).toInt() * 100/255;
      int b = server.arg(3).toInt() * 100/255;
      Serial.printf("RGB: %d, %d, %d\r\n", r, g, b);
      swSer.printf("$SCOLOR,%d,%d,%d\r\n", r, g, b);
   }
}
/**
 * @brief 
 * 
 */
void start_server(void) {
   server.serveStatic("/", SPIFFS, "/joystick.html"); 
   server.serveStatic("/virtualjoystick.js", SPIFFS, "/app.js");
   server.serveStatic("/styles.css", SPIFFS, "/styles.css");
   //call handleJSData function when this URL is accessed by the js in the html file
   server.on("/jsData.html", handleJSData);
   Serial.println("Begin server:");
   server.begin();
   MDNS.begin(host);
   webSocket.begin();                          // start the websocket server
}
/**
 * @brief 
 * 
 */
void run_AP_config(void) {
   is_AP_mode_set = true;
   Serial.println("Enter to AP configuration");
   server.stop();          /// Stop server for Config portal to be working
   webSocket.close();      /// Stop socket as well
   Serial.println("Starting config portal");
   wm.setConfigPortalTimeout(300); /// If no access point name has been previously entered disable timeout.
   wm.setCaptivePortalEnable(true);
   if (!wm.startConfigPortal(AP_SSID.c_str(), AP_PASSWD.c_str()))
   { 
      Serial.println("Not connected to WiFi but continuing anyway.");
   } else {
      /// You have connected to the WiFi
      String ssid = wm.getWiFiSSID();
      local_ip = WiFi.localIP();
      Serial.print("Successfully connected ip: ");
      Serial.println(local_ip.toString());
   }
   Serial.println("Re-connect to server");
   wm.setCaptivePortalEnable(false);
   start_server();
   is_AP_mode_set = false;
}

void new_pushButtonEvent(BUTTON_PRESS press)
{
   if(press == SHORT_PRESS) {
      Serial.println("Short Button Pressed!");
   } else if(press == LONG_PRESS) {
      Serial.println("Long Button Pressed!");
      run_AP_config();
   }
}

void setup() {
   WiFi.mode(WIFI_STA); /// explicitly set mode, esp defaults to STA+AP  
   Serial.begin(115200);
   swSer.begin(115200, SWSERIAL_8N1, SWSER_RX_PIN, SWSER_TX_PIN, false, 256);
   /// Initialize the output variables as outputs
   pinMode(PIN_LED_STATUS,   OUTPUT);
   pinMode(PIN_LED_ONBOARD,  OUTPUT);
   //pinMode(PIN_INPUT_BTN,    INPUT);
   push_button.onNewPushButtonEvent(new_pushButtonEvent);
   LED_ONBOARD_ON;
   Serial.println("\nStarting R1mini esp remote");
   Serial.println("Scanning networks...");
   int n = WiFi.scanNetworks();
   for(int i = 0; i < n; i++)
   {
      Serial.println(WiFi.SSID(i));
   }
   Serial.println("--------------------------");
   WiFi.printDiag(Serial); /// Remove this line if you do not want to see WiFi password printed
   if (WiFi.SSID() == "") {
      run_AP_config();
   } else {
      Serial.print("Connect to AP");
      Serial.print(WiFi.SSID());
      Serial.print(" Pass:");
      Serial.println(WiFi.psk());
      WiFi.begin(WiFi.SSID(), WiFi.psk());
   }
   int connRes = WiFi.waitForConnectResult();
   Serial.print(" secs in setup() connection result is ");
   Serial.println(connRes);
   if (WiFi.status()!=WL_CONNECTED){
      Serial.println("failed to connect, finishing setup anyway");
   } else{
      Serial.print("local ip: ");
      Serial.println(WiFi.localIP());
      server.begin();
      Serial.println("Server Started!");
   }
   /// Try to access files uploaded by SPIFFS library
   if (!SPIFFS.begin()) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
   }
   Dir dir = SPIFFS.openDir("/");
   while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
      Serial.println();
   }
   //set the static pages on SPIFFS for the html and js
   start_server();
   webSocket.onEvent(webSocketEvent); /// Register callback function 'webSocketEvent'
   Serial.println("WebSocket server started.");
   delay(500);
   LED_ONBOARD_OFF;
}


void loop() {
   if( (millis() - status_led_last_millis) > 500) {
      ledState=!ledState;
      digitalWrite(PIN_LED_STATUS, ledState);
      status_led_last_millis = millis();
  }
  server.handleClient();
  webSocket.loop();
  push_button.update();
  //buttonHandler_loop();
}