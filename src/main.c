//main.c
#include <zephyr/kernel.h>    // Zephyr 核心功能，如延遲等
#include "adc_sensor.h"       // ADC 感測器相關函式宣告
#include "ble_service.h"      // BLE 通訊相關函式宣告

int main(void)
{
    // 初始化 ADC 感測器硬體與設定
    adc_sensor_init();

    // 初始化 BLE 通訊模組，準備連線與資料傳輸
    ble_init();

    // 建立一個長度為6的陣列，用來存放要傳送的資料封包
	uint8_t packet[6] = {0};

    // 主迴圈，持續執行以下動作
    while (1) {
        // 如果有偵測到按鈕觸發，會將事件資料寫入 packet 陣列並回傳 true
        if ( adc_check_toy(packet)) { // 玩具的韌體用這行
        //if ( adc_check_tv(packet)) { // TV的韌體用這行
        //if ( adc_check_gsi(packet)) { // 儀器的韌體用這行

            // 有新事件觸發，透過 BLE 傳送通知封包給連線裝置
            ble_send_notify_packet(packet, sizeof(packet));

            
        }
        // 休息 100 毫秒，避免過度輪詢導致資源浪費
        k_msleep(100);
  k_msleep(200);
