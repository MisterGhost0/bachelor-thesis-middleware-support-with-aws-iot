#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_system.h"


#include "include/wifi.h"
#include "include/mqtt_task.h"
#include "include/config.h"
#include "include/mpl3115a2.h"
#include "include/altitude.h"


#define ESP_TAG_0                   "ESP32_I2C_PORT_MODE_CONFIG"
#define ESP_TAG_1                   "ESP32_I2C_WRITE"
#define ESP_TAG_2                   "ESP32_I2C_READ"
#define ALTITUDE_TAG                "MPL13115A2"
#define I2C_PORT_NUMBER             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 /* 1MHz transfer speed */
#define NACK_VAL                    0x1 /*!< I2C nack value */
#define ACK_VAL                     0x0  /*!< I2C ack value */
#define I2C_ACK_CHECK_EN			0x1
#define I2C_ACK_CHECK_DIS			0x0
#define I2C_SDA_GPIO                GPIO_NUM_21 /* GPIO 21 --> SDA */
#define I2C_SCL_GPIO                GPIO_NUM_22 /* GPIO 22 --> SDL */
#define ERROR_TEXT_BUFFER           50 /* Adjust this buffer size for error text */
#define TASKSTACKSIZE               2048

static const char *TAG = "ALTITUDE-CLICK";
static char errorTextBuffer[100];

esp_err_t i2c_master_init();
void writeRegister(uint8_t registerAddress, uint8_t data);
uint8_t readRegister(uint8_t registerAddress);
float getTemperature(void);
float getPressure(void);
void vTaskTemperaturePressureMeasurement(void * parameter);
void temperaturePressureInit(void);


/**
 * @author Batuhan Sazak
 * @brief This function creates an I2C Port to communicate with the slave device and defines the ESP32 uC as master device.
 */
esp_err_t i2c_master_init(){
    
    esp_err_t espRc = 0;
    char errorTextBuffer[ERROR_TEXT_BUFFER];

    int i2c_master_port = I2C_PORT_NUMBER;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_GPIO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_SCL_GPIO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0; //important 

    espRc = i2c_param_config(i2c_master_port, &conf);
    if(espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_0, "Failed at param_config() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        return espRc;
    }
    
    espRc = i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_0, "Failed at driver_install() - Error Code: %d ->  %s\n", espRc, errorTextBuffer);
        return espRc;
    }
    return 0;
}

/**
 * @author Batuhan Sazak
 * @brief I2C Que to write data to a register
 * This function will write the data given as an parameter to the specific register in I2C que.
 */
void writeRegister(uint8_t registerAddress, uint8_t data) {
    
    esp_err_t espRc; 

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   
    espRc = i2c_master_start(cmd);
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_start() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
    }
    
    espRc = i2c_master_write_byte(cmd, (MPLS3115A2_ADDR_7BIT << 1) | I2C_MASTER_WRITE, 1);
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_write_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
    }

    espRc = i2c_master_write_byte(cmd, registerAddress, 1);
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_write_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
    }

    espRc = i2c_master_write_byte(cmd, data, 1);
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_write_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
    }

    espRc = i2c_master_stop(cmd);
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_stop() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
    }

    espRc = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, (1000 / portTICK_RATE_MS));
    if (espRc != ESP_OK){
        esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
        ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_cmd_begin() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
    }
    i2c_cmd_link_delete(cmd);

}

/**
 * @author Batuhan Sazak
 * @brief I2C Que to read data from register
 * This function will return an uint8_t data read from specific target register
 */
uint8_t readRegister(uint8_t registerAddress){   
        
        uint8_t data;
	    esp_err_t espRc; 
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        espRc = i2c_master_start(cmd);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_start() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        espRc = i2c_master_write_byte(cmd, (MPLS3115A2_ADDR_7BIT << 1) | I2C_MASTER_WRITE, 1);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_write_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_master_write_byte(cmd, registerAddress, 1);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_write_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_master_start(cmd);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_start() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_master_write_byte(cmd, (MPLS3115A2_ADDR_7BIT << 1) | I2C_MASTER_READ, 1);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_write_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_master_read_byte(cmd, &data, 1);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_2, "Failed at i2c_master_read_byte() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_master_stop(cmd);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_stop() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, 1000 / portTICK_RATE_MS);
        if (espRc != ESP_OK){
            esp_err_to_name_r(espRc, errorTextBuffer, ERROR_TEXT_BUFFER);
            ESP_LOGE(ESP_TAG_1, "Failed at i2c_master_cmd_begin() - Error Code: %d -> %s\n", espRc, errorTextBuffer);
        }
        i2c_cmd_link_delete(cmd);

    return data;
}

/**
 * @author Batuhan Sazak
 * @brief This function will return the temperature value as a float data.
 * The Most Significant Bitand Least Significant Bit are concatenated 
 * together as an uint16_t and shifted to 12bit int. The value is returned as an float.
 */
float getTemperature(void){

    writeRegister(CTRL_REG_1, CTRL_REG1_OST );

    uint8_t t_low = readRegister(OUT_T_LSB_REG);
    uint8_t t_high = readRegister(OUT_T_MSB_REG);

    uint16_t t = t_high;
    t <<= 8;
    t |= t_low;
    t >>= 4;

    if (t & 0x800) {
        t |= 0xF000;
    }

    float temperature = t;
    temperature /= 16.0;

    return temperature;

}
/**
 * @author Batuhan Sazak
 * @brief This function will return the pressure value as a float data.
 * The Most Significant Bit, Center Significant Bit and Least Significant Bit are concatenated 
 * together as an float
 */
float getPressure(void){

    while (readRegister(CTRL_REG_1) & CTRL_REG1_OST){
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    
    writeRegister(CTRL_REG_1, CTRL_REG1_ALT | CTRL_REG1_OST);

    uint8_t p_msb = readRegister(OUT_P_MSB_REG);
    uint8_t p_csb = readRegister(OUT_P_CSB_REG);
    uint8_t p_lsb = readRegister(OUT_P_LSB_REG);

    uint32_t pressure = p_msb;
    pressure <<= 8;
    pressure |= p_csb;
    pressure <<= 8;
    pressure |= p_lsb;
    pressure >>= 4;

    float barometer = pressure;
    barometer /= 3.93; //
    barometer /= 100;  //für hPa
    return barometer;
}

/***
 * @author Batuhan Sazak
 * @brief Measurement of temperature in °C and pressure in hPa 
 * This function gives two float values (temperature, barometer)
 * and theese values will be automatically broadcasted via MQTT-Client to the MQTT-Broker by incoming measurements.
 */
void vTaskTemperaturePressureMeasurement(void * parameter){

    i2c_master_init();
    QueueHandle_t xQueue1;
    if ((xQueue1 = xQueueCreate (130, 130 * sizeof(char))) == NULL){
        ESP_LOGI(TAG, "Failed to create Altitude Queue\n");
    }else{
        setupMqttTask(xQueue1);
    }

    while (1){
        
        char *message;
        char shadowTopic[] = "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update";
        char jsonPayload[50];
        char comma = ',';

        char test[130];
        float temperature = getTemperature();

        sprintf(jsonPayload, "{\"state\": {\"reported\": {\"temperature\": %.2f }}}", temperature);

        if ((message = malloc(strlen(shadowTopic) + strnlen(&comma,1) + strlen(jsonPayload) +1 )) != NULL){
            message[0] = '\0';
            strcat(message, shadowTopic);
            strncat(message, &comma, 1);
            strcat(message, jsonPayload);
        }else{
            printf("Malloc failed!\n");
        }

        //printf("message: %s\n & length: %d\n", message, strlen(message));

        strcpy(test, message);

        printf("test: %s\n & length: %d\n", test, strlen(test));
        
        if(xQueueSend(xQueue1, test, (TickType_t) portMAX_DELAY) == pdTRUE){
            ESP_LOGI(TAG, "Value was sent sucessfully!\n");
            printf("Temperature: %.2f°C \n ", temperature);
        }else{
            ESP_LOGE(TAG, "Failed to receive data from temperature queue!\n");
        }
        vTaskDelay(10000 / portTICK_RATE_MS);
    } 
}

void temperaturePressureInit(void){
    xTaskCreate(vTaskTemperaturePressureMeasurement, "ALTITUDE-BARO-TEMP-CLICK", 2048, NULL, 10, NULL);
}
