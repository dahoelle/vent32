#ifndef HEATER_H
#define HEATER_H

#define DISPL_WIDTH 64
#define DISPL_HEIGHT 64

#define PIN_RELAY_SIG 5
#define PIN_RELAY_VCC 4
#define PIN_RELAY_GND 3

#define PIN_SHT20_SDA 8
#define PIN_SHT20_SCL 9

#define SHT20_ADDRESS 0x40
#define SHT20_MEASURE_TEMP_HOLD 0xE3
#define SHT20_MEASURE_HUMIDITY_HOLD 0xE5
#define SHT20_SOFT_RESET 0xFE

#define EEPROM_SIZE 512
#define NET_MODE_ADDR 0
#define NET_SSID_ADDR 32          // Address for ssid (32 to 63)
#define NET_PASS_ADDR 64       // Address for pass (64 to 94)

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

//const char * rootHTML = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>SHT Controller</title><style>body{text-align:center;font-size:24px;margin:0;padding:0;height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;background-color:#121212;color:#ffffff;}button{font-size:20px;padding:10px 20px;cursor:pointer;background-color:#4caf50;color:white;border:none;border-radius:5px;transition:background-color 0.3s;}button:hover{background-color:#388e3c;}.status-container{display:flex;gap:30px;margin:20px 0;}.status-box{padding:20px;border:2px solid #444;border-radius:10px;min-width:150px;}.control-row{display:flex;gap:10px;margin:15px 0;justify-content:center;align-items:center;}#fan-status{width:20px;height:20px;border-radius:50%;margin-top:10px;}#settings-icon{position:absolute;top:20px;right:20px;font-size:30px;cursor:pointer;color:white;}#settings-popup{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%, -50%);background-color:#333;padding:20px;border-radius:10px;color:white;width:300px;text-align:center;}input{background-color:#333;color:white;border:1px solid #555;border-radius:5px;padding:15px;width:200px;margin:10px 0;}</style></head><body><h2>SHT Controller</h2><div class=\"status-container\"><div class=\"status-box\">💧 Humidity<div id=\"humidity-value\">--%</div></div><div class=\"status-box\">🌀 Fan Status<div id=\"fan-status\"></div></div></div><div class=\"control-row\"><input type=\"number\" step=\"0.1\" id=\"threshold\" placeholder=\"Humidity Threshold\"><button onclick=\"updateThreshold(event)\">Set</button></div><div class=\"control-row\"><input type=\"number\" id=\"min-vent-time\" placeholder=\"Min Vent Time (s)\"><button onclick=\"updateMinVentTime(event)\">Set</button></div><div id=\"settings-icon\" onclick=\"showSettingsPopup()\">⚙️</div><div id=\"settings-popup\"><h3>Settings</h3><p>Wi-Fi Configurations</p><form onsubmit=\"submitWifiConfig(event)\"><input type=\"text\" id=\"ssid\" placeholder=\"SSID\" required maxlength=\"32\"><input type=\"password\" id=\"passkey\" placeholder=\"Passkey\" required maxlength=\"32\"><button type=\"submit\">Save Configuration</button></form><button onclick=\"closeSettingsPopup()\" style=\"margin-top:15px;\">Close</button></div><script>function updateStatus(){fetch('/state').then(r=>r.json()).then(data=>{document.getElementById('humidity-value').textContent=`${data.humidity.toFixed(1)}%`;document.getElementById('fan-status').style.backgroundColor=data.fan_state?'#4caf50':'#ff4444';}).catch(console.error);}setInterval(updateStatus,1000);function updateThreshold(event){event.preventDefault();const value=document.getElementById('threshold').value;fetch(`/threshold?value=${value}`,{method:'PUT'}).then(r=>r.ok&&alert('Threshold updated!')).catch(console.error);}function updateMinVentTime(event){event.preventDefault();const value=document.getElementById('min-vent-time').value;fetch(`/min-vent-time?value=${value}`,{method:'PUT'}).then(r=>r.ok&&alert('Vent time updated!')).catch(console.error);}function showSettingsPopup(){document.getElementById('settings-popup').style.display='block';}function closeSettingsPopup(){document.getElementById('settings-popup').style.display='none';}function submitWifiConfig(event){event.preventDefault();const ssid=document.getElementById('ssid').value;const passkey=document.getElementById('passkey').value;if(ssid.length <= 32 && passkey.length <= 32){fetch('/wifi-config',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,passkey})}).then(response=>{if(response.ok){alert('Wi-Fi configuration saved successfully.');}else{alert('Failed to save Wi-Fi configuration.');}}).catch(error=>console.error('Error:',error));}else{alert('SSID and Passkey must be 32 characters or less.');}}</script></body></html>";
//const char * rootHTML = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>SHT Controller</title><style>body{text-align:center;font-size:24px;margin:0;padding:0;height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;background-color:#121212;color:#ffffff;}button{font-size:20px;padding:10px 20px;cursor:pointer;background-color:#4caf50;color:white;border:none;border-radius:5px;transition:background-color 0.3s;}button:hover{background-color:#388e3c;}.status-container{display:flex;gap:30px;margin:20px 0;}.status-box{padding:20px;border:2px solid #444;border-radius:10px;min-width:150px;}.control-row{display:flex;gap:10px;margin:15px 0;justify-content:center;align-items:center;}#fan-status{width:30px;height:30px;border-radius:50%;margin-top:10px;position:relative;}#fan-status::after{content:'';position:absolute;width:12px;height:12px;border-radius:50%;border:2px solid rgba(0,0,0,0.2);}#fan-status.on{background-color:#4CAF50;box-shadow:inset 0 0 0 3px #388e3c;}#fan-status.on::after{background-color:#FF4444;top:3px;right:3px;}#fan-status.off{background-color:#FF4444;box-shadow:inset 0 0 0 3px #cc0000;}#fan-status.off::after{background-color:#4CAF50;bottom:3px;left:3px;}#settings-icon{position:absolute;top:20px;right:20px;font-size:30px;cursor:pointer;color:white;}#settings-popup{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%, -50%);background-color:#333;padding:20px;border-radius:10px;color:white;width:300px;text-align:center;}input{background-color:#333;color:white;border:1px solid #555;border-radius:5px;padding:15px;width:200px;margin:10px 0;}</style></head><body><h2>SHT Controller</h2><div class=\"status-container\"><div class=\"status-box\">💧 Humidity<div id=\"humidity-value\">--%</div></div><div class=\"status-box\">🌡️ Temperature<div id=\"temperature-value\">--°C</div></div><div class=\"status-box\">🌀 Fan Status<div id=\"fan-status\" class=\"off\"></div></div></div><div class=\"control-row\"><input type=\"number\" step=\"0.1\" id=\"threshold\" placeholder=\"Humidity Threshold\"><button onclick=\"updateThreshold(event)\">Set</button></div><div class=\"control-row\"><input type=\"number\" id=\"min-vent-time\" placeholder=\"Min Vent Time (s)\"><button onclick=\"updateMinVentTime(event)\">Set</button></div><div id=\"settings-icon\" onclick=\"showSettingsPopup()\">⚙️</div><div id=\"settings-popup\"><h3>Settings</h3><p>Wi-Fi Configurations</p><form onsubmit=\"submitWifiConfig(event)\"><input type=\"text\" id=\"ssid\" placeholder=\"SSID\" required maxlength=\"32\"><input type=\"password\" id=\"passkey\" placeholder=\"Passkey\" required maxlength=\"32\"><button type=\"submit\">Save Configuration</button></form><button onclick=\"closeSettingsPopup()\" style=\"margin-top:15px;\">Close</button></div><script>function updateStatus(){fetch('/state').then(r=>r.json()).then(data=>{document.getElementById('humidity-value').textContent=`${data.humidity.toFixed(1)}%`;document.getElementById('temperature-value').textContent=`${data.tempC.toFixed(1)}°C`;const fanElement=document.getElementById('fan-status');fanElement.className=data.fan_state?'on':'off';}).catch(console.error);}setInterval(updateStatus,1000);function updateThreshold(event){event.preventDefault();const value=document.getElementById('threshold').value;fetch(`/threshold?value=${value}`,{method:'PUT'}).then(r=>r.ok&&alert('Threshold updated!')).catch(console.error);}function updateMinVentTime(event){event.preventDefault();const value=document.getElementById('min-vent-time').value;fetch(`/min-vent-time?value=${value}`,{method:'PUT'}).then(r=>r.ok&&alert('Vent time updated!')).catch(console.error);}function showSettingsPopup(){document.getElementById('settings-popup').style.display='block';}function closeSettingsPopup(){document.getElementById('settings-popup').style.display='none';}function submitWifiConfig(event){event.preventDefault();const ssid=document.getElementById('ssid').value;const passkey=document.getElementById('passkey').value;if(ssid.length <= 32 && passkey.length <= 32){fetch('/wifi-config',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,passkey})}).then(response=>{if(response.ok){alert('Wi-Fi configuration saved successfully.');}else{alert('Failed to save Wi-Fi configuration.');}}).catch(error=>console.error('Error:',error));}else{alert('SSID and Passkey must be 32 characters or less.');}}</script></body></html>";
//const char * rootHTML = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>SHT Controller</title><style>body{text-align:center;font-size:24px;margin:0;padding:0;height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;background-color:#121212;color:#ffffff;}button{font-size:20px;padding:10px 20px;cursor:pointer;background-color:#4caf50;color:white;border:none;border-radius:5px;transition:background-color 0.3s;}button:hover{background-color:#388e3c;}.status-container{display:flex;gap:30px;margin:20px 0;}.status-box{padding:20px;border:2px solid #444;border-radius:10px;min-width:150px;}.control-row{display:flex;gap:10px;margin:15px 0;justify-content:center;align-items:center;}#fan-status{width:20px;height:20px;border-radius:50%;margin-top:10px;}#settings-icon{position:absolute;top:20px;right:20px;font-size:30px;cursor:pointer;color:white;}#settings-popup{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%, -50%);background-color:#333;padding:20px;border-radius:10px;color:white;width:300px;text-align:center;}input{background-color:#333;color:white;border:1px solid #555;border-radius:5px;padding:15px;width:200px;margin:10px 0;}</style></head><body><h2>SHT Controller</h2><div class=\"status-container\"><div class=\"status-box\">💧 Humidity<div id=\"humidity-value\">--%</div></div><div class=\"status-box\">🌡️ Temperature<div id=\"tempC-value\">--°C</div></div><div class=\"status-box\">🌀 Fan Status<div id=\"fan-status\"></div></div></div><div class=\"control-row\"><input type=\"number\" step=\"0.1\" id=\"threshold\" placeholder=\"Humidity Threshold\"><button onclick=\"updateThreshold(event)\">Set</button></div><div class=\"control-row\"><input type=\"number\" id=\"min-vent-time\" placeholder=\"Min Vent Time (s)\"><button onclick=\"updateMinVentTime(event)\">Set</button></div><div class=\"control-row\"><input type=\"number\" id=\"quick-vent-seconds\" placeholder=\"Quick Vent (s)\"><button onclick=\"triggerQuickVent(event)\">Quick Vent</button></div><div id=\"settings-icon\" onclick=\"showSettingsPopup()\">⚙️</div><div id=\"settings-popup\"><h3>Settings</h3><p>Wi-Fi Configurations</p><form onsubmit=\"submitWifiConfig(event)\"><input type=\"text\" id=\"ssid\" placeholder=\"SSID\" required maxlength=\"32\"><input type=\"password\" id=\"passkey\" placeholder=\"Passkey\" required maxlength=\"32\"><button type=\"submit\">Save Configuration</button></form><button onclick=\"closeSettingsPopup()\" style=\"margin-top:15px;\">Close</button></div><script>function updateStatus(){fetch('/state').then(r=>r.json()).then(data=>{document.getElementById('humidity-value').textContent=`${data.humidity.toFixed(1)}%`;document.getElementById('tempC-value').textContent=`${data.tempC.toFixed(1)}°C`;document.getElementById('fan-status').style.backgroundColor=data.fan_state?'#4caf50':'#ff4444';}).catch(console.error);}setInterval(updateStatus,1000);function updateThreshold(event){event.preventDefault();const value=document.getElementById('threshold').value;fetch(`/threshold?value=${value}`,{method:'PUT'}).then(r=>r.ok&&alert('Threshold updated!')).catch(console.error);}function updateMinVentTime(event){event.preventDefault();const value=document.getElementById('min-vent-time').value;fetch(`/min-vent-time?value=${value}`,{method:'PUT'}).then(r=>r.ok&&alert('Vent time updated!')).catch(console.error);}function triggerQuickVent(event){event.preventDefault();const seconds=document.getElementById('quick-vent-seconds').value;fetch(`/quick-vent${seconds?`?seconds=${seconds}`:''}`,{method:'PUT'}).then(r=>r.ok&&alert('Quick vent triggered!')).catch(console.error);}function showSettingsPopup(){document.getElementById('settings-popup').style.display='block';}function closeSettingsPopup(){document.getElementById('settings-popup').style.display='none';}function submitWifiConfig(event){event.preventDefault();const ssid=document.getElementById('ssid').value;const passkey=document.getElementById('passkey').value;if(ssid.length<=32&&passkey.length<=32){fetch('/wifi-config',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,passkey})}).then(r=>{r.ok?alert('Wi-Fi configuration saved successfully.'):alert('Failed to save Wi-Fi configuration.');}).catch(e=>console.error('Error:',e));}else{alert('SSID and Passkey must be 32 characters or less.');}}</script></body></html>";

const char * rootHTML = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>SHT Controller</title><style>body{text-align:center;font-size:24px;margin:0;padding:0;height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;background-color:#121212;color:#ffffff;}button{font-size:20px;padding:10px 20px;cursor:pointer;background-color:#4caf50;color:white;border:none;border-radius:5px;transition:background-color 0.3s;}button:hover{background-color:#388e3c;}.status-container{display:flex;gap:30px;margin:20px 0;}.status-box{padding:20px;border:2px solid #444;border-radius:10px;min-width:150px;}.control-row{display:flex;gap:10px;margin:15px 0;justify-content:center;align-items:center;}#fan-status{width:20px;height:20px;border-radius:50%;margin-top:10px;}#fan-status.fan-on{background-color:#4caf50;}#fan-status.fan-off{background-color:#ff4444;}#manual-mode{padding:5px 10px;border-radius:5px;}#settings-icon{position:absolute;top:20px;right:20px;font-size:30px;cursor:pointer;color:white;}#settings-popup{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%, -50%);background-color:#333;padding:20px;border-radius:10px;color:white;width:300px;text-align:center;}input{background-color:#333;color:white;border:1px solid #555;border-radius:5px;padding:15px;width:200px;margin:10px 0;}</style></head><body><h2>SHT Controller</h2><div class=\"status-container\"><div class=\"status-box\">💧 Humidity<div id=\"humidity-value\">--%</div></div><div class=\"status-box\">🌡️ Temp<div id=\"tempC-value\">--°C</div></div><div class=\"status-box\">🌀 Fan<div id=\"fan-status\" class=\"fan-off\"></div></div></div><div class=\"control-row\"><span id=\"manual-mode\" style=\"background-color:#ff4444;\">Auto</span><input type=\"number\" id=\"vent-time\" placeholder=\"90\" value=\"90\"><button onclick=\"handleVent()\">VENT</button></div><div class=\"control-row\"><input type=\"number\" step=\"0.1\" id=\"threshold\" placeholder=\"Humidity Threshold\"><button onclick=\"updateThreshold(event)\">Set</button></div><div class=\"control-row\"><input type=\"number\" id=\"min-vent-time\" placeholder=\"Min Vent Time (s)\"><button onclick=\"updateMinVentTime(event)\">Set</button></div><div id=\"settings-icon\" onclick=\"showSettingsPopup()\">⚙️</div><div id=\"settings-popup\"><h3>Settings</h3><p>Wi-Fi Configurations</p><form onsubmit=\"submitWifiConfig(event)\"><input type=\"text\" id=\"ssid\" placeholder=\"SSID\" required maxlength=\"32\"><input type=\"password\" id=\"passkey\" placeholder=\"Passkey\" required maxlength=\"32\"><button type=\"submit\">Save Configuration</button></form><button onclick=\"closeSettingsPopup()\" style=\"margin-top:15px;\">Close</button></div><script>function updateStatus(){fetch('/state').then(r=>r.json()).then(data=>{document.getElementById('humidity-value').textContent=`${data.humidity.toFixed(1)}%`;document.getElementById('tempC-value').textContent=`${data.tempC.toFixed(1)}°C`;document.getElementById('fan-status').className=data.fan_state?'fan-on':'fan-off';document.getElementById('manual-mode').textContent=data.manual_mode?'Manual':'Auto';document.getElementById('manual-mode').style.backgroundColor=data.manual_mode?'#4caf50':'#ff4444';}).catch(console.error);}setInterval(updateStatus,500);function handleVent(){const seconds=document.getElementById('vent-time').value||90;fetch(`/vent?seconds=${seconds}`,{method:'PUT'}).then(r=>r.ok&&alert('Vent command sent!')).catch(console.error);}function updateThreshold(e){e.preventDefault();fetch(`/threshold?value=${document.getElementById('threshold').value}`,{method:'PUT'}).then(r=>r.ok&&alert('Threshold updated!')).catch(console.error);}function updateMinVentTime(e){e.preventDefault();fetch(`/min-vent-time?value=${document.getElementById('min-vent-time').value}`,{method:'PUT'}).then(r=>r.ok&&alert('Vent time updated!')).catch(console.error);}function showSettingsPopup(){document.getElementById('settings-popup').style.display='block';}function closeSettingsPopup(){document.getElementById('settings-popup').style.display='none';}function submitWifiConfig(e){e.preventDefault();const ssid=document.getElementById('ssid').value,passkey=document.getElementById('passkey').value;if(ssid.length>32||passkey.length>32)return alert('Max 32 characters!');fetch('/wifi-config',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,passkey})}).then(r=>alert(r.ok?'Saved!':'Failed!')).catch(console.error);}</script></body></html>";

extern NetworkSettings networkSettings;
void setNetworkSettings(const NetworkSettings &settings);
NetworkSettings getNetworkSettings();


#endif