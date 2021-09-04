#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "core2forAWS.h"
#include "ui.h"

#define MAX_TEXTAREA_LENGTH 1024

static lv_obj_t *out_txtarea;
static lv_obj_t *wifi_label;
static lv_obj_t *room_label;
static lv_obj_t *date_label;
static lv_obj_t *due_bar;
static lv_obj_t *cleaned_button;
static lv_obj_t *cleaned_button_label;
bool cleaned_button_clicked = false;   // variable to store if the user clicked the Cleaned button

static char *TAG = "UI";

// sets the value of the due bar ( 0 .. 100 )
void ui_set_due_bar(int16_t value) {
    lv_bar_set_value(due_bar, value, LV_ANIM_OFF);  
}

// sets the color of the leds ( example: 0x00FF00 )
void ui_set_led_color(uint32_t color) {
    Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, color);  // setting both LED strips to the same color
    Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, color); // setting both LED strips to the same color
    Core2ForAWS_Sk6812_Show();
}

// called if the user clicks the Cleaned button
static void cleaned_button_event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        cleaned_button_clicked = true;  // store in variable that the button was clicked
        ESP_LOGI(TAG, "Done button clicked");
    }
}

// queries whether the Cleaned button is already clicked & resets its state to false
bool is_cleaned_button_clicked() {
    bool ret = cleaned_button_clicked;  // return state
    cleaned_button_clicked = false;     // set back to current state to false
    return ret;
}

static void ui_textarea_prune(size_t new_text_length){
    const char * current_text = lv_textarea_get_text(out_txtarea);
    size_t current_text_len = strlen(current_text);
    if(current_text_len + new_text_length >= MAX_TEXTAREA_LENGTH){
        for(int i = 0; i < new_text_length; i++){
            lv_textarea_set_cursor_pos(out_txtarea, 0);
            lv_textarea_del_char_forward(out_txtarea);
        }
        lv_textarea_set_cursor_pos(out_txtarea, LV_TEXTAREA_CURSOR_LAST);
    }
}

// adds text to textarea for debug purposes
void ui_textarea_add(char *baseTxt, char *param, size_t paramLen) {
    if( baseTxt != NULL ){
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        if (param != NULL && paramLen != 0){
            size_t baseTxtLen = strlen(baseTxt);
            ui_textarea_prune(paramLen);
            size_t bufLen = baseTxtLen + paramLen;
            char buf[(int) bufLen];
            sprintf(buf, baseTxt, param);
            lv_textarea_add_text(out_txtarea, buf);
        } 
        else{
            lv_textarea_add_text(out_txtarea, baseTxt); 
        }
        xSemaphoreGive(xGuiSemaphore);
    } 
    else{
        ESP_LOGE(TAG, "Textarea baseTxt is NULL!");
    }
}

// sets wifi label text and state
void ui_wifi_label_update(bool state, char *ssid){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    if (state == false) {   // if there is no wifi signal
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);  // black wifi symbol
    } 
    else{
        char buffer[100];
        sprintf (buffer, "#0000ff %s # %s", LV_SYMBOL_WIFI, ssid);  // blue wifi symbol + black wifi ssid text
        lv_label_set_text(wifi_label, buffer);
    }
    xSemaphoreGive(xGuiSemaphore);
}

// sets date label based on date value
void ui_date_label_update(rtc_date_t date){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    char label_datetext[200];
    sprintf(label_datetext, "%02d:%02d", date.hour, date.minute);   // label text shows only the current time in format e.g. 20:43
    lv_label_set_text(date_label, label_datetext);
    lv_obj_align(date_label, NULL, LV_ALIGN_IN_TOP_MID, 0, 60);     // re-aligning because text size can change based on current time
    lv_label_set_align(date_label, LV_LABEL_ALIGN_CENTER);          // re-aligning because text size can change based on current time
    xSemaphoreGive(xGuiSemaphore);
}

// initializes ui components - setting styles, and positioning UI elements with default values
void ui_init() {
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    static lv_style_t title_style;  // create title style (big font) for date_label
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_style_set_text_color(&title_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    static lv_style_t subtitle_style;   // create title style (medium size font) for cleaned_button
    lv_style_init(&subtitle_style);
    lv_style_set_text_font(&subtitle_style, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SUBTITLE);
    lv_style_set_text_color(&subtitle_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    wifi_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(wifi_label, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 6);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);  // default value until connection is ready
    lv_label_set_recolor(wifi_label, true);

    room_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(room_label, NULL, LV_ALIGN_IN_TOP_RIGHT, -150, 6);
    lv_label_set_text(room_label, "Cafeteria");     // NOTE: add room name here
    lv_label_set_recolor(room_label, true);

    date_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_add_style(date_label, LV_OBJ_PART_MAIN, &title_style);
    lv_label_set_text(date_label, "00:00");     // default value until connection is ready
    lv_obj_align(date_label, NULL, LV_ALIGN_IN_TOP_MID, 0, 60);
    lv_label_set_align(date_label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_recolor(date_label, true);

    due_bar = lv_bar_create(lv_scr_act(), NULL);
    lv_obj_set_size(due_bar, 180, 20);
    lv_obj_align(due_bar, NULL, LV_ALIGN_IN_TOP_MID, 0, 140);
    lv_bar_set_value(due_bar, 100, LV_ANIM_OFF);

    static lv_style_t cleaned_button_style;
    lv_style_set_border_color(&cleaned_button_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_style_set_border_color(&cleaned_button_style, LV_STATE_FOCUSED, LV_COLOR_GREEN);

    cleaned_button = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_add_style(cleaned_button, LV_BTN_PART_MAIN, &cleaned_button_style);
    lv_obj_set_event_cb(cleaned_button, cleaned_button_event_handler);  // cleaned_button_event_handler will be called if user clicks the button
    lv_obj_set_width(cleaned_button, 200);
    lv_obj_align(cleaned_button, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -20);

    cleaned_button_label = lv_label_create(cleaned_button, NULL);
    lv_obj_add_style(cleaned_button_label, LV_OBJ_PART_MAIN, &subtitle_style);
    lv_label_set_text(cleaned_button_label, "Cleaned");
    
    out_txtarea = lv_textarea_create(lv_scr_act(), NULL);   // for debug
    lv_obj_set_size(out_txtarea, 300, 180);
    lv_obj_align(out_txtarea, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -12);
    lv_textarea_set_max_length(out_txtarea, MAX_TEXTAREA_LENGTH);
    lv_textarea_set_text_sel(out_txtarea, false);
    lv_textarea_set_cursor_hidden(out_txtarea, true);
    lv_textarea_set_text(out_txtarea, "Starting CleaningTracker\n");
    lv_obj_set_hidden(out_txtarea, true);   // hidden for release
    
    xSemaphoreGive(xGuiSemaphore);

    Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x0000FF);    // set both LED strips to Blue until connection is ready
    Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0x0000FF);   // set both LED strips to Blue until connection is ready
    Core2ForAWS_Sk6812_Show();
}
