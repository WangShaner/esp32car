#include "Arduino.h"
// #include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include <SPIFFS.h>
#include <Update.h>

AsyncWebServer server(8080);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

const char* ssid = "CMCC-FWrv";
const char* password = "32jfdy9u";
const char* http_username = "admin";
const char* http_password = "admin";

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

typedef uint16_t siz_t;
typedef uint16_t distance_t;

distance_t distance = (distance_t)-1;  // in mm

#define kDistanceNum 3
distance_t distances[kDistanceNum];
siz_t distanceIndex;  

bool automaticMode = false;

#define pinA0 12
#define pinA1 14
#define pinA2 27

#define pinB0 33
#define pinB1 25
#define pinB2 26

#define pinC0 4
#define pinC1 2
#define pinC2 15

#define pinD0 5
#define pinD1 18
#define pinD2 19

#define pinEcho 35
#define pinTrig 32

void initPins() {
  pinMode(pinA0, OUTPUT);
  pinMode(pinA1, OUTPUT);
  pinMode(pinA2, OUTPUT);
  pinMode(pinB0, OUTPUT);
  pinMode(pinB1, OUTPUT);
  pinMode(pinB2, OUTPUT);
  pinMode(pinC0, OUTPUT);
  pinMode(pinC1, OUTPUT);
  pinMode(pinC2, OUTPUT);
  pinMode(pinD0, OUTPUT);
  pinMode(pinD1, OUTPUT);
  pinMode(pinD2, OUTPUT);
  pinMode(pinEcho, INPUT);                    //Set pinEcho as input, to receive measure result from US-025,US-026
  pinMode(pinTrig, OUTPUT);                   //Set pinTrig as output, used to send high pusle to trig measurement (>10us)

}

void forwardOne(int8_t pin0, int8_t pin1, int8_t pin2) {
  digitalWrite(pin0, HIGH);
  digitalWrite(pin1, HIGH);
  digitalWrite(pin2, LOW);
}

void backwardOne(int8_t pin0, int8_t pin1, int8_t pin2) {
  digitalWrite(pin0, HIGH);
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, HIGH);
}

void stopOne(int8_t pin0, int8_t pin1, int8_t pin2) {
  digitalWrite(pin0, LOW);
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, LOW);
}

void backward() {
  Serial.printf("Forward!\n");
  forwardOne(pinA0, pinA1, pinA2);
  forwardOne(pinB0, pinB1, pinB2);
  forwardOne(pinC0, pinC1, pinC2);
  forwardOne(pinD0, pinD1, pinD2);
}

void forward() {
  Serial.printf("Backward!\n");
  backwardOne(pinA0, pinA1, pinA2);
  backwardOne(pinB0, pinB1, pinB2);
  backwardOne(pinC0, pinC1, pinC2);
  backwardOne(pinD0, pinD1, pinD2);
}

void stop() {
  Serial.printf("Stop!\n");
  stopOne(pinA0, pinA1, pinA2);
  stopOne(pinB0, pinB1, pinB2);
  stopOne(pinC0, pinC1, pinC2);
  stopOne(pinD0, pinD1, pinD2);
}

void turnLeft() {
  stopOne(pinA0, pinA1, pinA2);
  stopOne(pinB0, pinB1, pinB2);
  forwardOne(pinC0, pinC1, pinC2);
  forwardOne(pinD0, pinD1, pinD2);
}

void turnRight() {
  forwardOne(pinA0, pinA1, pinA2);
  forwardOne(pinB0, pinB1, pinB2);
  stopOne(pinC0, pinC1, pinC2);
  stopOne(pinD0, pinD1, pinD2);
}

void move(AsyncWebServerRequest *request) {
  if (request->hasParam("dir")) {
    AsyncWebParameter* p = request->getParam("dir");
    if (p->value() == "F") {
      forward();
    } else if (p->value() == "B") {
      backward();
    } else if (p->value() == "S") {
      stop();
    } else if (p->value() == "L") {
      turnLeft();
    } else if (p->value() == "R") {
      turnRight();
    } 
  }
  request->send(200);
}

void action(AsyncWebServerRequest *request) {
  if (request->hasParam("type")) {
    AsyncWebParameter* p = request->getParam("type");
    if (p->value() == "1") {
      automaticMode = !automaticMode;
      if (!automaticMode) {
        stop();
      }
      Serial.printf("automaticMode:%d!\n", automaticMode);
    }
  }
  request->send(200, "text/plain", automaticMode ? "in automaticMode" : "out of automaticMode");
}

void sense(AsyncWebServerRequest *request) {
  if (request->hasParam("type")) {
    AsyncWebParameter* p = request->getParam("type");
    if (p->value() == "distance") {
      char distanceStr[10];
      itoa(distance, distanceStr, 10);
      request->send(200, "text/plain", distanceStr);
      Serial.printf("distance:%s!\n", distanceStr);
      return;
    }
  }
  request->send(200, "text/plain", "unknown request");
}

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket event
}

distance_t measureDistance() {
  digitalWrite(pinTrig, HIGH);              //begin to send a high pulse, then US-025/US-026 begin to measure the distance
    delayMicroseconds(20);                    //set this high pulse width as 20us (>10us)
    digitalWrite(pinTrig, LOW);               //end this high pulse
    
    int32_t Time_Echo_us = pulseIn(pinEcho, HIGH);               //calculate the pulse width at pinEcho, 
    if((Time_Echo_us < 60000) && (Time_Echo_us > 1))     //a valid pulse width should be between (1, 60000).
    {
      int32_t Len_mm = (Time_Echo_us*34/100)/2;      //calculate the distance by pulse width, Len_mm = (Time_Echo_us * 0.34mm/us) / 2 (mm)
      Serial.print("Present Distance is: ");  //output result to Serial monitor
      Serial.print(Len_mm, DEC);            //output result to Serial monitor
      Serial.println("mm");                 //output result to Serial monitor
      return Len_mm;
    }
    return -1;
}

void initDistances() {
  distance = measureDistance();
  for (siz_t i = 0; i < kDistanceNum; ++i) {
    distances[i] = distance;
  }
  distanceIndex = 0;
}

inline siz_t nextIndex(size_t index, siz_t size) {
  return (++index) % size;
}

void updateDistance() {
  distance = measureDistance();
  distances[distanceIndex] = distance;
  distanceIndex = nextIndex(distanceIndex, kDistanceNum);
}

void selfDrive() {
  const distance_t kLowDistance1 = 300;
  const distance_t kLowDistance2 = 400;
  if (distance <= kLowDistance1) {
    turnLeft();
  } else {
    forward();
  }
}

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }
  Serial.printf(WiFi.localIP().toString().c_str());
  Serial.printf("\n");

  initPins();
  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // attach AsyncEventSource
  server.addHandler(&events);

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // upload a file to /upload
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, onUpload);

  // send a file when /index is requested
  server.on("/index", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.htm");
  });

  // HTTP basic authentication
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
        return request->requestAuthentication();
    request->send(200, "text/plain", "Login Success!");
  });

  server.on("/move", HTTP_GET, move);

  server.on("/action", HTTP_GET, action);

  server.on("/sense", HTTP_GET, sense);

  // // Simple Firmware Update Form
  // server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
  //   request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  // });
  // server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
  //   shouldReboot = !Update.hasError();
  //   AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
  //   response->addHeader("Connection", "close");
  //   request->send(response);
  // },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //   if(!index){
  //     Serial.printf("Update Start: %s\n", filename.c_str());
  //     Update.runAsync(true);
  //     if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
  //       Update.printError(Serial);
  //     }
  //   }
  //   if(!Update.hasError()){
  //     if(Update.write(data, len) != len){
  //       Update.printError(Serial);
  //     }
  //   }
  //   if(final){
  //     if(Update.end(true)){
  //       Serial.printf("Update Success: %uB\n", index+len);
  //     } else {
  //       Update.printError(Serial);
  //     }
  //   }
  // });

  // attach filesystem root at URL /fs
  server.serveStatic("/fs", SPIFFS, "/");

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();
  initDistances();
}

void loop(){
  updateDistance();
  if (automaticMode) {
    selfDrive();
  }
  delay(500);
  if(shouldReboot){
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  static char temp[128];
  sprintf(temp, "Seconds since boot: %u", millis()/1000);
  events.send(temp, "time"); //send event "time"
}
