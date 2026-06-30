/*
 * 多档位触摸调速呼吸灯
 * 功能：LED持续呼吸灯效果，触摸引脚切换呼吸速度（1→2→3→1循环）
 * 硬件：ESP32 + 触摸引脚GPIO4 + LED(GPIO2)
 * 核心：PWM呼吸灯 + 触摸边缘检测 + 档位切换
 */

#include <Arduino.h>

// ===================== 硬件配置 =====================
const int TOUCH_PIN = 4;    // 触摸引脚（GPIO4）
const int LED_PIN = 2;      // LED引脚（GPIO2，板载LED）

// ===================== PWM配置 =====================
const int PWM_FREQ = 5000;      // PWM频率 5kHz
const int PWM_CHANNEL = 0;      // PWM通道0
const int PWM_RESOLUTION = 8;   // 8位分辨率 (0-255)

// ===================== 触摸阈值 =====================
const int TOUCH_THRESHOLD = 30;   // 低于此值判定为"被触摸"

// ===================== 呼吸灯参数 =====================
const int PWM_MIN = 0;      // 最小亮度
const int PWM_MAX = 255;    // 最大亮度

// ===================== 三档速度配置 =====================
// 用结构体定义每个档位的参数，清晰易扩展
struct SpeedConfig {
    const char* name;       // 档位名称
    int step;               // 亮度步长（每次变化量）
    int delayMs;            // 每步延时（ms）
    const char* desc;       // 描述
};

// 三档配置表
const SpeedConfig speedTable[3] = {
    { "慢速",  1,  30, "缓慢呼吸，如沉睡" },   // 1档
    { "中速",  3,  10, "正常呼吸，如静息" },   // 2档
    { "快速",  8,   3, "急促呼吸，如奔跑" }    // 3档
};

// ===================== 状态变量 =====================
int speedLevel = 1;               // 当前档位：1, 2, 3
bool ledState = false;            // LED开关状态（可选）
bool lastTouchState = false;      // 上一次触摸状态（边缘检测）
unsigned long lastToggleTime = 0; // 上次切换时间（防抖）
const unsigned long DEBOUNCE_MS = 300;  // 防抖间隔300ms

// 呼吸灯运行时变量
int brightness = 0;       // 当前亮度值
bool breathingUp = true;  // 呼吸方向：true=渐亮，false=渐暗

// ===================== 读取触摸状态 =====================
bool readTouchState() {
    int touchValue = touchRead(TOUCH_PIN);
    return (touchValue < TOUCH_THRESHOLD);
}

// ===================== 切换档位（触摸触发）====================
void toggleSpeedLevel() {
    unsigned long currentTime = millis();
    
    // 软件防抖
    if (currentTime - lastToggleTime < DEBOUNCE_MS) {
        return;
    }
    
    // 档位循环：1→2→3→1
    speedLevel++;
    if (speedLevel > 3) {
        speedLevel = 1;
    }
    
    lastToggleTime = currentTime;
    
    // 输出当前档位信息
    const SpeedConfig& cfg = speedTable[speedLevel - 1];
    Serial.println("\n========================================");
    Serial.print("[触摸] 档位切换 → ");
    Serial.print(cfg.name);
    Serial.print(" (");
    Serial.print(speedLevel);
    Serial.println("档)");
    Serial.print("       步长: ");
    Serial.print(cfg.step);
    Serial.print(" | 延时: ");
    Serial.print(cfg.delayMs);
    Serial.print("ms | ");
    Serial.println(cfg.desc);
    Serial.println("========================================");
}

// ===================== 呼吸灯PWM更新 =====================
void updateBreathing() {
    // 获取当前档位配置
    const SpeedConfig& cfg = speedTable[speedLevel - 1];
    
    // 根据方向改变亮度
    if (breathingUp) {
        // 渐亮阶段
        brightness += cfg.step;
        if (brightness >= PWM_MAX) {
            brightness = PWM_MAX;
            breathingUp = false;  // 到达最亮，开始变暗
        }
    } else {
        // 渐暗阶段
        brightness -= cfg.step;
        if (brightness <= PWM_MIN) {
            brightness = PWM_MIN;
            breathingUp = true;   // 到达最暗，开始变亮
        }
    }
    
    // 写入PWM
    ledcWrite(LED_PIN, brightness);
    
    // 延时控制呼吸速度
    delay(cfg.delayMs);
}

// ===================== Setup =====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("   多档位触摸调速呼吸灯");
    Serial.println("========================================");
    
    // 配置PWM
    ledcAttachChannel(LED_PIN, PWM_FREQ, PWM_RESOLUTION, PWM_CHANNEL);
    ledcWrite(LED_PIN, 0);  // 初始熄灭
    Serial.println("[PWM] LED通道初始化完成");
    
    // 初始化触摸状态
    lastTouchState = readTouchState();
    
    // 输出初始档位
    const SpeedConfig& cfg = speedTable[speedLevel - 1];
    Serial.print("[初始化] 当前档位: ");
    Serial.print(cfg.name);
    Serial.print(" | 步长: ");
    Serial.print(cfg.step);
    Serial.print(" | 延时: ");
    Serial.print(cfg.delayMs);
    Serial.println("ms");
    Serial.println("[提示] 触摸GPIO4引脚切换呼吸速度");
    Serial.println("========================================\n");
}

// ===================== Loop =====================
void loop() {
    // ========== 1. 触摸检测与档位切换 ==========
    bool currentTouchState = readTouchState();
    
    // 边缘检测：从未触摸 → 被触摸的瞬间
    if (currentTouchState && !lastTouchState) {
        toggleSpeedLevel();
    }
    
    // 更新上一次触摸状态
    lastTouchState = currentTouchState;
    
    // ========== 2. 持续运行呼吸灯 ==========
    updateBreathing();
}