#include "mqtt_client.h"
//#include "mqtt_config.h"
#include "mqtt_supported_features.h"
#include "include/config.h"
#include "include/mqtt_receiver_task.h"
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
esp_err_t vTaskMqttReceiverClient(void *parameter);

QueueHandle_t queueHandleTag = NULL;
static const char *TAG = "MQTT_SPRINKLE_CONTROL";
esp_mqtt_client_handle_t receiver_client_tag = NULL;

const esp_mqtt_client_config_t mqtt_receiver_cfg_tag = {
    .host = MQTT_HOST,
    .port = MQTT_PORT,
    .cert_pem = (const char *) aws_root_ca_pem_start,
    .client_cert_pem = (const char *) certificate_pem_crt_start,
    .client_key_pem = (const char *) private_pem_key_start,
    .transport = MQTT_TRANSPORT_OVER_SSL,
    //.username = MQTT_USER,
    //.password = MQTT_PASSW
};

/**
 * @brief This function is an event handler for the mqtt client.
 * It will react to any mqtt event, but in the case of this case, it's main 
 * usage is to react when data is received on the topic esp8266/taguid. When some data is 
 * received, it will write it to the queues.
 * @param event the event on which the function will be called.
 */
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t receiver_client_tag = event->client;

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
            msg_id = esp_mqtt_client_publish(receiver_client_tag, "esp8266/taguid", " Stepper-Click subscribes to the topic esp8266/taguid ...", 0, 0, 0);
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
            if (queueHandleTag != NULL) {
                xQueueSend(queueHandleTag, (void *) event->data, (TickType_t) 0);
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
 * and starts the client, after that it publishes a message to the esp8266/taguid topic that it 
 * has successfully started.
 */
void mqtt_tag_receiver_client_start() {
    receiver_client_tag = esp_mqtt_client_init(&mqtt_receiver_cfg_tag);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(receiver_client_tag, ESP_EVENT_ANY_ID, mqtt_event_handler, receiver_client_tag));
    ESP_ERROR_CHECK(esp_mqtt_client_start(receiver_client_tag));
    //esp_mqtt_client_publish(receiver_client, "esp8266/taguid", "starting", 8, NULL, NULL);
}

/**
 * @brief This function is used as the main API of the mqtt receiver task. You can call this function from 
 * any main.c file or such. It only needs a queueHandle provided, in order to be able to write to it.
 * The queue is the main way of communicating between the caller of the API.
 * @param queueHandleTag The queue to which values are to be written to.
 */
void setupMqttReceiverTask(QueueHandle_t queueHandleTag) {
    ESP_LOGI(TAG, "CREATING MQTT TASK");
    xTaskCreate(&vTaskMqttReceiverClient, "mqtt_task", MQTT_TASKSTACKSIZE, queueHandleTag, 10, NULL);
}

/**
 * @brief this is the main function of the mqtt receiver task. It will start the mqtt client and
 * subscribe to the topic esp8266/taguid.
 */
esp_err_t vTaskMqttReceiverClient(void *parameter) {
    mqtt_tag_receiver_client_start();

    vTaskDelay(5000 / portTICK_RATE_MS);

    int subscription = esp_mqtt_client_subscribe(receiver_client_tag, "esp8266/taguid", 0);
    ESP_LOGI(TAG, "SUBSCRIPTION TO TOPIC: %d", subscription);

    queueHandleTag = (QueueHandle_t) parameter;

    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
