#include "mqtt_client.h"
//#include "mqtt_config.h"
#include "mqtt_supported_features.h"
#include "include/config.h"
#include "include/mqtt_receiver_task.h"
#include "esp_transport_ssl.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

extern const uint8_t aws_root_ca_pem_start[]     asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[]       asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[]   asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[]     asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[]       asm("_binary_private_pem_key_end");

// Forward function declarations
esp_err_t vTaskMqttReceiverClientTemperature(void *parameter);

QueueHandle_t queueHandleTemperature = NULL;
static const char *TAG = "MQTT_ALARM_BUZZER";
esp_mqtt_client_handle_t receiver_client_temp = NULL;

const esp_mqtt_client_config_t mqtt_receiver_cfg_temp = {
    .host = MQTT_HOST,
    .port = MQTT_PORT,
    .cert_pem = (const char *) aws_root_ca_pem_start,
    .client_cert_pem = (const char *) certificate_pem_crt_start,
    .client_key_pem = (const char *) private_pem_key_start,
    .transport = MQTT_TRANSPORT_OVER_SSL,
    .client_id = CLIENT_ID // -> important for shadow
    //.username = MQTT_USER,
    //.password = MQTT_PASSW
};

/**
 * @brief This function is an event handler for the mqtt client.
 * It will react to any mqtt event, but in the case of this case, it's main 
 * usage is to react when data is received on the topics esp32/temperature. When some data is 
 * received, it will write it to the queues.
 * @param event the event on which the function will be called.
 */
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t receiver_client_temp = event->client;

    int msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id); 
            msg_id = esp_mqtt_client_publish(receiver_client_temp, "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update/accepted", " Buzz-Click subscribes to the esp32/temperature ...", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC: %.*s\r\n", event->topic_len, event->topic);
            printf("TOPIC-LENGTH: %d characters\r\n", event->topic_len);
            printf("DATA: %.*s\r\n", event->data_len, event->data);
            printf("DATA-LENGTH: %.d characters\r\n", event->data_len);
            
            if (queueHandleTemperature != NULL) {
                xQueueSend(queueHandleTemperature, (void *) event->data, (TickType_t) 0);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "MQTT_EVENT_ANY");
            break;
    }
    return ESP_OK;
}

/**
 * @brief This function registers the mqtt event handler.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

/**
 * @brief this function is used to start the mqtt client. It registers the event handler 
 * and starts the client, after that it publishes a message to the /alarm topic that it 
 * has successfully started.
 */
void mqtt_temperature_receiver_client_start() {
    receiver_client_temp = esp_mqtt_client_init(&mqtt_receiver_cfg_temp);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(receiver_client_temp, ESP_EVENT_ANY_ID, mqtt_event_handler, receiver_client_temp));
    ESP_ERROR_CHECK(esp_mqtt_client_start(receiver_client_temp));
    //esp_mqtt_client_publish(receiver_client, "esp8266/taguid", "starting", 8, NULL, NULL);
}

/**
 * @brief This function is used as the main API of the mqtt receiver task. You can call this function from 
 * any main.c file or such. It only needs a queueHandle provided, in order to be able to write to it.
 * The queue is the main way of communicating between the caller of the API.
 * @param queueHandleTemperature The queue to which values are to be written to.
 */
void setupMqttReceiverTaskTemperature(QueueHandle_t queueHandleTemperature) {
    ESP_LOGI(TAG, "CREATING MQTT TASK");
    xTaskCreate(&vTaskMqttReceiverClientTemperature, "mqtt_task", MQTT_TASKSTACKSIZE, queueHandleTemperature, 10, NULL);
}

/**
 * @brief this is the main function of the mqtt receiver task. It will start the mqtt client and
 * subscribe to the topic esp32/temperature.
 */
esp_err_t vTaskMqttReceiverClientTemperature(void *parameter) {
    mqtt_temperature_receiver_client_start();

    vTaskDelay(5000 / portTICK_RATE_MS);
    int subscription = esp_mqtt_client_subscribe(receiver_client_temp, "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update/accepted", 0);
    ESP_LOGI(TAG, "SUBSCRIPTION TO TOPIC: %d", subscription);

    queueHandleTemperature = (QueueHandle_t) parameter;

    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
