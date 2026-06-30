/*
 * 实验9: 实时传感器Web仪表盘
 * 功能：ESP32 Web Server下发HTML页面 + 实时返回触摸传感器模拟量数值
 *        网页端使用AJAX每200ms轮询，显示实时跳动的数字和波形图
 * 硬件：ESP32开发板 + 触摸引脚（GPIO4）
 * 依赖库：ESPAsyncWebServer, AsyncTCP
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ===================== WiFi配置 =====================
const char* ssid = "YOUR_WIFI_SSID";       // 替换为你的WiFi名称
const char* password = "YOUR_WIFI_PASSWORD"; // 替换为你的WiFi密码

// ===================== 硬件引脚 =====================
const int TOUCH_PIN = 4;    // 触摸引脚（GPIO4）

// ===================== Web服务器 =====================
AsyncWebServer server(80);

// ===================== HTML仪表盘页面 =====================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 实时传感器仪表盘</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Microsoft YaHei', 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #0c0e1b 0%, #1a1c3d 50%, #0c0e1b 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      color: #fff;
      overflow: hidden;
    }
    .container {
      width: 90%;
      max-width: 600px;
      padding: 20px;
    }
    .header {
      text-align: center;
      margin-bottom: 30px;
    }
    .header h1 {
      font-size: 1.8rem;
      background: linear-gradient(90deg, #00f5ff, #7c3aed, #f472b6);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin-bottom: 8px;
    }
    .header p { color: #888; font-size: 0.9rem; }
    
    /* 仪表盘面板 */
    .dashboard {
      background: rgba(255,255,255,0.03);
      backdrop-filter: blur(20px);
      border-radius: 24px;
      padding: 40px;
      border: 1px solid rgba(255,255,255,0.08);
      box-shadow: 0 25px 50px rgba(0,0,0,0.4);
    }
    
    /* 圆形仪表盘 */
    .gauge-container {
      position: relative;
      width: 280px;
      height: 280px;
      margin: 0 auto 30px;
    }
    .gauge-bg {
      width: 100%;
      height: 100%;
      border-radius: 50%;
      background: conic-gradient(
        from 180deg,
        #00f5ff 0deg,
        #7c3aed 90deg,
        #f472b6 180deg,
        transparent 180deg
      );
      mask: radial-gradient(circle at center, transparent 55%, black 56%);
      -webkit-mask: radial-gradient(circle at center, transparent 55%, black 56%);
      transform: rotate(-90deg);
      transition: all 0.3s;
    }
    .gauge-inner {
      position: absolute;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      width: 200px; height: 200px;
      border-radius: 50%;
      background: radial-gradient(circle at 30% 30%, rgba(255,255,255,0.1), transparent 50%),
                  linear-gradient(180deg, #0c0e1b, #1a1c3d);
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      border: 1px solid rgba(255,255,255,0.05);
    }
    .gauge-value {
      font-size: 4rem;
      font-weight: bold;
      font-family: 'Courier New', monospace;
      background: linear-gradient(180deg, #fff, #888);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      line-height: 1;
      transition: all 0.2s;
    }
    .gauge-value.changed {
      animation: valuePop 0.3s ease;
    }
    @keyframes valuePop {
      0% { transform: scale(1); }
      50% { transform: scale(1.15); }
      100% { transform: scale(1); }
    }
    .gauge-unit { font-size: 0.9rem; color: #888; margin-top: 5px; }
    .gauge-label {
      font-size: 1rem;
      color: #00f5ff;
      margin-top: 8px;
      text-transform: uppercase;
      letter-spacing: 2px;
    }
    .gauge-needle {
      position: absolute;
      top: 50%; left: 50%;
      width: 4px; height: 110px;
      background: linear-gradient(to top, transparent 30%, #00f5ff 30%);
      transform-origin: bottom center;
      transform: translate(-50%, -100%) rotate(-90deg);
      border-radius: 2px;
      transition: transform 0.3s ease-out;
      filter: drop-shadow(0 0 5px rgba(0,245,255,0.5));
    }
    .gauge-center-dot {
      position: absolute;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      width: 12px; height: 12px;
      background: #00f5ff;
      border-radius: 50%;
      box-shadow: 0 0 10px rgba(0,245,255,0.8);
    }
    
    /* 统计信息卡片 */
    .info-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-bottom: 25px;
    }
    .info-card {
      background: rgba(255,255,255,0.03);
      border-radius: 12px;
      padding: 15px;
      text-align: center;
      border: 1px solid rgba(255,255,255,0.05);
    }
    .info-label {
      font-size: 0.75rem;
      color: #888;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 5px;
    }
    .info-value {
      font-size: 1.3rem;
      font-weight: bold;
      color: #00f5ff;
    }
    
    /* 波形图 */
    .chart-container {
      background: rgba(0,0,0,0.2);
      border-radius: 12px;
      padding: 15px;
      height: 150px;
      position: relative;
      overflow: hidden;
    }
    .chart-title {
      font-size: 0.75rem;
      color: #888;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 10px;
    }
    .chart-canvas { width: 100%; height: 100px; }
    
    /* 状态栏 */
    .status-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-top: 20px;
      padding-top: 15px;
      border-top: 1px solid rgba(255,255,255,0.05);
    }
    .status-indicator {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 0.85rem;
    }
    .status-dot {
      width: 8px; height: 8px;
      border-radius: 50%;
      background: #4caf50;
      box-shadow: 0 0 8px rgba(76,175,80,0.6);
      animation: pulse-dot 2s infinite;
    }
    .status-dot.disconnected {
      background: #f44336;
      box-shadow: 0 0 8px rgba(244,67,54,0.6);
      animation: none;
    }
    @keyframes pulse-dot {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    .update-time { font-size: 0.75rem; color: #666; }
    
    /* 提示信息 */
    .hint {
      text-align: center;
      margin-top: 15px;
      padding: 10px;
      border-radius: 8px;
      background: rgba(0,245,255,0.05);
      font-size: 0.8rem;
      color: #00d4ff;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>📊 ESP32 实时传感器仪表盘</h1>
      <p>触摸传感器数据实时监控面板</p>
    </div>
    
    <div class="dashboard">
      <!-- 圆形仪表盘 -->
      <div class="gauge-container">
        <div class="gauge-bg"></div>
        <div class="gauge-inner">
          <div class="gauge-value" id="sensorValue">--</div>
          <div class="gauge-unit">RAW VALUE</div>
          <div class="gauge-label">触摸传感器</div>
        </div>
        <div class="gauge-needle" id="gaugeNeedle"></div>
        <div class="gauge-center-dot"></div>
      </div>
      
      <!-- 统计信息 -->
      <div class="info-grid">
        <div class="info-card">
          <div class="info-label">最大值</div>
          <div class="info-value" id="maxValue">--</div>
        </div>
        <div class="info-card">
          <div class="info-label">最小值</div>
          <div class="info-value" id="minValue">--</div>
        </div>
        <div class="info-card">
          <div class="info-label">平均值</div>
          <div class="info-value" id="avgValue">--</div>
        </div>
        <div class="info-card">
          <div class="info-label">采样频率</div>
          <div class="info-value">5 Hz</div>
        </div>
      </div>
      
      <!-- 实时波形图 -->
      <div class="chart-container">
        <div class="chart-title">📈 实时波形图</div>
        <canvas class="chart-canvas" id="chartCanvas" width="500" height="100"></canvas>
      </div>
      
      <!-- 状态栏 -->
      <div class="status-bar">
        <div class="status-indicator">
          <div class="status-dot" id="statusDot"></div>
          <span id="connStatus">数据采集中...</span>
        </div>
        <div class="update-time" id="updateTime">--:--:--</div>
      </div>
      
      <div class="hint">
        💡 提示：将手逐渐靠近 GPIO4 引脚，观察数值变化
      </div>
    </div>
  </div>

<script>
// ===================== 数据历史 =====================
const maxHistory = 100;           // 波形图最大数据点数
let dataHistory = [];              // 数据历史数组
let maxVal = 0;                    // 最大值
let minVal = 99999;                // 最小值
let sumVal = 0;                    // 总和
let count = 0;                     // 采样次数

// ===================== 获取传感器数据（AJAX轮询）====================
function fetchSensorData() {
  fetch("/sensor")
    .then(response => {
      if (!response.ok) throw new Error("Network error");
      return response.json();
    })
    .then(data => {
      updateDisplay(data.value);
      document.getElementById("connStatus").innerHTML = "✅ 连接正常";
      document.getElementById("statusDot").className = "status-dot";
    })
    .catch(err => {
      document.getElementById("connStatus").innerHTML = "❌ 连接断开";
      document.getElementById("statusDot").className = "status-dot disconnected";
    });
}

// ===================== 更新显示 =====================
function updateDisplay(value) {
  const valueEl = document.getElementById("sensorValue");
  
  // 更新主数值（带动画效果）
  valueEl.innerHTML = value;
  valueEl.classList.remove("changed");
  void valueEl.offsetWidth;  // 触发重绘
  valueEl.classList.add("changed");
  
  // 更新统计信息
  if (value > maxVal) maxVal = value;
  if (value < minVal) minVal = value;
  sumVal += value;
  count++;
  
  document.getElementById("maxValue").innerHTML = maxVal;
  document.getElementById("minValue").innerHTML = minVal;
  document.getElementById("avgValue").innerHTML = Math.round(sumVal / count);
  
  // 更新仪表盘指针
  // 触摸值范围大约 0-100，映射到 -90deg 到 90deg
  const angle = Math.max(-90, Math.min(90, -90 + (value / 100) * 180));
  document.getElementById("gaugeNeedle").style.transform = 
    `translate(-50%, -100%) rotate(${angle}deg)`;
  
  // 更新历史数据
  dataHistory.push(value);
  if (dataHistory.length > maxHistory) {
    dataHistory.shift();
  }
  
  // 绘制波形图
  drawChart();
  
  // 更新时间
  const now = new Date();
  document.getElementById("updateTime").innerHTML = now.toTimeString().split(" ")[0];
}

// ===================== 绘制波形图 =====================
function drawChart() {
  const canvas = document.getElementById("chartCanvas");
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  
  ctx.clearRect(0, 0, w, h);
  
  if (dataHistory.length < 2) return;
  
  // 绘制网格线
  ctx.strokeStyle = "rgba(255,255,255,0.05)";
  ctx.lineWidth = 1;
  for (let i = 0; i < 5; i++) {
    let y = (h / 4) * i;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }
  
  // 绘制波形
  ctx.strokeStyle = "#00f5ff";
  ctx.lineWidth = 2;
  ctx.shadowColor = "#00f5ff";
  ctx.shadowBlur = 10;
  
  ctx.beginPath();
  const step = w / (maxHistory - 1);
  
  for (let i = 0; i < dataHistory.length; i++) {
    const x = i * step;
    // 归一化到画布高度 (假设值范围 0-100)
    const y = h - (dataHistory[i] / 100) * h;
    if (i === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  }
  ctx.stroke();
  ctx.shadowBlur = 0;
  
  // 填充区域
  ctx.fillStyle = "rgba(0,245,255,0.1)";
  ctx.lineTo(w, h);
  ctx.lineTo(0, h);
  ctx.closePath();
  ctx.fill();
  
  // 当前点高亮
  const lastX = (dataHistory.length - 1) * step;
  const lastY = h - (dataHistory[dataHistory.length - 1] / 100) * h;
  ctx.fillStyle = "#00f5ff";
  ctx.beginPath();
  ctx.arc(lastX, lastY, 4, 0, Math.PI * 2);
  ctx.fill();
}

// ===================== 初始化 =====================
document.addEventListener("DOMContentLoaded", function() {
  // 每200ms拉取一次数据 = 5Hz采样频率
  setInterval(fetchSensorData, 200);
});
</script>
</body>
</html>
)rawliteral";

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("   ESP32 实时传感器仪表盘 - 实验9");
  Serial.println("========================================");
  
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
  
  // 根路径 - 下发HTML仪表盘页面
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });
  
  // /sensor - 数据采集上报API，返回JSON格式的触摸传感器数值
  server.on("/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
    // 读取触摸传感器模拟量数值
    int touchValue = touchRead(TOUCH_PIN);
    
    // 构建JSON响应
    String json = "{";
    json += "\"value\":" + String(touchValue) + ",";
    json += "\"timestamp\":" + String(millis()) + ",";
    json += "\"unit\":\"raw\"";
    json += "}";
    
    // 发送JSON响应
    request->send(200, "application/json", json);
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
  // 异步服务器，无需在loop中处理HTTP请求
  // 触摸读取在/sensor端点被请求时执行
  
  // 每2秒打印一次触摸值到串口（调试用）
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= 2000) {
    lastPrintTime = millis();
    int touchValue = touchRead(TOUCH_PIN);
    Serial.println("[传感器] 触摸值: " + String(touchValue) + " (手靠近时值变小)");
  }
  
  delay(10);
}