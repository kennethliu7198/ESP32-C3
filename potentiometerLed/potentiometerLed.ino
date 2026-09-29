#include <Arduino.h>
#include <math.h>

// LED 接在 GPIO 0，可變電阻中間的滑動端接在 GPIO 3。
const uint8_t LED_PIN = 0;
const uint8_t POTENTIOMETER_PIN = 3;

// 若旋鈕往你想要的「變亮」方向轉，LED 卻變暗，請改成 true。
const bool INVERT_POT_DIRECTION = false;

const float FILTER_STRENGTH = 0.2f; // 越小越穩定，但亮度反應越慢
const unsigned long SAMPLE_INTERVAL_MS = 10;

float filteredReading = 0;
unsigned long lastSampleTime = 0;

void setup()
{
    pinMode(POTENTIOMETER_PIN, INPUT);

    // ESP32-C3 的 ADC 使用 12 位元解析度，讀值範圍是 0 到 4095。
    analogReadResolution(12);

    // LED 使用 8 位元 PWM，數值 0 是熄滅，255 是最亮。
    analogWriteResolution(LED_PIN, 8);
    analogWrite(LED_PIN, 0);

    // 先用目前的旋鈕位置初始化濾波值，避免開機時亮度突然跳動。
    filteredReading = analogRead(POTENTIOMETER_PIN);
}

void loop()
{
    const unsigned long currentTime = millis();
    if (currentTime - lastSampleTime < SAMPLE_INTERVAL_MS)
    {
        return;
    }
    lastSampleTime = currentTime;

    // 讀取旋鈕位置，並平滑小幅度的電氣雜訊。
    const int reading = analogRead(POTENTIOMETER_PIN);
    filteredReading += FILTER_STRENGTH * (reading - filteredReading);

    // 將 ADC 數值轉成 0.0 到 1.0，方便換算亮度。
    float knobLevel = filteredReading / 4095.0f;

    // 如果旋鈕方向與預期相反，翻轉控制方向。
    if (INVERT_POT_DIRECTION)
    {
        knobLevel = 1.0f - knobLevel;
    }

    // 平方曲線讓亮度由暗到亮的視覺漸層更自然。
    const int pwmValue = lroundf(knobLevel * knobLevel * 255.0f);
    analogWrite(LED_PIN, pwmValue);
}