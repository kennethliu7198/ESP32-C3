#include <Arduino.h>
#include <math.h>

// LED 接在 GPIO 0；光敏電阻分壓輸出接在 GPIO 3。
const uint8_t LED_PIN = 0;
const uint8_t LIGHT_SENSOR_PIN = 3;

// 若環境越亮，GPIO 3 讀值越大，設為 true；反之改成 false。
// 光敏電阻接在分壓電路的上方或下方，會造成讀值方向不同。
const bool BRIGHTER_GIVES_HIGHER_READING = true;

const int ADC_MAX_VALUE = 4095;         // 12 位元 ADC 的最大讀值
const int MIN_RANGE_WIDTH = 120;        // 最小明暗範圍，降低小幅雜訊的影響
const float FILTER_STRENGTH = 0.15f;    // 越小越穩定，但反應越慢
const float RANGE_FOLLOW_RATE = 0.001f; // 舊的明暗極值逐漸淡出的速度
const unsigned long SAMPLE_INTERVAL_MS = 20;
const unsigned long STARTUP_CALIBRATION_MS = 1000;

float filteredReading = 0;
float trackedMinimum = 0;
float trackedMaximum = 0;
unsigned long lastSampleTime = 0;

void setup()
{
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    // ESP32-C3 的 ADC 設為 12 位元，讀值範圍是 0 到 4095。
    analogReadResolution(12);

    // 使用 8 位元 PWM，亮度可在 0（熄滅）到 255（最亮）之間調整。
    analogWriteResolution(LED_PIN, 8);
    analogWrite(LED_PIN, 0);

    // 開機先記錄一小段時間的環境光，作為初始明暗範圍。
    filteredReading = analogRead(LIGHT_SENSOR_PIN);
    trackedMinimum = filteredReading;
    trackedMaximum = filteredReading;

    const unsigned long calibrationStart = millis();
    while (millis() - calibrationStart < STARTUP_CALIBRATION_MS)
    {
        const int reading = analogRead(LIGHT_SENSOR_PIN);
        filteredReading = reading;

        if (filteredReading < trackedMinimum)
        {
            trackedMinimum = filteredReading;
        }
        if (filteredReading > trackedMaximum)
        {
            trackedMaximum = filteredReading;
        }

        delay(20);
    }
}

void loop()
{
    const unsigned long currentTime = millis();
    if (currentTime - lastSampleTime < SAMPLE_INTERVAL_MS)
    {
        return;
    }
    lastSampleTime = currentTime;

    // 平滑感測讀值，減少 ADC 雜訊造成的亮度抖動。
    const int reading = analogRead(LIGHT_SENSOR_PIN);
    filteredReading += FILTER_STRENGTH * (reading - filteredReading);

    // 新的極值立即納入；長時間沒有再出現的舊極值則慢慢往目前值靠近。
    if (filteredReading < trackedMinimum)
    {
        trackedMinimum = filteredReading;
    }
    else
    {
        trackedMinimum += RANGE_FOLLOW_RATE * (filteredReading - trackedMinimum);
    }

    if (filteredReading > trackedMaximum)
    {
        trackedMaximum = filteredReading;
    }
    else
    {
        trackedMaximum += RANGE_FOLLOW_RATE * (filteredReading - trackedMaximum);
    }

    // 明暗變化太小時，保留一個最低範圍，避免極小雜訊被放大成大幅亮度變化。
    float rangeMinimum = trackedMinimum;
    float rangeMaximum = trackedMaximum;
    if (rangeMaximum - rangeMinimum < MIN_RANGE_WIDTH)
    {
        const float center = (rangeMinimum + rangeMaximum) / 2.0f;
        rangeMinimum = center - MIN_RANGE_WIDTH / 2.0f;
        rangeMaximum = center + MIN_RANGE_WIDTH / 2.0f;

        if (rangeMinimum < 0)
        {
            rangeMinimum = 0;
            rangeMaximum = MIN_RANGE_WIDTH;
        }
        if (rangeMaximum > ADC_MAX_VALUE)
        {
            rangeMaximum = ADC_MAX_VALUE;
            rangeMinimum = ADC_MAX_VALUE - MIN_RANGE_WIDTH;
        }
    }

    // 將目前讀值換算成 0.0（暗）到 1.0（亮）。
    float lightLevel = (filteredReading - rangeMinimum) /
                       (rangeMaximum - rangeMinimum);
    lightLevel = constrain(lightLevel, 0.0f, 1.0f);

    // 依照分壓接法修正方向，確保數值越大代表環境越亮。
    if (!BRIGHTER_GIVES_HIGHER_READING)
    {
        lightLevel = 1.0f - lightLevel;
    }

    // 環境越暗，LED 越亮；平方校正讓視覺上的漸層較自然。
    const float ledBrightness = 1.0f - lightLevel;
    const int pwmValue = lroundf(ledBrightness * ledBrightness * 255.0f);
    analogWrite(LED_PIN, pwmValue);
}