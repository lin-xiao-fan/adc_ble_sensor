#pragma once
// 防止此檔案被重複包含，多次包含只會處理一次

#include <zephyr/kernel.h>  // Zephyr核心函式庫，可能用到基礎資料型別與API

// 初始化 ADC 感測器，設定 ADC 硬體與通道
void adc_sensor_init(void);

// 檢查是否有按鈕被觸發
// 如果有觸發事件，會將事件資訊寫入 packet 陣列中，回傳 true
// 沒有事件時回傳 false
bool adc_sensor_check_trigger(uint8_t *packet);
bool adc_check_toy(uint8_t *packet);
bool adc_check_tv(uint8_t *packet);
bool adc_check_gsi(uint8_t *packet);
// 取得指定 sensor (通道) 的 ADC 數值
// sensor_id 範圍為 0 ~ 5（目前6個通道）
// 失敗時回傳 -1
int16_t adc_sensor_get_value(int sensor_id);
