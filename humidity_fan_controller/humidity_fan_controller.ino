#include <humidity_fan_controller.h>
#include <wifi_acess.h>
#include <../scheduler.h>


//========OBJ======================
NetworkSettings networkSettings = {NetworkMode::AP,"",""};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
DNSServer dnsServer;
Scheduler scheduler;

SensorState sensorState = IDLE;
unsigned long lastMeasurementTime = 0;

float threshold = 60;
float minVentTime = 60;
float humidity = -999; 
float tempC = -999;
bool fanState = false;


//----VENT------------
unsigned long ventTimeout = 0; //ms
bool manualMode = false;
time_t autoVentStart = 0;

void ventFor(int t) {  // t in seconds
  manualMode = true;
  ventTimeout = millis() + (t * 1000);
  fanState = true;
  digitalWrite(PIN_RELAY_SIG, HIGH);
}

void ventForAdditional(int t) {  // t in seconds
  if (manualMode && millis() < ventTimeout) {  // Check if still in active manual mode
    ventTimeout += (t * 1000);  // Extend timeout
  } else {
    ventFor(t);
  }
}

void checkVentilation() {
  if (manualMode) {
    // Manual mode: Run for EXACTLY requested time
    if (millis() >= ventTimeout) {
      manualMode = false;
      fanState = false;
      digitalWrite(PIN_RELAY_SIG, LOW);
    }
  } else {
    // Automatic mode: Ensure MINIMUM vent time
    bool shouldVentilate = humidity > threshold;
    
    if (fanState) {
      // Fan is ON - check if can turn off
      bool minTimePassed = (millis() - autoVentStart) >= (minVentTime * 1000UL);
      if (!shouldVentilate && minTimePassed) {
        fanState = false;
        digitalWrite(PIN_RELAY_SIG, LOW);
      }
    } else {
      // Fan is OFF - turn on if needed
      if (shouldVentilate) {
        autoVentStart = millis();
        fanState = true;
        digitalWrite(PIN_RELAY_SIG, HIGH);
      }
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
  scheduler.setInterval(checkVentilation, 500);
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
  strncpy(networkSettings.ssid, WiFiAcess_SSID, sizeof(networkSettings.ssid));
  strncpy(networkSettings.pass, WiFiAcess_PASS, sizeof(networkSettings.pass));
  //Serial.println(networkSettings.ssid);
  //Serial.println(networkSettings.pass);
  //Serial.println("====end net settings======");
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
  server.on("/",              HTTP_GET, [](AsyncWebServerRequest * request){
    request->send_P(200, "text/html", rootHTML);
  });
  server.on("/state",         HTTP_GET, [](AsyncWebServerRequest* request) {
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
  server.on("/threshold",     HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"threshold\":" + String(threshold, 1) + "}");
  });
  server.on("/threshold",     HTTP_PUT, [](AsyncWebServerRequest *request) {
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
      request->send(400, "text/plain", "Vent time must be 0-3600 seconds");
      return;
    }
    
    minVentTime = newTime;
    request->send(200, "text/plain", "Minimum vent time updated to " + String(minVentTime) + "s");
  });
  server.on("/vent",          HTTP_PUT, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("seconds")) {
      request->send(400, "text/plain", "Missing 'seconds' parameter");
      return;
    }
    String secondsParam = request->getParam("seconds")->value();
    unsigned int t = secondsParam.toInt();
    if (t == 0) { // toInt() returns 0 on failure
      request->send(400, "text/plain", "Invalid vent time");
      return;
    }
    ventForAdditional(t);
    request->send(200, "text/plain", "Venting for " + String(t) + "s");
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
    {setup_vent_sync, "vent Timeout Handling"},
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