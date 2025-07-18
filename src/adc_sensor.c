// adc_sensor.c
#include "adc_sensor.h"
#include "ble_service.h"
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <hal/nrf_saadc.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>  // for k_uptime_get()

// 常數定義
#define PRESSURE_THRESHOLD   250       // 玩具按鈕壓力閾值
#define TV_THRESHOLD         400       // TV按鈕壓力閾值（較高）
#define DEBOUNCE_INTERVAL_MS 1500      // 去彈跳時間(毫秒)，避免重複觸發
#define ADC_RESOLUTION       12        // ADC解析度 12 bit
#define ADC_GAIN             ADC_GAIN_1
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION      ADC_ACQ_TIME_DEFAULT
#define PACKET_SIZE          6
#define ADC_CHANNEL_COUNT    6         // ADC通道數量

// 取得 ADC 裝置物件指標
static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

// 宣告ADC讀取的資料暫存區陣列，每個通道一個
static int16_t sample_buffers[ADC_CHANNEL_COUNT];

// ADC 通道設定結構陣列
static struct adc_channel_cfg adc_cfgs[ADC_CHANNEL_COUNT];

// ADC 讀取序列結構陣列
static struct adc_sequence adc_sequences[ADC_CHANNEL_COUNT];

// ADC 對應的硬體腳位 (nRF SAADC輸入腳位)
static const nrf_saadc_input_t adc_inputs[ADC_CHANNEL_COUNT] = {
    NRF_SAADC_INPUT_AIN0, // channel 0: P0.02 (玩具按鈕 1)
    NRF_SAADC_INPUT_AIN1, // channel 1: P0.03 (玩具按鈕 2)
    NRF_SAADC_INPUT_AIN7, // channel 2: P0.31 (玩具按鈕 3)
    NRF_SAADC_INPUT_AIN6, // channel 3: P0.30 (玩具按鈕 4)
    NRF_SAADC_INPUT_AIN5, // channel 4: P0.29 (TV 按鈕 1)
    NRF_SAADC_INPUT_AIN4  // channel 5: P0.28 (TV 按鈕 2)
};

// 紀錄 TV 按鈕(4與5)是否處於按下狀態，避免連續重複觸發
static bool tv_pressed[2] = { false, false };  // [0] 對應按鈕4，[1] 對應按鈕5

// 紀錄上一次觸發時間(毫秒)，用於去彈跳判斷
static uint32_t last_trigger_time = 0;

// 初始化 ADC，設定每個通道的參數並註冊
void adc_sensor_init(void) {
    // 確認 ADC 裝置是否準備好
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready!\n");
        return;
    }

    // 逐一設定 ADC 通道
    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        adc_cfgs[i] = (struct adc_channel_cfg){
            .gain             = ADC_GAIN,
            .reference        = ADC_REFERENCE,
            .acquisition_time = ADC_ACQUISITION,
            .channel_id       = i,
            .input_positive   = adc_inputs[i],
        };

        // 註冊此通道設定到 ADC 裝置
        if (adc_channel_setup(adc_dev, &adc_cfgs[i]) != 0) {
            printk("Failed to set up ADC channel %d\n", i);
            return;
        }

        // 設定 ADC 讀取序列的緩衝區與解析度
        adc_sequences[i] = (struct adc_sequence){
            .channels    = BIT(i),
            .buffer      = &sample_buffers[i],
            .buffer_size = sizeof(sample_buffers[i]),
            .resolution  = ADC_RESOLUTION,
        };

        printk("ADC initialized on channel %d\n", i);
    }
}

// 偵測是否有按鈕被按下或放開，並組成資料封包
bool adc_sensor_check_trigger(uint8_t *packet) {
    int pressed = -1;            // 紀錄哪個按鈕被觸發，-1表示沒觸發
    bool is_tv_release = false;  // TV 按鈕是否是放開狀態
    uint32_t now = ble_get_time_since_connected();  // 取得目前連線後的時間（毫秒）

    // 逐通道讀取 ADC 數值並判斷
    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        // 讀取 ADC 數值，失敗就跳過該通道
        if (adc_read(adc_dev, &adc_sequences[i]) != 0) continue;

        int16_t value = sample_buffers[i];

        // 前4個通道為玩具按鈕，只要超過壓力閾值就判定為按下
        if (i < 4 && value > PRESSURE_THRESHOLD) {
            pressed = i;
            break;
        }

        // 通道4與5是 TV 按鈕，有按下與放開狀態判斷
        if (i == 4 || i == 5) {
            int idx = i - 4;  // tv_pressed陣列索引
            if (value > TV_THRESHOLD) {
                // 按鈕目前沒被按下，改為按下，紀錄觸發
                if (!tv_pressed[idx]) {
                    pressed = i;
                    tv_pressed[idx] = true;
                }
            } else {
                // 按鈕之前是按下，現在壓力低於閾值，判定為放開
                if (tv_pressed[idx]) {
                    pressed = i;
                    is_tv_release = true;
                    tv_pressed[idx] = false;
                }
            }
        }
    }

    // 沒有任何按鈕被觸發，回傳 false
    if (pressed < 0) return false;

    // 判斷是否在去彈跳時間內，避免重複觸發
    if (now - last_trigger_time < DEBOUNCE_INTERVAL_MS) return false;

    // 更新最後觸發時間
    last_trigger_time = now;

    // 組成 BLE 傳送的資料封包
    packet[0] = (now >> 16) & 0xFF;     // 事件時間高位
    packet[1] = (now >> 8) & 0xFF;      // 事件時間中位
    packet[2] = now & 0xFF;             // 事件時間低位
    packet[3] = (uint8_t)pressed;       // 按鈕編號
    packet[4] = 0x00;                   // toy = 0, GSI = 1 (目前固定0)
    // 第5格表示 TV 按鈕開關狀態：0=關，1=開（非放開狀態才是開）
    packet[5] = ((pressed == 4 || pressed == 5) && !is_tv_release) ? 0x01 : 0x00;

    return true;
}

// 讀取指定感測器 ADC 數值
int16_t adc_sensor_get_value(int sensor_id) {
    // 檢查索引有效性
    if (sensor_id < 0 || sensor_id >= ADC_CHANNEL_COUNT) return -1;

    // 讀取 ADC，失敗回傳 -1
    if (adc_read(adc_dev, &adc_sequences[sensor_id]) != 0) {
        printk("ADC read error (channel %d)\n", sensor_id);
        return -1;
    }

    // 回傳該通道的最新 ADC 數值
    return sample_buffers[sensor_id];
}
