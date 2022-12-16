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
#include "driver/gpio.h"    
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/queue.h"

#include "include/wifi.h"
#include "include/mqtt_receiver_task.h"
#include "include/config.h"
#include "include/stepper.h"

#define TAG_MOTOR "STEPPER-MOTOR-CLICK"
#define ESP_TAG_0 "ESP32_GPIO_SET_DIRECTION"
#define ESP_TAG_1 "ESP32_GPIO_SET_LEVEL"
#define ERROR_TEXT_BUFFER           50 /* Adjust this buffer size for error text */
#define TASKSTACKSIZE               2048
#define HIGH 1
#define LOW 0

#define OPENVENTILE 0
#define CLOSEVENTILE 1

QueueHandle_t xQueueUID = 0;

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

esp_err_t set_gpio_dir_and_lvl(void);
esp_err_t rotate_motor(void);
void vTaskControlSprinkleSystem(void * parameter);

/**
 * @author Batuhan Sazak
 * @brief This function defines all required pins to control the step motor as on output and also sets their levels to Low
 * 
 * @brief Pin Layouts
 * MS1 = PIN25 - Allow adjustments to the microstepping resolution and
 * MS2 = PIN26   change microstepping resolution to full, half, quarter and eighth steps.
 * DIR = PIN27 - This pin will allow to determine the direction of motor rotation (Hi - Lo or Lo - Hi).
 * EN  = PIN32 - Enable FET functionality within the motor driver. If Hi -> FET disable, IC will not drive motor.
 * STEP = PIN33 - Any transition on this pin from Lo - Hi will trigger the motor to step forward one step.
 * 
 * MS1 MS2 Resolution:
 * Low Low -> Full step (2 phase)
 * High Low -> Half step
 * Low High Quarter step
 * High High Eighth step
 */
esp_err_t set_gpio_dir_and_lvl(void){
    
    esp_err_t espRc = 0;
    char errorText[ERROR_TEXT_BUFFER];

    espRc = gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    espRc = gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }
    
    espRc = gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    espRc = gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    espRc = gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    if (espRc != ESP_OK){
        ERROR_HANDLING_1(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    //EN-PIN
    espRc = gpio_set_level(GPIO_NUM_32, LOW);
    if (espRc != ESP_OK){
        ERROR_HANDLING_2(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    //MS1-PIN
    espRc = gpio_set_level(GPIO_NUM_25, LOW);
    if (espRc != ESP_OK){
        ERROR_HANDLING_2(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    //MS2-PIN
    espRc = gpio_set_level(GPIO_NUM_26, LOW);
    if (espRc != ESP_OK){
        ERROR_HANDLING_2(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    return 0;
}

/**
 * @author Batuhan Sazak
 * @brief This function rotates the step motor. Basically one single step is made by setting a 1 and directly 0 to the GPIO 33. 
 * The defined MS1 and MS2 resolutions above will have an impact at the step resolution. All of these consecutively steps will cause an rotation 
 * 
 */
esp_err_t rotate_motor(void){
    esp_err_t espRc = 0;
    char errorText[ERROR_TEXT_BUFFER];

    //STEP-PIN
    espRc = gpio_set_level(GPIO_NUM_33, 1);
    if (espRc != ESP_OK){
        ERROR_HANDLING_2(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }

    espRc = gpio_set_level(GPIO_NUM_33, 0);
    if (espRc != ESP_OK){
        ERROR_HANDLING_2(espRc,errorText,ERROR_TEXT_BUFFER);
        return espRc;
    }
    return 0;
}

/**
 * @author Batuhan Sazak
 * @brief This is the task function. This function controls the step motor by receiving the specific control value 
 * from the MQTT-Broker. The received data from the MQTT-Broker can be a cmd=0 or cmd=1. The value 0 will cause an opening cycle
 * and 1 will cause an closing cycle.
 */
void controlSprinkleSystem(){

    set_gpio_dir_and_lvl();
    char queBufferTag[10];

    char blueTagUid[] = "138D9380";
    char whiteCardUid[] = "CC6B979B";

    xQueueUID = xQueueCreate(8, 8 * sizeof(char));
    if (xQueueUID == NULL) {
        ESP_LOGE(TAG_MOTOR, "Queue for UID was not created and must not be used.\n");
        
    }else{
        setupMqttReceiverTask(xQueueUID);
    }

    BaseType_t xTaskWokenByReceive = pdFALSE;


    while (true){

        ESP_LOGI(TAG_MOTOR,"Begin of function loop! Sprinkle System is ready to use!\n");

        int cyleCounter = 0;

        if (xQueueReceive(xQueueUID, &queBufferTag, portMAX_DELAY) == pdTRUE){
            ESP_LOGI(TAG_MOTOR,"Received UID from que: %s\n", queBufferTag);
        }else{
            ESP_LOGE(TAG_MOTOR,"Failed to receive data from UID queue!\n");
        }
        

        if (strcmp(queBufferTag, blueTagUid) == 0){
            gpio_set_level(GPIO_NUM_27, LOW);
            ESP_LOGI(TAG_MOTOR,"Ventile of sprinkle system is opened now!\n");
            while(1){
                cyleCounter++;
                rotate_motor();
                vTaskDelay(10 / portTICK_PERIOD_MS);
                ESP_LOGI(TAG_MOTOR,"Opening ventile: %d \n", cyleCounter);
                
                if (xQueueReceiveFromISR(xQueueUID, &queBufferTag, &xTaskWokenByReceive) == pdTRUE){ 
                    if (strcmp(queBufferTag, whiteCardUid) == 0){
                        ESP_LOGI(TAG_MOTOR,"Recognized card token! Closig ventile..");
                        break;
                    }
                }else if(xQueueReceiveFromISR(xQueueUID, &queBufferTag, &xTaskWokenByReceive) == pdFALSE){
                    continue;
                }  
            }
        }

        if (strcmp(queBufferTag, whiteCardUid) == 0){
            gpio_set_level(GPIO_NUM_27, HIGH);
            ESP_LOGI(TAG_MOTOR,"Ventile of sprinkle system is closed now!\n");
            while(1){
                cyleCounter++;
                rotate_motor();
                vTaskDelay(10 / portTICK_PERIOD_MS);
                ESP_LOGI(TAG_MOTOR,"Closing ventile: %d\n", cyleCounter);
                
                if (xQueueReceiveFromISR(xQueueUID, &queBufferTag, &xTaskWokenByReceive) == pdTRUE){ 
                    if (strcmp(queBufferTag, blueTagUid) == 0){
                        ESP_LOGI(TAG_MOTOR,"Recognized card token! Closig ventile..");
                        break;
                    }
                }else if(xQueueReceiveFromISR(xQueueUID, &queBufferTag, &xTaskWokenByReceive) == pdFALSE){
                    continue;
                }  
                
            }
        }
    }
}

void vTask1(){
    controlSprinkleSystem();
}

void stepperInit(void){
    xTaskCreate(vTask1, "Task-Control-Sprinkle", TASKSTACKSIZE, NULL, 9, NULL);
}
