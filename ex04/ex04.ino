/*
 * 基于触摸传感器的"自锁"开关
 * 功能：摸一下亮，再摸一下灭（类似台灯触摸开关）
 * 硬件：ESP32 + 触摸引脚GPIO4 + LED(GPIO2)
 * 核心：边缘检测 + 软件防抖
 */

#include <Arduino.h>

// ===================== 硬件配置 =====================
const int TOUCH_PIN = 4;    // 触摸引脚（GPIO4）
const int LED_PIN = 2;      // LED引脚（GPIO2，板载LED）

// ===================== 触摸阈值 =====================
const int TOUCH_THRESHOLD = 30;   // 低于此值判定为"被触摸"
                                    // 需要根据实际环境调试：
                                    // 无触摸时通常50~100，触摸时通常10~30

// ===================== 状态变量 =====================
bool ledState = false;            // LED当前状态：false=灭, true=亮
bool lastTouchState = false;      // 上一次触摸状态：false=未触摸, true=触摸中
                                  // 用于边缘检测

// ===================== 软件防抖参数 =====================
unsigned long lastToggleTime = 0;     // 上次翻转状态的时间戳
const unsigned long DEBOUNCE_MS = 300; // 防抖间隔：300ms内忽略重复触摸
                                        // 防止手抖、手指接触不良导致的误触发

// ===================== 读取触摸状态（带阈值判断）====================
bool readTouchState() {
    int touchValue = touchRead(TOUCH_PIN);
    
    // 调试输出：观察触摸值范围，帮助确定阈值
    #ifdef DEBUG_TOUCH
    Serial.print("Touch Value: ");
    Serial.println(touchValue);
    #endif
    
    // 触摸值低于阈值 → 被触摸
    return (touchValue < TOUCH_THRESHOLD);
}

// ===================== Setup =====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("   触摸自锁开关 - 边缘检测 + 软件防抖");
    Serial.println("========================================");
    
    // 配置LED引脚
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // 初始熄灭
    Serial.println("[初始化] LED已熄灭");
    
    // 初始化触摸状态
    lastTouchState = readTouchState();
    
    Serial.println("[提示] 触摸GPIO4引脚来切换LED状态");
    Serial.println("========================================\n");
}

// ===================== Loop =====================
void loop() {
    // 1. 读取当前触摸状态
    bool currentTouchState = readTouchState();
    
    // 2. 边缘检测：检测"按下瞬间"（从未触摸 → 被触摸）
    //    即：上一次未触摸(false) && 当前被触摸(true)
    if (currentTouchState && !lastTouchState) {
        
        // 3. 软件防抖：检查是否在防抖时间内
        unsigned long currentTime = millis();
        if (currentTime - lastToggleTime >= DEBOUNCE_MS) {
            
            // ===== 翻转LED状态 =====
            ledState = !ledState;           // 取反：亮→灭，灭→亮
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
            
            // 更新时间戳
            lastToggleTime = currentTime;
            
            // 输出状态
            Serial.print("[触摸] 状态切换 → LED ");
            Serial.println(ledState ? "亮起 💡" : "熄灭 🔌");
            Serial.print("       触摸值: ");
            Serial.print(touchRead(TOUCH_PIN));
            Serial.print(" | 防抖间隔: ");
            Serial.print(DEBOUNCE_MS);
            Serial.println("ms");
        }
        else {
            // 防抖期间忽略此次触摸
            Serial.print("[忽略] 防抖中，剩余 ");
            Serial.print(DEBOUNCE_MS - (currentTime - lastToggleTime));
            Serial.println("ms");
        }
    }
    
    // 4. 更新上一次状态（为下一次边缘检测做准备）
    lastTouchState = currentTouchState;
    
    // 5. 短暂延时，降低CPU占用
    delay(10);
}