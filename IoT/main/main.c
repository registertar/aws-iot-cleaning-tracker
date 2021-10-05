#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "ui.h"

static const char *TAG = "MAIN";

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
}

void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    ESP_LOGW(TAG, "Disconnected from AWS IoT Core...");

    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

static bool shadowUpdateInProgress;

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                const char *pReceivedJsonDocument, void *pContextData) {
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    shadowUpdateInProgress = false;

    if(SHADOW_ACK_TIMEOUT == status) {
        ESP_LOGE(TAG, "Update timed out");
    } else if(SHADOW_ACK_REJECTED == status) {
        ESP_LOGE(TAG, "Update rejected");
    } else if(SHADOW_ACK_ACCEPTED == status) {
        ESP_LOGI(TAG, "Update accepted");
    }
}

void cleaningStatus_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    char * status = (char *) (pContext->pData);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - Cleaning Status state changed to %s", status);
    }
}

char timestampStatus[32] = "";
char clientidStatus[32] = "";
char cleaningStatus[32] = "";

void aws_iot_task(void *param) {
    IoT_Error_t rc = FAILURE;

    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

    jsonStruct_t timestampStatusActuator;
    timestampStatusActuator.cb = NULL;
    timestampStatusActuator.pKey = "timestampStatus";
    timestampStatusActuator.pData = &timestampStatus;
    timestampStatusActuator.type = SHADOW_JSON_STRING;
    timestampStatusActuator.dataLength = 32;

    jsonStruct_t clientidStatusActuator;
    clientidStatusActuator.cb = NULL;
    clientidStatusActuator.pKey = "clientidStatus";
    clientidStatusActuator.pData = &clientidStatus;
    clientidStatusActuator.type = SHADOW_JSON_STRING;
    clientidStatusActuator.dataLength = 32;

    jsonStruct_t cleaningStatusActuator;
    cleaningStatusActuator.cb = cleaningStatus_Callback;
    cleaningStatusActuator.pKey = "cleaningStatus";
    cleaningStatusActuator.pData = &cleaningStatus;
    cleaningStatusActuator.type = SHADOW_JSON_STRING;
    cleaningStatusActuator.dataLength = 32;

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    // initialize the mqtt client
    AWS_IoT_Client iotCoreClient;

    ShadowInitParameters_t sp = ShadowInitParametersDefault;
    sp.pHost = HostAddress;
    sp.port = port;
    sp.enableAutoReconnect = false;
    sp.disconnectHandler = disconnect_callback_handler;

    sp.pRootCA = (const char *)aws_root_ca_pem_start;
    sp.pClientCRT = "#";
    sp.pClientKey = "#0";
    
    #define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)
    char *client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS){
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }

    ESP_LOGI(TAG, "Device client Id: >> %s <<", client_id);

    rtc_date_t dueDate; // stores cleaning due date time
    rtc_date_t date;    // stores current date time
    /*
    date.year = 2021;
    date.month = 8;
    date.day = 14;
    date.hour = 18;
    date.minute = 39;
    date.second = 0;  
    BM8563_SetTime(&date);  // this code can be used to set the current date and time in the device for the first time
    */

    // Wait for WiFI to show as connected
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Shadow Init");

    rc = aws_iot_shadow_init(&iotCoreClient, &sp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_init returned error %d, aborting...", rc);
        abort();
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = client_id;
    scp.pMqttClientId = client_id;
    scp.mqttClientIdLen = CLIENT_ID_LEN;

    // Connect to shadow in infinite loop until connected successfully
    while(true) {
        ESP_LOGI(TAG, "Shadow Connect");
        rc = aws_iot_shadow_connect(&iotCoreClient, &scp);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, retrying...", rc);
        } else {
            ESP_LOGI(TAG, "Connected to AWS IoT Device Shadow service");
            break;  // exit from the loop
        }
    }

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_shadow_set_autoreconnect_status(&iotCoreClient, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d, aborting...", rc);
        abort();
    }

    // register delta callbacks
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &timestampStatusActuator);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &clientidStatusActuator);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &cleaningStatusActuator);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }

    //
    // Business logic starts here
    //

    // initialize cleaning due date to current time + 1 hour
    BM8563_GetTime(&dueDate);
    dueDate.hour+=1;

    // loop and publish changes
    while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
        rc = aws_iot_shadow_yield(&iotCoreClient, 200);
        if(NETWORK_ATTEMPTING_RECONNECT == rc || shadowUpdateInProgress) {
            rc = aws_iot_shadow_yield(&iotCoreClient, 1000);
            // If the client is attempting to reconnect, or already waiting on a shadow update,
            // we will skip the rest of the loop.
            continue;
        }

        // START get sensor readings + update UI

        BM8563_GetTime(&date);          // get current date time
        ui_date_label_update(date);     // show time on UI

        int timediff = (dueDate.hour * 60 + dueDate.minute) - (date.hour * 60 + date.minute);   // minutes between now and cleaning due time
        ESP_LOGI(TAG, "timediff: %d", timediff);
        if (timediff < 0)
            ui_set_led_color(0xFF0000); // set LED strips to RED if no time left
        else if (timediff < 15)
            ui_set_led_color(0xFFFF00); // set LED strips to YELLOW if 15 or less mins left (warning)
        else
            ui_set_led_color(0x00FF00); // set LED strips to GREEN otherwise (ok)

        if (timediff < 0)
            timediff = 0;
        ui_set_due_bar(timediff * 100 / 60);    // show remaining time on the progressbar as well

        // END get sensor readings


        // if room is cleaned
        if (is_cleaned_button_clicked()) { // send message only if Cleaned

            BM8563_GetTime(&dueDate);
            dueDate.hour+=1;    // update cleaning due date to current time + 1 hour

            // set values for shadow document
            sprintf(timestampStatus, "%d-%02d-%02d %02d:%02d:%02d", date.year, date.month, date.day, date.hour, date.minute, date.second);  // date time stamp
            sprintf(clientidStatus, "%s", client_id);   // IoT id
            sprintf(cleaningStatus, "CLEANED");         // Cleaning status

            // log
            ESP_LOGI(TAG, "*****************************************************************************************");
            ESP_LOGI(TAG, "On Device: timestampStatus %s", timestampStatus);
            ESP_LOGI(TAG, "On Device: clientidStatus %s", clientidStatus);
            ESP_LOGI(TAG, "On Device: cleaningStatus %s", cleaningStatus);

            // compose and update shadow document with: timestamp + clientid + cleaningstatus
            rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
            if(SUCCESS == rc) {
                rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 3,
                    &timestampStatusActuator,
                    &clientidStatusActuator,
                    &cleaningStatusActuator);
                if(SUCCESS == rc) {
                    rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                    if(SUCCESS == rc) {
                        ESP_LOGI(TAG, "Update Shadow: %s", JsonDocumentBuffer);
                        rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer,
                                                ShadowUpdateStatusCallback, NULL, 4, true);
                        shadowUpdateInProgress = true;
                    }
                }
            }
            ESP_LOGI(TAG, "*****************************************************************************************");
            ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));    // wait 1 sec, then loop
    }

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "An error occurred in the loop %d", rc);
    }

    ESP_LOGI(TAG, "Disconnecting");
    rc = aws_iot_shadow_disconnect(&iotCoreClient);

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Disconnect error %d", rc);
    }

    vTaskDelete(NULL);
}

void app_main()
{   
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    Core2ForAWS_LED_Enable(1);

    ui_init();
    initialise_wifi();

    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096*2, NULL, 5, NULL, 1);
}
