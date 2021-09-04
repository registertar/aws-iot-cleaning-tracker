#pragma once

#include "bm8563.h"     // includes type rtc_date_t

// initializes ui components
void ui_init();

// adds text to textarea for debug purposes
void ui_textarea_add(char *txt, char *param, size_t paramLen);

// sets wifi label text and state
void ui_wifi_label_update(bool state, char *ssid);

// sets date label based on date value
void ui_date_label_update(rtc_date_t date);

// queries whether the Cleaned button is already clicked & resets its state to false
bool is_cleaned_button_clicked();

// sets the value of the due bar ( 0 .. 100 )
void ui_set_due_bar(int16_t value);

// sets the color of the leds ( example: 0x00FF00 )
void ui_set_led_color(uint32_t color);

