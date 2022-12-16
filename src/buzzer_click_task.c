#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/ledc.h"
#include "freertos/queue.h"
//#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
//#include <hal/mcpwm_hal.h>

#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
//#include "esp_system.h"

#include "driver/gpio.h"
#include "include/wifi.h"
#include "include/mqtt_task.h"
#include "include/mqtt_receiver_task.h"
#include "include/config.h"
#include "include/buzzer.h"

#define TAG "BUZZER-CLICK"
#define ESP_TAG_0 "ESP32_GPIO_SET_DIRECTION"
#define ESP_TAG_1 "ESP32_GPIO_SET_LEVEL"
#define ERROR_TEXT_BUFFER           50 /* Adjust this buffer size for error text */
#define TASKSTACKSIZE               2048
#define HIGH 1
#define LOW 0

#define PWM_LS_TIMER_0         LEDC_TIMER_0
#define PWM_LS_MODE            LEDC_LOW_SPEED_MODE
#define PWM_LS_CH0_GPIO        GPIO_NUM_9 // Define the output GPIO
#define PWM_LS_CH0_CHANNEL     LEDC_CHANNEL_0
#define PWM_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define PWM_DUTY               (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define TEST_FADE_TIME         (1000)

#define PWM_FREQUENCY_ULTRAFAST (5000) // Frequency in Hertz. Set frequency at 5 kHz
#define PWM_FREQUENCY_FAST      (4000) // Frequency in Hertz. Set frequency at 4 kHz
#define PWM_FREQUENCY_MEDIUM    (3000) // Frequency in Hertz. Set frequency at 3 kHz
#define PWM_FREQUENCY_SLOW      (2000) // Frequency in Hertz. Set frequency at 2 kHz
#define PWM_FREQUENCY_ULTRASLOW (1000) // Frequency in Hertz. Set frequency at 1 kHz

#define ERROR_HANDLING_1(X,Y,Z) \
    esp_err_to_name_r(espRc, errorText, ERROR_TEXT_BUFFER); \
    ESP_LOGE(ESP_TAG_0, "System call failed! - Error Code: %d -> %s\n", espRc, errorText);

#define ERROR_HANDLING_2(X,Y,Z) \
    esp_err_to_name_r(espRc, errorText, ERROR_TEXT_BUFFER); \
    ESP_LOGE(ESP_TAG_1, "System call failed! - Error Code: %d -> %s\n", espRc, errorText);


//QueueHandle_t xQueueUID = NULL;
QueueHandle_t xQueueTemperature = NULL;
esp_err_t init_gpios(void);
void controllBuzzing();

/**
 * @author Batuhan Sazak
 * @brief Buzz-Click-Programm
 * @brief This function sets all required pins to the expected levels to enable a basic functionality
 * PINLAYOUT:
 * VCC -> 5V
 * GND -> GND
 * GPI-PIN -> GPIO10 
 * PWM-PIN -> GPIO9
 */

esp_err_t init_gpios(void){

    esp_err_t espRc = 0;
    char errorText[ERROR_TEXT_BUFFER];
    
    espRc = gpio_set_direction(GPIO_NUM_10, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    espRc = gpio_set_direction(GPIO_NUM_9, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    //GPI-PIN
    espRc = gpio_set_level(GPIO_NUM_10, HIGH);
    if (espRc != ESP_OK){
        ERROR_HANDLING_2(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    return 0;
}

void controllBuzzing(){

    // Prepare and then apply the PWM timer configuration
    ledc_timer_config_t pwm_LS_timer_1 = {
        .speed_mode       = PWM_LS_MODE,
        .timer_num        = PWM_LS_TIMER_0,
        .duty_resolution  = PWM_DUTY_RES, // je höher die Frequenz umso geringer der Arbeitszyklus und umgekehrt
        .freq_hz          = PWM_FREQUENCY_ULTRAFAST,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&pwm_LS_timer_1));

    // Prepare and then apply the PWM channel configuration
    ledc_channel_config_t pwm_channel_1 = {
        .speed_mode     = PWM_LS_MODE,
        .channel        = PWM_LS_CH0_CHANNEL,
        .timer_sel      = PWM_LS_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_LS_CH0_GPIO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pwm_channel_1));

    // Initialize fade service.
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    //initial alarm is off!
    ESP_ERROR_CHECK(ledc_set_fade_with_time(pwm_channel_1.speed_mode, pwm_channel_1.channel, 0, TEST_FADE_TIME));
    ESP_ERROR_CHECK(ledc_fade_start(pwm_channel_1.speed_mode, pwm_channel_1.channel, LEDC_FADE_NO_WAIT));


    xQueueTemperature = xQueueCreate(146, 146 * sizeof(char));
    if (xQueueTemperature == NULL) {
        ESP_LOGE(TAG,"Queue for Tamperature was not created and must not be used.\n");
    }else{
        setupMqttReceiverTaskTemperature(xQueueTemperature);
    }

    BaseType_t xTaskWokenByReceive = pdFALSE;

    while (true){

        ESP_LOGI(TAG,"begin of loop!\n");
        
        char queBufferTemp[150];
        if(xQueueReceive(xQueueTemperature, &queBufferTemp, portMAX_DELAY) == pdTRUE){
            //ESP_LOGI(TAG,"Received temperature value from que: %s°C\n", queBufferTemp);
            ESP_LOGI(TAG,"Received temperature value from que: %s\n", queBufferTemp);
        }else{
            ESP_LOGE(TAG,"Failed to receive data from temperature queue!\n");
        }

        char token[6];
        memcpy(token, &queBufferTemp[36], 5 );
        token[6]='\0';

        printf("token: %s\n", token);

        char *floatPtr;
        double temperature;

        temperature = strtod(token, &floatPtr);
        if (temperature == 0){
            ESP_LOGE(TAG,"Couldn't convert value!\n");
        }else{
            ESP_LOGI(TAG,"The number(double) is %.2lf\n", temperature);
            ESP_LOGI(TAG,"String part is |%s|", floatPtr); 
        }

        if (temperature >= 23.00){
            int i = 0;
            for(;;){
                ESP_LOGI(TAG,"Temperature before ISR: %.2lf\n", temperature);

                ESP_ERROR_CHECK(ledc_set_fade_with_time(pwm_channel_1.speed_mode, pwm_channel_1.channel, PWM_DUTY, TEST_FADE_TIME));
                ESP_ERROR_CHECK(ledc_fade_start(pwm_channel_1.speed_mode, pwm_channel_1.channel, LEDC_FADE_NO_WAIT));
                vTaskDelay(1500 / portTICK_PERIOD_MS);
                ESP_ERROR_CHECK(ledc_set_fade_with_time(pwm_channel_1.speed_mode, pwm_channel_1.channel, 0, TEST_FADE_TIME));
                ESP_ERROR_CHECK(ledc_fade_start(pwm_channel_1.speed_mode, pwm_channel_1.channel, LEDC_FADE_NO_WAIT));
                vTaskDelay(500 / portTICK_PERIOD_MS);
                
                ESP_LOGI(TAG, "%d: Attention! Alarm is on because the temperature is too high!\n",++i);

                if (xQueueReceiveFromISR(xQueueTemperature, &queBufferTemp, &xTaskWokenByReceive) == pdTRUE){ 
  
                    memcpy(token, &queBufferTemp[36], 5 );
                    token[6]='\0';

                    temperature = strtod(token, &floatPtr);
                    
                    printf("token after ISR: %s\n", token);

                    if (temperature == 0){
                        ESP_LOGE(TAG,"Couldn't convert value!\n");
                    }else{
                        ESP_LOGI(TAG,"Temperature after ISR: %.2lf\n", temperature); 
                        if (temperature < 23.00){
                            break;
                        }
                    } 
                } 
            }
        }
        if (temperature < 23.00){
            
            ESP_LOGI(TAG, "Alarm is off now. Temperature is on normal level...\n");
            ESP_ERROR_CHECK(ledc_set_fade_with_time(pwm_channel_1.speed_mode, pwm_channel_1.channel, 0, TEST_FADE_TIME));
            ESP_ERROR_CHECK(ledc_fade_start(pwm_channel_1.speed_mode, pwm_channel_1.channel, LEDC_FADE_NO_WAIT));  
        }
    }
}

void vTask1(){ 
    init_gpios();
    controllBuzzing();
    //vTaskDelete(NULL);
}

void buzzInit(void){
    xTaskCreate(vTask1, "BUZZ-CLICK", TASKSTACKSIZE, NULL, 9, NULL);
}
