#include <driver/mcpwm.h>
#include <hal/mcpwm_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "driver/gpio.h"    
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/queue.h"

#include "include/wifi.h"
#include "include/mqtt_task.h"
#include "include/mqtt_receiver_task.h"
#include "include/config.h"
#include "include/stepper.h"

#define ESP_TAG_0 "ESP32_GPIO_SET_DIRECTION"
#define ESP_TAG_1 "ESP32_GPIO_SET_LEVEL"
#define ERROR_TEXT_BUFFER           50 /* Adjust this buffer size for error text */
#define TASKSTACKSIZE               2048


/**
 * @author Batuhan Sazak
 * @brief Macro functions for error handling
 */
#define ERROR_HANDLING_1(X,Y,Z) \
    esp_err_to_name_r(espRc, errorText, ERROR_TEXT_BUFFER); \
    ESP_LOGE(ESP_TAG_0, "System call failed! - Error Code: %d -> %s\n", espRc, errorText);

#define ERROR_HANDLING_2(X,Y,Z) \
    esp_err_to_name_r(espRc, errorText, ERROR_TEXT_BUFFER); \
    ESP_LOGE(ESP_TAG_1, "System call failed! - Error Code: %d -> %s\n", espRc, errorText);


int app_main() {

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();

    //xQueue1 = xQueueCreate( 32, 32 * sizeof(uint16_t) );
    //setupMqttReceiverTask(xQueue1);

    stepperInit();

    return 0;
}
