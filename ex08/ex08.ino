/*
 * 实验8: 物联网安防报警器模拟实验
 * 功能：ESP32作为安防主机，网页控制布防/撤防，触摸引脚触发锁定报警
 * 硬件：ESP32开发板 + LED（GPIO2）+ 触摸引脚（GPIO4）
 * 依赖库：ESPAsyncWebServer, AsyncTCP
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ===================== WiFi配置 =====================
const char* ssid = "YOUR_WIFI_SSID";       // 替换为你的WiFi名称
const char* password = "YOUR_WIFI_PASSWORD"; // 替换为你的WiFi密码

// ===================== 硬件引脚 =====================
const int LED_PIN = 2;      // LED引脚（GPIO2，板载LED）
const int TOUCH_PIN = 4;    // 触摸引脚（GPIO4）

// ===================== 全局变量：系统状态 =====================
enum SecurityState {
  DISARMED,     // 撤防状态
  ARMED,        // 布防状态
  ALARMING      // 报警状态（锁定）
};

SecurityState currentState = DISARMED;  // 全局状态变量，初始为撤防

// 报警闪烁控制
bool ledState = false;                  // LED当前亮灭状态
unsigned long lastBlinkTime = 0;        // 上次闪烁时间戳
const int ALARM_BLINK_INTERVAL = 100;   // 报警闪烁间隔：100ms（高频狂闪）

// 触摸检测
const int TOUCH_THRESHOLD = 30;         // 触摸阈值（越小越灵敏）
bool touchTriggered = false;            // 触摸触发标志（防止重复触发）

// ===================== Web服务器 =====================
AsyncWebServer server(80);

// ===================== HTML网页（布防/撤防控制面板）====================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 安防报警器</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Microsoft YaHei', Arial, sans-serif;
      background: linear-gradient(135deg, #0f0c29 0%, #302b63 50%, #24243e 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      color: #fff;
    }
    .container {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border-radius: 20px;
      padding: 40px;
      width: 90%;
      max-width: 420px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
      border: 1px solid rgba(255,255,255,0.1);
    }
    h1 { text-align: center; margin-bottom: 5px; font-size: 1.6rem; }
    .subtitle { text-align: center; color: #888; margin-bottom: 25px; font-size: 0.85rem; }
    
    /* 状态面板 */
    .status-panel {
      background: rgba(0,0,0,0.3);
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 25px;
      text-align: center;
      border: 2px solid transparent;
      transition: all 0.3s;
    }
    .status-icon { font-size: 4rem; margin-bottom: 10px; }
    .status-text { font-size: 1.4rem; font-weight: bold; margin-bottom: 5px; }
    .status-desc { font-size: 0.85rem; color: #aaa; }
    
    .status-disarmed { border-color: #4caf50; box-shadow: 0 0 20px rgba(76,175,80,0.2); }
    .status-armed { border-color: #ff9800; box-shadow: 0 0 20px rgba(255,152,0,0.2); }
    .status-armed .status-icon { animation: pulse 2s infinite; }
    .status-alarming { 
      border-color: #f44336; 
      box-shadow: 0 0 30px rgba(244,67,54,0.5);
      animation: shake 0.5s infinite;
    }
    .status-alarming .status-icon { animation: flash 0.2s infinite; }
    
    @keyframes pulse { 0%,100%{transform:scale(1)} 50%{transform:scale(1.1)} }
    @keyframes flash { 0%,100%{opacity:1} 50%{opacity:0.3} }
    @keyframes shake { 0%,100%{transform:translateX(0)} 25%{transform:translateX(-3px)} 75%{transform:translateX(3px)} }
    
    /* 按钮 */
    .btn-group { display: flex; gap: 15px; margin-bottom: 20px; }
    .btn {
      flex: 1;
      padding: 15px 20px;
      border: none;
      border-radius: 12px;
      font-size: 1.1rem;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s;
      color: white;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 20px rgba(0,0,0,0.3); }
    .btn:active { transform: translateY(0); }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; transform: none !important; }
    .btn-arm { background: linear-gradient(135deg, #ff9800, #f57c00); }
    .btn-disarm { background: linear-gradient(135deg, #4caf50, #388e3c); }
    
    /* 触摸数值显示 */
    .touch-indicator {
      text-align: center;
      margin-top: 15px;
      padding: 10px;
      border-radius: 8px;
      background: rgba(255,255,255,0.05);
      font-size: 0.85rem;
    }
    .touch-value { font-size: 1.5rem; font-weight: bold; color: #00d4ff; }
    
    /* 日志 */
    .log-panel {
      background: rgba(0,0,0,0.3);
      border-radius: 10px;
      padding: 15px;
      max-height: 150px;
      overflow-y: auto;
      margin-top: 20px;
    }
    .log-title { font-size: 0.8rem; color: #888; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 1px; }
    .log-entry { font-size: 0.8rem; padding: 3px 0; border-bottom: 1px solid rgba(255,255,255,0.05); color: #ccc; }
    .log-time { color: #00d4ff; margin-right: 8px; }
    .log-alarm { color: #f44336; }
    .log-arm { color: #ff9800; }
    .log-disarm { color: #4caf50; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🛡️ ESP32 安防报警器</h1>
    <p class="subtitle">物联网安防主机控制面板</p>
    
    <div class="status-panel" id="statusPanel">
      <div class="status-icon" id="statusIcon">🔓</div>
      <div class="status-text" id="statusText">已撤防</div>
      <div class="status-desc" id="statusDesc">系统处于安全状态</div>
    </div>
    
    <div class="btn-group">
      <button class="btn btn-arm" id="btnArm" onclick="armSystem()">🛡️ 布防</button>
      <button class="btn btn-disarm" id="btnDisarm" onclick="disarmSystem()" disabled>🔓 撤防</button>
    </div>
    
    <div class="touch-indicator">
      <div>触摸传感器数值</div>
      <div class="touch-value" id="touchValue">--</div>
    </div>
    
    <div class="log-panel">
      <div class="log-title">📋 系统日志</div>
      <div id="logPanel">
        <div class="log-entry"><span class="log-time">--:--:--</span>系统初始化完成</div>
      </div>
    </div>
  </div>

<script>
let currentState = "DISARMED";

function armSystem() {
  fetch("/arm")
    .then(response => response.text())
    .then(data => {
      updateUI("ARMED");
      addLog("🛡️ 系统已布防", "log-arm");
    });
}

function disarmSystem() {
  fetch("/disarm")
    .then(response => response.text())
    .then(data => {
      updateUI("DISARMED");
      addLog("🔓 系统已撤防", "log-disarm");
    });
}

function updateUI(state) {
  currentState = state;
  const panel = document.getElementById("statusPanel");
  const icon = document.getElementById("statusIcon");
  const text = document.getElementById("statusText");
  const desc = document.getElementById("statusDesc");
  const btnArm = document.getElementById("btnArm");
  const btnDisarm = document.getElementById("btnDisarm");
  
  panel.className = "status-panel";
  
  if (state === "DISARMED") {
    panel.classList.add("status-disarmed");
    icon.innerHTML = "🔓";
    text.innerHTML = "已撤防";
    desc.innerHTML = "系统处于安全状态";
    btnArm.disabled = false;
    btnDisarm.disabled = true;
  } else if (state === "ARMED") {
    panel.classList.add("status-armed");
    icon.innerHTML = "🛡️";
    text.innerHTML = "已布防";
    desc.innerHTML = "监控中... 触摸引脚将触发报警";
    btnArm.disabled = true;
    btnDisarm.disabled = false;
  } else if (state === "ALARMING") {
    panel.classList.add("status-alarming");
    icon.innerHTML = "🚨";
    text.innerHTML = "⚠️ 报警中！";
    desc.innerHTML = "检测到入侵！请点击撤防解除";
    btnArm.disabled = true;
    btnDisarm.disabled = false;
  }
}

function addLog(message, className) {
  const panel = document.getElementById("logPanel");
  const now = new Date();
  const time = now.toTimeString().split(" ")[0];
  const entry = document.createElement("div");
  entry.className = "log-entry";
  entry.innerHTML = '<span class="log-time">' + time + '</span><span class="' + className + '">' + message + '</span>';
  panel.insertBefore(entry, panel.firstChild);
  if (panel.children.length > 10) panel.removeChild(panel.lastChild);
}

// 定时轮询状态（每500ms）
setInterval(function() {
  fetch("/status")
    .then(response => response.json())
    .then(data => {
      document.getElementById("touchValue").innerHTML = data.touch;
      if (data.state !== currentState) {
        updateUI(data.state);
        if (data.state === "ALARMING") {
          addLog("🚨 检测到入侵！报警触发！", "log-alarm");
        }
      }
    })
    .catch(err => {
      document.getElementById("touchValue").innerHTML = "ERR";
    });
}, 500);

document.addEventListener("DOMContentLoaded", function() {
  updateUI("DISARMED");
});
</script>
</body>
</html>
)rawliteral";

// ===================== 状态JSON生成 =====================
String getStatusJSON() {
  String stateStr;
  switch(currentState) {
    case DISARMED: stateStr = "DISARMED"; break;
    case ARMED: stateStr = "ARMED"; break;
    case ALARMING: stateStr = "ALARMING"; break;
  }
  int touchVal = touchRead(TOUCH_PIN);
  return "{\"state\":\"" + stateStr + "\",\"touch\":" + String(touchVal) + "}";
}

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("   ESP32 安防报警器 - 实验8");
  Serial.println("========================================");
  
  // 配置LED引脚为输出
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println("[GPIO] LED引脚初始化完成 (GPIO" + String(LED_PIN) + ")");
  
  // 触摸引脚不需要pinMode，直接使用touchRead()
  Serial.println("[GPIO] 触摸引脚初始化完成 (GPIO" + String(TOUCH_PIN) + ")");
  
  // 连接WiFi
  Serial.print("[WiFi] 正在连接: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retryCount++;
    if (retryCount > 40) {
      Serial.println("\n[WiFi] 连接超时，请检查SSID和密码！");
      ESP.restart();
    }
  }
  
  Serial.println("\n[WiFi] 连接成功！");
  Serial.print("[WiFi] IP地址: ");
  Serial.println(WiFi.localIP());
  
  // ========== Web服务器路由 ==========
  
  // 根路径 - 返回主页面
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });
  
  // 布防请求
  server.on("/arm", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentState == DISARMED) {
      currentState = ARMED;
      touchTriggered = false;  // 重置触摸标志
      digitalWrite(LED_PIN, LOW);
      Serial.println("[安防] 🛡️ 系统已布防");
      request->send(200, "text/plain", "ARMED");
    } else {
      request->send(200, "text/plain", "ALREADY_ARMED");
    }
  });
  
  // 撤防请求 - 唯一能解除报警的方式
  server.on("/disarm", HTTP_GET, [](AsyncWebServerRequest *request) {
    currentState = DISARMED;
    digitalWrite(LED_PIN, LOW);      // LED熄灭
    touchTriggered = false;           // 重置触摸标志
    Serial.println("[安防] 🔓 系统已撤防，报警解除");
    request->send(200, "text/plain", "DISARMED");
  });
  
  // 状态查询（JSON）
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getStatusJSON());
  });
  
  // 启动服务器
  server.begin();
  Serial.println("[HTTP] Web服务器已启动");
  Serial.println("========================================");
  Serial.println("请在浏览器中访问: http://" + WiFi.localIP().toString());
  Serial.println("========================================\n");
}

// ===================== Loop =====================
void loop() {
  // 读取触摸传感器原始值
  int touchValue = touchRead(TOUCH_PIN);
  
  // 检测触摸：值低于阈值表示有触摸（电容式触摸，值越小表示电容越大）
  bool isTouched = (touchValue < TOUCH_THRESHOLD);
  
  // ========== 核心逻辑：布防状态下触摸触发报警（锁定）==========
  if (currentState == ARMED && isTouched && !touchTriggered) {
    touchTriggered = true;           // 标记已触发（防止重复触发）
    currentState = ALARMING;          // 切换到报警状态（锁定！）
    alarmStartTime = millis();        // 记录报警开始时间
    Serial.println("[安防] 🚨 报警触发！检测到触摸入侵！");
    Serial.println("       触摸值: " + String(touchValue) + " (阈值: " + String(TOUCH_THRESHOLD) + ")");
    Serial.println("       ⚠️ 报警已锁定！请通过网页点击撤防解除");
  }
  
  // ========== 报警状态：LED高频闪烁（即使松手也不停止）==========
  if (currentState == ALARMING) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= ALARM_BLINK_INTERVAL) {
      lastBlinkTime = currentTime;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
  
  // 调试输出：每2秒打印一次状态
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= 2000) {
    lastPrintTime = millis();
    String stateStr;
    switch(currentState) {
      case DISARMED: stateStr = "撤防"; break;
      case ARMED: stateStr = "布防"; break;
      case ALARMING: stateStr = "报警"; break;
    }
    Serial.println("[状态] " + stateStr + " | 触摸值: " + String(touchValue) + 
                   " | 触摸阈值: " + String(TOUCH_THRESHOLD));
  }
  
  delay(10);  // 短暂延时，避免看门狗复位
}