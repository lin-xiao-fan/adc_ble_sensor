// adc_sensor.h
#pragma once

#include <zephyr/kernel.h>

void adc_sensor_init(void);
bool adc_sensor_check_trigger(uint8_t *packet);
int16_t adc_sensor_get_value(int sensor_id );