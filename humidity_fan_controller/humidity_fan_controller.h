/* build.ini
build_flags = 
    -DUSE_ARDUINO_EEPROM_LIB
    -Wl,--gc-sections
    -fno-exceptions

Arduino IDE users:

    Tools > Optimization: "Smallest Code (-Os)"

    Tools > Partition Scheme: "Minimal SPIFFS"

*/

#ifndef HEATER_H
#define HEATER_H

#define DISPL_WIDTH 64
#define DISPL_HEIGHT 64

#define PIN_RELAY_SIG 5

#define PIN_SHT20_SDA 8
#define PIN_SHT20_SCL 9

#define SHT20_ADDRESS 0x40
#define SHT20_MEASURE_TEMP_HOLD 0xE3
#define SHT20_MEASURE_HUMIDITY_HOLD 0xE5
#define SHT20_SOFT_RESET 0xFE

#define EEPROM_SIZE 512
#define NET_MODE_ADDR 0
#define NET_SSID_ADDR 32          // Address for ssid (32 to 63)
#define NET_PASS_ADDR 64          // Address for pass (64 to 94)
#define THRESHOLD_ADDR 128        // Address for humidity threshold (float, 4 bytes)
#define MIN_VENT_TIME_ADDR 132    // Address for min vent time (unsigned int, 2 bytes)
#define THRESHOLD_ADDR 128        // 4 bytes (float)
#define MIN_VENT_TIME_ADDR 132    // 4 bytes (uint32_t)

#include <vector>
#include <functional>
#include <Arduino.h>
#include <Wire.h> 
#include <ESP.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <NTPClient.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <ESPAsyncWebSrv.h>

struct SetupTask {
  std::function<void()> func;
  const char * description;
};
enum SensorState { IDLE, TEMP_REQUESTED, HUMIDITY_REQUESTED };
enum NetworkMode { //stored as byte
  AP = 0,    // Access Point mode, default
  CLI = 1    // Client mode (assumes wifi set)
};
struct NetworkSettings {
  byte mode;
  char ssid[32];    
  char pass[32];     
};
const char * deviceDNSName = "sht_controller";
const char * deviceNetName = "sht_controller";
const char * deviceSSID = "SHT Controller";

const String localAPIPURL = "http://192.168.0.1:80/";	

const IPAddress apIP(192, 168, 0, 1); // Static IP for the AP
const IPAddress apGateway(192, 168, 0, 1); // Gateway IP
const IPAddress apSubnet(255, 255, 255, 0); // Subnet mask


const char rootHTML[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SHT Controller</title><style>
  body{text-align:center;font-size:24px;margin:0;padding:0;height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;background:#121212;color:#fff}
  h2{font-size:48px;font-weight:800;margin:20px 0}
  button{font-size:20px;padding:10px 20px;cursor:pointer;background:#4caf50;color:#fff;border:0;border-radius:5px;transition:.3s}
  button:hover{background:#388e3c}
  .status-container{display:flex;gap:30px;margin:20px 0}
  .status-box{padding:20px;border:2px solid #444;border-radius:10px;min-width:150px}
  .control-row{display:flex;gap:10px;margin:15px 0;justify-content:center;align-items:center}
  #fan-status{width:20px;height:20px;border-radius:50%;margin-top:10px}
  #fan-status.fan-on{background:#4caf50}
  #fan-status.fan-off{background:#ff4444}
  #manual-mode{padding:5px 10px;border-radius:5px;transition:0.3s}
  #settings-icon{position:absolute;top:20px;right:20px;font-size:30px;cursor:pointer;color:#fff}
  #settings-popup{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:#333;padding:20px;border-radius:10px;color:#fff;width:300px;text-align:center}
  input{background:#333;color:#fff;border:1px solid #555;border-radius:5px;padding:15px;width:200px;margin:10px 0}
  #toast{position:fixed;top:20px;left:20px;background:#444;color:#fff;padding:15px 25px;border-radius:8px;opacity:0;transform:translateY(-100%);transition:0.3s;pointer-events:none}
  #toast.show{opacity:1;transform:translateY(0)}
</style></head><body>
  <h2>SHT Ccontroller</h2>
  <div id="toast"></div>
  <div class="status-container">
    <div class="status-box">💧 Humidity<div id="humidity-value">--%</div></div>
    <div class="status-box">🌡️ Temp<div id="tempC-value">--°C</div></div>
    <div class="status-box">🌀 Fan<div id="fan-status" class="fan-off"></div></div>
  </div>
  <div class="control-row">
    <span id="manual-mode" style="background:#ffa726">Auto</span>
    <input type="number" id="vent-time" placeholder="90" value="90">
    <button onclick="handleVent()">VENT</button>
  </div>
  <div class="control-row">
    <input type="number" step="0.1" id="threshold" placeholder="Humidity Threshold">
    <button onclick="updateThreshold(event)">Set</button>
  </div>
  <div class="control-row">
    <input type="number" id="min-vent-time" placeholder="Min Vent Time (s)">
    <button onclick="updateMinVentTime(event)">Set</button>
  </div>
  <div id="settings-icon" onclick="showSettingsPopup()">⚙️</div>
  <div id="settings-popup">
    <h3>Settings</h3>
    <p>Wi-Fi Configurations</p>
    <form onsubmit="submitWifiConfig(event)">
      <input type="text" id="ssid" placeholder="SSID" required maxlength="32">
      <input type="password" id="passkey" placeholder="Passkey" required maxlength="32">
      <button type="submit">Save Configuration</button>
    </form>
    <button onclick="closeSettingsPopup()" style="margin-top:15px">Close</button>
  </div>
  <script>
    function showToast(message) {
      const toast = document.getElementById('toast');
      toast.textContent = message;
      toast.classList.add('show');
      setTimeout(() => toast.classList.remove('show'), 2000);
    }

    function updateStatus(){
      fetch('/state').then(r=>r.json()).then(data=>{
        document.getElementById('humidity-value').textContent=`${data.humidity.toFixed(1)}%`;
        document.getElementById('tempC-value').textContent=`${data.tempC.toFixed(1)}°C`;
        document.getElementById('fan-status').className = data.fan_state ? 'fan-on' : 'fan-off';
        document.getElementById('manual-mode').textContent = data.manual_mode ? 'Manual' : 'Auto';
        document.getElementById('manual-mode').style.backgroundColor = data.manual_mode ? '#42a5f5' : '#ffa726';
      }).catch(console.error)
    }
    
    setInterval(updateStatus,500);

    function handleVent(){
      const e = document.getElementById('vent-time').value || 90;
      fetch(`/vent?seconds=${e}`,{method:'PUT'})
        .then(e => e.ok && showToast('Vent command sent!'))
        .catch(console.error)
    }

    function updateThreshold(e){
      e.preventDefault();
      fetch(`/threshold?value=${document.getElementById('threshold').value}`,{method:'PUT'})
        .then(e => e.ok && showToast('Threshold updated!'))
        .catch(console.error)
    }

    function updateMinVentTime(e){
      e.preventDefault();
      fetch(`/min-vent-time?value=${document.getElementById('min-vent-time').value}`,{method:'PUT'})
        .then(e => e.ok && showToast('Min vent time updated!'))
        .catch(console.error)
    }

    // Rest of the original JS functions remain the same...
  </script>
</body></html>
)=====";



extern NetworkSettings networkSettings;
void setNetworkSettings(const NetworkSettings &settings);
NetworkSettings getNetworkSettings();
void setNetworkSettings(const NetworkSettings &settings);
NetworkSettings getNetworkSettings();
void setThreshold(float value);
float getThreshold();
void setMinVentTime(unsigned int value);
unsigned int getMinVentTime();


#endif