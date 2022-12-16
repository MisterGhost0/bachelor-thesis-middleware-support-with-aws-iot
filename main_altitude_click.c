#include "driver/gpio.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
//#include "sdkconfig.h"
#include "nvs_flash.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include <errno.h>
#include <math.h>

#include "include/wifi.h"
#include "include/mqtt_task.h"
#include "include/config.h"
#include "include/mpl3115a2.h"
#include "include/altitude.h"

static const char *TAG = "ALTITUDE-CLICK";
//static char errorTextBuffer[100];

int app_main(void){

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //Initialise Wifi 
    wifi_init_sta();

    //xTaskCreate(vTaskTemperaturePressureMeasurement, "Altitude-Click", 2048, NULL, 10, NULL);
    
    //Initialise Task
    ESP_LOGI(TAG, "CREATING ALTITUDE-CLICK TASK");
    temperaturePressureInit();
    
    return 0;
}
