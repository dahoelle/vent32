#include <humidity_fan_controller.h>
#include <./wifi_acess.h>
#include <../scheduler.h>


//========OBJ======================
NetworkSettings networkSettings = {NetworkMode::AP,WiFiAcess_SSID,WiFiAcess_PASS};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
DNSServer dnsServer;
Scheduler scheduler;

SensorState sensorState = IDLE;
unsigned long lastMeasurementTime = 0;

float threshold = 60;
float minVentTime = 1000;
float humidity = -999; 
float tempC = -999;
bool fanState = false;


//----VENT------------
time_t ventTimeout = 0;
bool manualMode = false;

void ventFor(int t) {
  manualMode = true;
  ventTimeout = time(NULL) + t;
  fanState = true;
  digitalWrite(PIN_RELAY_SIG, HIGH);
}

void ventForAdditional(int t) {
  if (manualMode && ventTimeout > time(NULL)) {
    ventTimeout += t;
  } else {
    ventFor(t);
  }
}

void checkVentilation() {
  if (manualMode) {
    // Manual mode timeout check
    if (time(NULL) >= ventTimeout) {
      manualMode = false;
      fanState = false;
      digitalWrite(PIN_RELAY_SIG, LOW);
    }
  } else {
    // Automatic mode control
    bool shouldVentilate = humidity > threshold;
    if (fanState != shouldVentilate) {
      fanState = shouldVentilate;
      digitalWrite(PIN_RELAY_SIG, fanState ? HIGH : LOW);
    }
  }
}


//=========SETUP===================
void setup_gpio(){
  Serial.begin(115200);
  pinMode(PIN_RELAY_SIG, OUTPUT);
    
}
void setup_vent_sync(){
  //vent state synchronizer
  scheduler.setInterval(checkVentilation, 1000);
}
void setup_sensors() {
  Wire.begin();
  Wire.setClock(100000);
  
  scheduler.setInterval([] {
    unsigned long now = millis();
    
    switch (sensorState) {
      case IDLE:
        // Start temperature measurement
        Wire.beginTransmission(SHT20_ADDRESS);
        if (Wire.write(0xF3) == 1 && Wire.endTransmission() == 0) {
          lastMeasurementTime = now;
          sensorState = TEMP_REQUESTED;
        }
        break;

      case TEMP_REQUESTED:
        if (now - lastMeasurementTime >= 100) { // Wait 100ms for temp
          Wire.requestFrom(SHT20_ADDRESS, 3);
          if (Wire.available() >= 3) {
            uint16_t raw = Wire.read() << 8;
            raw |= Wire.read();
            tempC = -46.85f + (175.72f * raw / 65536.0f);
            
            // Start humidity measurement
            Wire.beginTransmission(SHT20_ADDRESS);
            if (Wire.write(0xF5) == 1 && Wire.endTransmission() == 0) {
              lastMeasurementTime = now;
              sensorState = HUMIDITY_REQUESTED;
            } else {
              sensorState = IDLE;
            }
          } else {
            tempC = -999;
            sensorState = IDLE;
          }
        }
        break;

      case HUMIDITY_REQUESTED:
        if (now - lastMeasurementTime >= 40) { // Wait 40ms for humidity
          Wire.requestFrom(SHT20_ADDRESS, 3);
          if (Wire.available() >= 3) {
            uint16_t raw = Wire.read() << 8;
            raw |= Wire.read();
            humidity = -6.0f + (125.0f * raw / 65536.0f);
          } else {
            humidity = -999;
          }
          sensorState = IDLE;
        }
        break;

      default:
        Serial.println("WARN: invalid sensor state");
        break;
    }
  }, 50); // Poll every 50ms
}
void setup_wifi(){
	WiFi.disconnect(true);
  WiFi.setHostname(deviceNetName);  
	delay(50);
  WiFi.mode(WIFI_STA);
  WiFi.begin(networkSettings.ssid ,networkSettings.pass);
  WiFi.setAutoReconnect(true);
  uint retries = 10;
  while(retries >0 ){
    retries--;
    if(WiFi.status() != WL_CONNECTED){
      delay(1000);
    }else{
      retries = 0;
    }
  }
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("wifi didnt connect. restarting...");
    ESP.restart();
  }
  
  Serial.println(WiFi.softAPIP());
  Serial.println(WiFi.localIP());
  //dns
  scheduler.setInterval([]() {
    dnsServer.processNextRequest();
  }, 500);
}
void setup_webserver(){ 
   //GET /
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request){
    request->send_P(200, "text/html", rootHTML);
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(256);  // Increased from 128 to 256 bytes
    doc["humidity"] = humidity;
    doc["tempC"] = tempC;
    doc["fan_state"] = fanState;
    doc["manual_mode"] = manualMode;

    // Use stack-friendly serialization
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/threshold", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"threshold\":" + String(threshold, 1) + "}");
  });

  server.on("/threshold", HTTP_PUT, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Missing 'value' parameter");
      return;
    }
    String valueParam = request->getParam("value")->value();
    float newThreshold = valueParam.toFloat();
    if (newThreshold < 0 || newThreshold > 100) {
      request->send(400, "text/plain", "Threshold must be between 0-100%");
      return;
    }
    threshold = newThreshold;
    request->send(200, "text/plain", "Threshold updated to " + String(threshold, 1) + "%");
  });

  server.on("/min-vent-time", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"min_vent_time\":" + String(minVentTime) + "}");
  });

  server.on("/min-vent-time", HTTP_PUT, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Missing 'value' parameter");
      return;
    }
    String valueParam = request->getParam("value")->value();
    unsigned int newTime = valueParam.toInt();
    if (newTime > 3600) {
      request->send(400, "text/plain", "Vent time must be 0-threshold seconds");
      return;
    }
    
    minVentTime = newTime;
    request->send(200, "text/plain", "Minimum vent time updated to " + String(minVentTime) + "s");
  });
  //404
  server.onNotFound([](AsyncWebServerRequest *request) {  
    Serial.printf("Request not found: %s\n", request->url().c_str());
    request->send(404, "text/plain", "Not found");
  });
  server.begin();
}
void setup() {
  delay(5);
  std::vector<SetupTask> setupTasks = {
    {setup_gpio, "GPIO and PinMode"},
    {setup_sensors, "Sensors and Estimations"},
    //{setup_vent_sync, "vent Timeout Handling"},
    //{setup_button, "Main button functions"},
    {setup_wifi, "WiFi"},
    {setup_webserver, "Web server"},
  };
  for (const auto& task : setupTasks) {
    Serial.print(task.description);
    Serial.println(" setup starting");
    task.func();
    Serial.print(task.description);
    Serial.println(" setup complete");
  }
  
  Serial.println("DEVICE SETUP COMPLETE");
}

//==========PROC==========
unsigned long lu = 0;
void loop() {
  unsigned long cmillis = millis();
  unsigned long delta = cmillis - lu; 
  lu = cmillis;  

  scheduler.updateProcess(delta);
}