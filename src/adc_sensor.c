// adc_sensor.c
#include "adc_sensor.h"
#include "ble_service.h"
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <hal/nrf_saadc.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>  // for k_uptime_get()

#define PRESSURE_THRESHOLD   250
#define TV_THRESHOLD         400
#define DEBOUNCE_INTERVAL_MS 1500
#define ADC_RESOLUTION       12
#define ADC_GAIN             ADC_GAIN_1
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION      ADC_ACQ_TIME_DEFAULT
#define PACKET_SIZE          6
#define ADC_CHANNEL_COUNT    6

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

static int16_t sample_buffers[ADC_CHANNEL_COUNT];
static struct adc_channel_cfg adc_cfgs[ADC_CHANNEL_COUNT];
static struct adc_sequence adc_sequences[ADC_CHANNEL_COUNT];

static const nrf_saadc_input_t adc_inputs[ADC_CHANNEL_COUNT] = {
    NRF_SAADC_INPUT_AIN0, // channel 0: P0.02
    NRF_SAADC_INPUT_AIN1, // channel 1: P0.03
    NRF_SAADC_INPUT_AIN7, // channel 2: P0.31
    NRF_SAADC_INPUT_AIN6, // channel 3: P0.30
    NRF_SAADC_INPUT_AIN5, // channel 4: P0.29
    NRF_SAADC_INPUT_AIN4  // channel 5: P0.28
};

static bool tv_pressed[2] = { false, false };  // [0] for btn 4, [1] for btn 5
static uint32_t last_trigger_time = 0;

void adc_sensor_init(void) {
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready!\n");
        return;
    }

    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        adc_cfgs[i] = (struct adc_channel_cfg){
            .gain             = ADC_GAIN,
            .reference        = ADC_REFERENCE,
            .acquisition_time = ADC_ACQUISITION,
            .channel_id       = i,
            .input_positive   = adc_inputs[i],
        };

        if (adc_channel_setup(adc_dev, &adc_cfgs[i]) != 0) {
            printk("Failed to set up ADC channel %d\n", i);
            return;
        }

        adc_sequences[i] = (struct adc_sequence){
            .channels    = BIT(i),
            .buffer      = &sample_buffers[i],
            .buffer_size = sizeof(sample_buffers[i]),
            .resolution  = ADC_RESOLUTION,
        };

        printk("ADC initialized on channel %d\n", i);
    }
}

bool adc_sensor_check_trigger(uint8_t *packet) {
    int pressed = -1;
    bool is_tv_release = false;
    uint32_t now = ble_get_time_since_connected();

    // Read ADC values and check triggers
    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        if (adc_read(adc_dev, &adc_sequences[i]) != 0) continue;

        int16_t value = sample_buffers[i];

        if (i < 4 && value > PRESSURE_THRESHOLD) {
            pressed = i;
            break;
        }

        if (i == 4 || i == 5) {
            int idx = i - 4;
            if (value > TV_THRESHOLD) {
                if (!tv_pressed[idx]) {
                    pressed = i;
                    tv_pressed[idx] = true;
                }
            } else {
                if (tv_pressed[idx]) {
                    pressed = i;
                    is_tv_release = true;
                    tv_pressed[idx] = false;
                }
            }
        }
    }

    if (pressed < 0) return false;
    if (now - last_trigger_time < DEBOUNCE_INTERVAL_MS) return false;
    last_trigger_time = now;

    packet[0] = (now >> 16) & 0xFF;
    packet[1] = (now >> 8) & 0xFF;
    packet[2] = now & 0xFF;
    packet[3] = (uint8_t)pressed;
    packet[4] = 0x00; // toy = 0, GSI = 1
    packet[5] = ((pressed == 4 || pressed == 5) && !is_tv_release) ? 0x01 : 0x00;

    return true;
}

int16_t adc_sensor_get_value(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= ADC_CHANNEL_COUNT) return -1;

    if (adc_read(adc_dev, &adc_sequences[sensor_id]) != 0) {
        printk("ADC read error (channel %d)\n", sensor_id);
        return -1;
    }

    return sample_buffers[sensor_id];
}
