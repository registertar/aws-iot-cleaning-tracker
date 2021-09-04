#pragma once

#include "bm8563.h"

void ui_init();
void ui_textarea_add(char *txt, char *param, size_t paramLen);
void ui_wifi_label_update(bool state, char *ssid);
void ui_date_label_update(rtc_date_t date);
bool is_done_button_clicked();
void ui_set_due_bar(int16_t value);
void ui_set_led_color(uint32_t color);

