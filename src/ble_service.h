#pragma once

#include <stdint.h>

void ble_init(void);

void ble_send_notify_packet(uint8_t *data, size_t len) ;

uint32_t ble_get_time_since_connected(void);
