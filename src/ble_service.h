#pragma once
// 防止檔案被重複包含

#include <stdint.h>  // 定義標準整數型態如 uint8_t、uint32_t 等

// 初始化 BLE (藍牙低功耗) 功能
// 主要負責設定藍牙模組、啟動服務等
void ble_init(void);

// 傳送通知封包 (Notification Packet) 給 BLE 連線的中央設備 (Central)
// data 是要傳送的資料指標，len 是資料長度
void ble_send_notify_packet(uint8_t *data, size_t len);

// 取得從 BLE 連線開始經過的時間（毫秒）
// 用來做事件的時間戳記
uint32_t ble_get_time_since_connected(void);
