#include "mqtt_client.h"
//#include "mqtt_config.h"
#include "mqtt_supported_features.h"
#include "include/config.h"
#include "include/mqtt_task.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

//#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)
extern const uint8_t aws_root_ca_pem_start[]     asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[]       asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[]   asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[]     asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[]       asm("_binary_private_pem_key_end");
//#endif

static const char *TAG = "MQTT_ALTITUDE_NODE";
esp_mqtt_client_handle_t client = NULL;

// Forward function declarations
esp_err_t vTaskMqttClient(void *parameter);
esp_err_t getTopic(char *message, char *result);
esp_err_t getValue(char *message, char *result);
esp_err_t clearMessage(char *message);


const esp_mqtt_client_config_t mqtt_cfg = {
    .host = MQTT_HOST,
    .port = MQTT_PORT,
    .cert_pem = (const char *) aws_root_ca_pem_start,
    .client_cert_pem = (const char *) certificate_pem_crt_start,
    .client_key_pem = (const char *) private_pem_key_start,
    .transport = MQTT_TRANSPORT_OVER_SSL,
    .client_id = CLIENT_ID
};

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update", "Altitude Click is sending values to the que ...", 0, 0, 0);
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
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
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


void mqtt_client_start() {
    client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
    //esp_mqtt_client_publish(client, "esp32/temperature", "starting", 8, NULL, NULL);
}

void setupMqttTask(QueueHandle_t queueHandle) {
    ESP_LOGI(TAG, "CREATING MQTT TASK");
    xTaskCreate(&vTaskMqttClient, "mqtt_task", MQTT_TASKSTACKSIZE, queueHandle, 10, NULL);
}

esp_err_t vTaskMqttClient(void *parameter) {
    mqtt_client_start();
    unsigned long counter;
    char message[200]; //before it was 50
    char value[50];
    char topic[80]; //before it was 16
    
    QueueHandle_t queueHandle = (QueueHandle_t) parameter;

    vTaskDelay(5000 / portTICK_RATE_MS);

    while (1) {
        xQueueReceive(queueHandle, &message, (TickType_t) portMAX_DELAY);
        ESP_LOGI(TAG, "Received value = [%s] on queue", message);

        printf("%s", message);
        
        if (strlen(message) > 0) {
            getTopic(&message, &topic);
            getValue(&message, &value);

            ESP_LOGI(TAG, "VALUE = [%s]", value);
            ESP_LOGI(TAG, "TOPIC = [%s]", topic);

            clearMessage(&message);
            ESP_ERROR_CHECK(esp_mqtt_client_publish(client, topic, value, strlen(value), NULL, NULL));
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

esp_err_t getTopic(char *message, char *result) {
    if (message == NULL || result == NULL) {
        ESP_LOGE(TAG, "The parameters message and result must not be null!");
        return EXIT_FAILURE;
    }

    for (int i = 0; i <= strlen(message); i++) {
        if (message[i] == ',') {
            result[i] = '\0';
            break;
        }

        result[i] = message[i];
    }

    return EXIT_SUCCESS;
}

esp_err_t getValue(char *message, char *result) {
    if (message == NULL || result == NULL) {
        ESP_LOGE(TAG, "The parameters message and result must not be null!");
        return EXIT_FAILURE;
    }

    uint8_t index = 0;

    while (message[index] != ',') {
        index++;
    }

    index += 1;

    for (int i = 0; i <= strlen(message); i++) {
        if (message[index] == '\0') {
            result[i] = '\0';
            break;
        }

        result[i] = message[index++];
    }

    return EXIT_SUCCESS;
}

esp_err_t clearMessage(char *message) {
    if (message == NULL) {
        ESP_LOGE(TAG, "The parameter message must not be null!");
        return EXIT_FAILURE;
    }

    for (int i = 0; i <= strlen(message); i++) {
        message[i] = '\0';
    }

    return EXIT_SUCCESS;
}
