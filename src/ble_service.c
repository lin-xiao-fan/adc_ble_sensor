//ble_service.c
#include <zephyr/kernel.h>                    // Zephyr核心API，k_uptime_get()等
#include <zephyr/bluetooth/bluetooth.h>      // 藍牙核心API
#include <zephyr/bluetooth/hci.h>            // 藍牙控制器介面
#include <zephyr/bluetooth/gatt.h>           // GATT 服務與特徵相關API
#include "ble_service.h"                      // 對應的 header

static struct bt_conn *current_conn;         // 紀錄目前連線物件
static uint8_t notify_value = 0;              // 通知特徵值，這裡用簡單數值
static uint32_t bt_conn_start_time = 0;      // 紀錄連線開始的系統時間（ms）

// 自訂128-bit UUID，用來定義服務ID (Service UUID)
static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x1234, 0x1234, 0x1234567890ab));

// 自訂128-bit UUID，用來定義特徵ID (Characteristic UUID)
static struct bt_uuid_128 char_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0xabcdef01, 0x1234, 0x5678, 0x9abc, 0xdef012345678));

// CCC（Client Characteristic Configuration）變更回調函式
// 用來偵測中央設備是否開啟通知（Notify）
static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    // 如果中央設備啟用通知且目前有連線
    if (value == BT_GATT_CCC_NOTIFY && current_conn) {
        // 發送一次通知給中央設備，內容為 notify_value
        bt_gatt_notify(current_conn, attr, &notify_value, sizeof(notify_value));
    }
}

// 定義 GATT 服務及特徵
BT_GATT_SERVICE_DEFINE(simple_svc,
    BT_GATT_PRIMARY_SERVICE(&service_uuid),  // 主要服務
    BT_GATT_CHARACTERISTIC(&char_uuid.uuid,  // 服務裡的特徵，支持通知
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, &notify_value),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),  // 註冊通知設定的回調
);

// 連線事件回調，連線成功時觸發
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (!err) {
        current_conn = bt_conn_ref(conn);    // 取得連線引用，保留連線物件
        bt_conn_start_time = k_uptime_get(); // 紀錄連線時的系統時間（毫秒）
    }
}

// 斷線事件回調
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (current_conn) {
        bt_conn_unref(current_conn);         // 釋放連線物件
        current_conn = NULL;
    }
}

// 註冊連線與斷線的callback結構
static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

// 初始化 BLE 功能
void ble_init(void)
{
    bt_enable(NULL);                         // 啟用藍牙協定棧

    bt_conn_cb_register(&conn_callbacks);   // 註冊連線回調

    // 廣播資料 (Advertising data)
    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)), // 一般廣播，不支援經典藍牙
    };

    // 開始廣播，使用名稱與上述廣播資料
    bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
}

// 取得從連線開始到目前的經過時間(ms)
// 如果沒有連線則回傳0
uint32_t ble_get_time_since_connected(void)
{
    if (!current_conn) {
        return 0;
    }
    return k_uptime_get() - bt_conn_start_time;
}

// 傳送通知封包給目前連線的中央設備
void ble_send_notify_packet(uint8_t *data, size_t len)
{
    if (current_conn) {
        // 傳送通知給服務中索引為1的屬性 (即特徵值)
        bt_gatt_notify(current_conn, &simple_svc.attrs[1], data, len);
    }
}
