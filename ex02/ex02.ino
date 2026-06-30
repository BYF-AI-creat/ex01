/*
 * SOS 求救信号灯
 * 功能：LED闪烁发送国际求救信号 SOS（... --- ...）
 * 硬件：ESP32 板载LED（GPIO2）
 * 串口输出：同步显示LED状态
 */

// 定义LED引脚，ESP32通常板载LED连接在GPIO 2
const int ledPin = 2;

// ===================== 摩尔斯电码参数 =====================
const int DOT_MS = 200;      // 短闪（点）时长：200ms
const int DASH_MS = 600;     // 长闪（划）时长：600ms
const int GAP_MS = 200;      // 闪灭间隔：200ms
const int LETTER_GAP_MS = 500;  // 字母间隔：500ms
const int WORD_GAP_MS = 2000;   // 单词间隔：2000ms

// ===================== Setup =====================
void setup() {
  // 初始化串口通信，设置波特率为115200
  Serial.begin(115200);
  
  // 将LED引脚设置为输出模式
  pinMode(ledPin, OUTPUT);
  
  Serial.println("\n========================================");
  Serial.println("   SOS 求救信号灯");
  Serial.println("   摩尔斯电码: ... --- ...");
  Serial.println("========================================");
}

// ===================== 发送短闪（点）====================
void sendDot() {
  digitalWrite(ledPin, HIGH);
  Serial.println("● 点 (DOT)");
  delay(DOT_MS);
  
  digitalWrite(ledPin, LOW);
  delay(GAP_MS);
}

// ===================== 发送长闪（划）====================
void sendDash() {
  digitalWrite(ledPin, HIGH);
  Serial.println("▬ 划 (DASH)");
  delay(DASH_MS);
  
  digitalWrite(ledPin, LOW);
  delay(GAP_MS);
}

// ===================== 发送字母 S (... )====================
void sendS() {
  Serial.println("\n--- 发送 S (... ) ---");
  for (int i = 0; i < 3; i++) {
    sendDot();
  }
}

// ===================== 发送字母 O (--- )====================
void sendO() {
  Serial.println("\n--- 发送 O (--- ) ---");
  for (int i = 0; i < 3; i++) {
    sendDash();
  }
}

// ===================== 发送完整 SOS =====================
void sendSOS() {
  // S: 短闪3次
  sendS();
  delay(LETTER_GAP_MS);  // 字母间隔
  
  // O: 长闪3次
  sendO();
  delay(LETTER_GAP_MS);  // 字母间隔
  
  // S: 短闪3次
  sendS();
}

// ===================== Loop =====================
void loop() {
  // 发送 SOS 信号
  sendSOS();
  
  // 单词间隔，然后重复
  Serial.println("\n========================================");
  Serial.println("   单词间隔，准备发送下一次 SOS");
  Serial.println("========================================");
  delay(WORD_GAP_MS);
}