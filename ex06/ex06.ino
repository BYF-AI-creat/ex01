/*
 * 警车双闪灯效（双通道PWM反相）
 * 功能：两个LED灯亮度呈反相关系，平滑交替渐变
 * 硬件：ESP32 + LED_A(GPIO18) + LED_B(GPIO19)
 * 核心：双通道独立PWM + 反相算法
 */

#include <Arduino.h>

// ===================== 硬件配置 =====================
const int LED_A_PIN = 18;   // LED A 引脚（建议GPIO18）
const int LED_B_PIN = 19;   // LED B 引脚（建议GPIO19）

// ===================== PWM配置 =====================
const int PWM_FREQ = 5000;      // PWM频率 5kHz
const int PWM_RESOLUTION = 8;   // 8位分辨率 (0-255)

// 两个独立PWM通道
const int PWM_CHANNEL_A = 0;    // 通道0 → LED A
const int PWM_CHANNEL_B = 1;    // 通道1 → LED B

// ===================== 呼吸参数 =====================
const int PWM_MIN = 0;      // 最小亮度
const int PWM_MAX = 255;    // 最大亮度
const int BREATH_STEP = 1;  // 亮度步长（越小越平滑）
const int DELAY_MS = 10;    // 每步延时（越小呼吸越快）

// 运行时变量
int brightnessA = 0;        // LED A 当前亮度
bool directionUp = true;    // 方向：true=A渐亮，false=A渐暗

// ===================== Setup =====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("   警车双闪灯效 - 双通道反相PWM");
    Serial.println("========================================");
    
    // 配置两个独立PWM通道
    ledcAttachChannel(LED_A_PIN, PWM_FREQ, PWM_RESOLUTION, PWM_CHANNEL_A);
    ledcAttachChannel(LED_B_PIN, PWM_FREQ, PWM_RESOLUTION, PWM_CHANNEL_B);
    
    Serial.println("[PWM] 通道A初始化完成 (GPIO" + String(LED_A_PIN) + ", CH" + String(PWM_CHANNEL_A) + ")");
    Serial.println("[PWM] 通道B初始化完成 (GPIO" + String(LED_B_PIN) + ", CH" + String(PWM_CHANNEL_B) + ")");
    
    // 初始状态：A灭，B亮
    ledcWrite(LED_A_PIN, PWM_MIN);
    ledcWrite(LED_B_PIN, PWM_MAX);
    
    Serial.println("[提示] 两个LED将呈现反相呼吸效果");
    Serial.println("========================================\n");
}

// ===================== Loop =====================
void loop() {
    // ========== 1. 更新LED A亮度 ==========
    if (directionUp) {
        // A渐亮阶段
        brightnessA += BREATH_STEP;
        if (brightnessA >= PWM_MAX) {
            brightnessA = PWM_MAX;
            directionUp = false;  // 到达最亮，开始变暗
        }
    } else {
        // A渐暗阶段
        brightnessA -= BREATH_STEP;
        if (brightnessA <= PWM_MIN) {
            brightnessA = PWM_MIN;
            directionUp = true;   // 到达最暗，开始变亮
        }
    }
    
    // ========== 2. 计算LED B亮度（反相关系）==========
    // 核心算法：B = 255 - A
    int brightnessB = PWM_MAX - brightnessA;
    
    // ========== 3. 写入两个PWM通道 ==========
    ledcWrite(LED_A_PIN, brightnessA);
    ledcWrite(LED_B_PIN, brightnessB);
    
    // ========== 4. 调试输出（每50ms打印一次）==========
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime >= 50) {
        lastPrintTime = millis();
        Serial.print("A: ");
        Serial.print(brightnessA);
        Serial.print(" | B: ");
        Serial.print(brightnessB);
        Serial.print(" | Sum: ");
        Serial.println(brightnessA + brightnessB);  // 恒等于255
    }
    
    // ========== 5. 延时控制速度 ==========
    delay(DELAY_MS);
}