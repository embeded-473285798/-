#ifndef OLED_RSSI_DISPLAY_H
#define OLED_RSSI_DISPLAY_H

#include <stdint.h>

void oled_rssi_display_start(void);
void oled_rssi_display_update(int8_t rssi);

#endif