#include <humidity_fan_controller.h>
#include <scheduler.h>


//========OBJ======================
NetworkSettings networkSettings = {NetworkMode::AP,"",""};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
DNSServer dnsServer;
Scheduler scheduler;

float humidity = -999; 
float tempC = -999;
bool fanState = false;

//-----EEPROM----------
void setNetworkSettings(const NetworkSettings& settings) {
  EEPROM.put(NET_MODE_ADDR, static_cast<byte>(settings.mode));
  for (int i = 0; i < 32; ++i)
    EEPROM.put(NET_SSID_ADDR + i, settings.ssid[i]);
  EEPROM.put(NET_SSID_ADDR + 32, '\0');
  for (int i = 0; i < 32; ++i)
    EEPROM.put(NET_PASS_ADDR + i, settings.pass[i]);
  EEPROM.put(NET_PASS_ADDR + 32, '\0');
  EEPROM.commit(); 
}
NetworkSettings getNetworkSettings() {
  NetworkSettings settings;
  settings.mode = EEPROM.read(NET_MODE_ADDR);
  EEPROM.get(NET_SSID_ADDR, settings.ssid);
  EEPROM.get(NET_PASS_ADDR, settings.pass);
  settings.ssid[31] = '\0';
  settings.pass[31] = '\0';
  return settings;
}
void setThreshold(float value) {
  EEPROM.put(THRESHOLD_ADDR, value);
  EEPROM.commit();
}
float getThreshold() {
  float value;
  EEPROM.get(THRESHOLD_ADDR, value);
  return value;
}

void setMinVentTime(unsigned int value) {
  EEPROM.put(MIN_VENT_TIME_ADDR, value);
  EEPROM.commit();
}

unsigned int getMinVentTime() {
  unsigned int value;
  EEPROM.get(MIN_VENT_TIME_ADDR, value);
  return value;
}

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
    bool shouldVentilate = humidity > getThreshold();
    if (fanState != shouldVentilate) {
      fanState = shouldVentilate;
      digitalWrite(PIN_RELAY_SIG, fanState ? HIGH : LOW);
    }
  }
}


//=========SETUP===================
void setup_gpio(){
  Serial.begin(115200);
  
  //pinMode(PIN_RELAY_SIG, OUTPUT);
  pinMode(PIN_RELAY_VCC, OUTPUT);
  pinMode(PIN_RELAY_GND, OUTPUT);
  //digitalWrite(PIN_RELAY_SIG, LOW);
  digitalWrite(PIN_RELAY_VCC, HIGH);
  digitalWrite(PIN_RELAY_GND, LOW);

  //vent state synchronizer
  //scheduler.setInterval(checkVentilation, 1000);
}
void setup_scheduler(){
  scheduler.setErrorHandler([](const String& msg) {
    Serial.print("[ERROR] ");
    Serial.println(msg);
  });
}
void setup_eeprom(){
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
    ESP.restart();
  }
  Serial.println(getThreshold());
  Serial.println(getMinVentTime());
  networkSettings = getNetworkSettings();
}
void setup_sensors() {
  Wire.begin();
  Wire.setClock(100000);
  
  scheduler.setInterval([] {
    // Start temperature measurement
    Wire.beginTransmission(SHT20_ADDRESS);
    Wire.write(0xF3);
    Wire.endTransmission();
    
    scheduler.setTimeout([] {
      // Read temperature
      Wire.requestFrom(SHT20_ADDRESS, 3);
      if(Wire.available() >= 3) {
        uint16_t raw = Wire.read() << 8;
        raw |= Wire.read();
        tempC = -46.85f + (175.72f * raw / 65536.0f);
        
        // Now start humidity measurement
        Wire.beginTransmission(SHT20_ADDRESS);
        Wire.write(0xF5);
        Wire.endTransmission();
        
        scheduler.setTimeout([] {
          Wire.requestFrom(SHT20_ADDRESS, 3);
          if(Wire.available() >= 3) {
            uint16_t raw = Wire.read() << 8;
            raw |= Wire.read();
            humidity = -6.0f + (125.0f * raw / 65536.0f);
          } else {
            humidity = -999;
          }
        }, 40);
      } else {
        tempC = -999;
      }
    }, 100); 
  }, 500);
}
void setup_wifi(){
	WiFi.disconnect(true);
  WiFi.setHostname(deviceNetName);  
	delay(50);
  if(networkSettings.mode == NetworkMode::AP) {
    WiFi.mode(WIFI_AP);
    dnsServer.setTTL(3600);
	  dnsServer.start(53, "*", apIP);// Redirect all DNS requests to the AP IP
    WiFi.softAPConfig(apIP, apGateway, apSubnet);
    WiFi.softAP(deviceSSID);
    WiFi.setAutoReconnect(true);
  }else if (networkSettings.mode == NetworkMode::CLI){
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
      setNetworkSettings({NetworkMode::AP,"",""});
      ESP.restart();
    }
  }else {
    Serial.println("unknown network mode. check settings.");
    setNetworkSettings({NetworkMode::AP,"",""});
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
  //AP Redirects for captive portals
  if(networkSettings.mode == NetworkMode::AP){
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect(localAPIPURL); });	// windows 11 workaround
    server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)
    server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localAPIPURL); });		   // android redirect
    server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localAPIPURL); });			   // microsoft redirect
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localAPIPURL); });  // apple call home
    server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localAPIPURL); });	   // firefox  call home
    server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox call home
    server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localAPIPURL); });			   //windows call home
  }
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
    request->send(200, "application/json", "{\"threshold\":" + String(getThreshold(), 1) + "}");
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
    setThreshold(newThreshold);
    request->send(200, "text/plain", "Threshold updated to " + String(getThreshold(), 1) + "%");
  });

  server.on("/min-vent-time", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"min_vent_time\":" + String(getMinVentTime()) + "}");
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
    
    setMinVentTime(newTime);
    request->send(200, "text/plain", "Minimum vent time updated to " + String(getMinVentTime()) + "s");
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
    {setup_scheduler,"Scheduler Err handling"},
    //{setup_oled, "OLED Display and Rendering"},
    {setup_eeprom, "EEPROM"},
    {setup_sensors, "Sensors and Estimations"},
    //{setup_ventTimeoutHandling, "vent Timeout Handling"},
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
  //TEST
  //mem tracker
  scheduler.setInterval([]() {
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes, Min heap: ");
    Serial.print(ESP.getMinFreeHeap());
    Serial.print(" bytes, Max alloc heap: ");
    Serial.print(ESP.getMaxAllocHeap());
    Serial.println(" bytes");
  },1000);
 
  Serial.println("DEVICE SETUP COMPLETE");
}

//==========PROC==========
unsigned long lu = 0;
void loop() {
  //unsigned long cmillis = millis();
  //unsigned long delta = cmillis - lu; 
  //lu = cmillis;  

  scheduler.updateProcess(/*delta*/);
}