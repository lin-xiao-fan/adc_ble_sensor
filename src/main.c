#include <zephyr/kernel.h>
#include "adc_sensor.h"
#include "ble_service.h"

int main(void)
{
    adc_sensor_init();
    ble_init();

	uint8_t packet[6] = {0};
    while (1) {
		
        if ( adc_sensor_check_trigger(packet)) {
            ble_send_notify_packet(packet, sizeof(packet));
        }
        k_msleep(200);
    }
}

