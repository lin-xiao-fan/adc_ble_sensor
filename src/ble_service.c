#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gatt.h>
#include "ble_service.h"

static struct bt_conn *current_conn;
static uint8_t notify_value = 0;
static uint32_t bt_conn_start_time = 0;  // 加入：記錄連線時間

// 自訂的 128-bit UUID
static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x1234, 0x1234, 0x1234567890ab));

static struct bt_uuid_128 char_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0xabcdef01, 0x1234, 0x5678, 0x9abc, 0xdef012345678));

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    if (value == BT_GATT_CCC_NOTIFY && current_conn) {
        bt_gatt_notify(current_conn, attr, &notify_value, sizeof(notify_value));
    }
}

BT_GATT_SERVICE_DEFINE(simple_svc,
    BT_GATT_PRIMARY_SERVICE(&service_uuid),
    BT_GATT_CHARACTERISTIC(&char_uuid.uuid,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, &notify_value),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

// 修改 connected callback，紀錄連線後的系統時間
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (!err) {
        current_conn = bt_conn_ref(conn);
        bt_conn_start_time = k_uptime_get();  // 記錄連線當下的時間
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

void ble_init(void)
{
    bt_enable(NULL);
    bt_conn_cb_register(&conn_callbacks);

    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    };

    bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
}

// 提供一個函數讓其他地方取得連線後的時間
uint32_t ble_get_time_since_connected(void)
{
    if (!current_conn) {
        return 0;
    }
    return k_uptime_get() - bt_conn_start_time;
}

void ble_send_notify_packet(uint8_t *data, size_t len)
{
    if (current_conn) {
        bt_gatt_notify(current_conn, &simple_svc.attrs[1], data, len);
    }
}
