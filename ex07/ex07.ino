#include <WiFi.h>
#include <WebServer.h>

// WiFi配置
const char* ssid     = "你的WiFi名称";
const char* password = "你的WiFi密码";

// 创建Web服务器，端口80
WebServer server(80);

// LED引脚与PWM参数
const int ledPin = 2;  // ESP32板载LED，也可以改为其他引脚
int ledValue = 0;      // 亮度值 0~255

// 网页HTML内容，内置滑动条+JS请求
const char* htmlPage = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>无极调光器</title>
</head>
<body style="text-align:center;margin-top:100px;">
    <h1>LED亮度调节</h1>
    <p>当前亮度值：<span id="valueShow">0</span></p>
    <!-- 滑动条 0~255 -->
    <input type="range" id="slider" min="0" max="255" value="0" style="width:80%;height:30px;">

    <script>
        let slider = document.getElementById("slider");
        let show = document.getElementById("valueShow");

        // 监听滑动条变化
        slider.oninput = function(){
            let val = this.value;
            show.innerText = val;
            // GET请求把数值发给ESP32
            fetch("/set?bright="+val);
        }
    </script>
</body>
</html>
)HTML";

// 根路径，返回网页
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// 处理亮度设置请求
void handleSet() {
  if (server.hasArg("bright")) {
    ledValue = server.arg("bright").toInt();
    ledValue = constrain(ledValue, 0, 255); // 限制范围0~255
    analogWrite(ledPin, ledValue);
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  pinMode(ledPin, OUTPUT);
  analogWrite(ledPin, 0);

  Serial.begin(115200);
  // 连接WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("ESP32 IP地址：");
  Serial.println(WiFi.localIP());

  // 注册网页路由
  server.on("/", handleRoot);
  server.on("/set", handleSet);

  // 启动Web服务
  server.begin();
}

void loop() {
  server.handleClient();
}