#include "config-manager.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#define WIFI_SSID "ESP32-Access-Point"
#define WIFI_PASSWORD "esp32"
#include "esp_log.h"

static const char* TAG = "CONFIG MANAGER";


void init_nvs()
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}
void NVS_Write_String(const char* name, const char* key, const char* stringVal)
{
    nvs_handle_t nvsHandle;
    esp_err_t retVal;

    retVal = nvs_open(name, NVS_READWRITE, &nvsHandle);
    if (retVal != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS handle for Write", esp_err_to_name(retVal));
    } else {
        printf("opening NVS Write handle Done \r\n");
        retVal = nvs_set_str(nvsHandle, key, stringVal);
        if (retVal != ESP_OK) {
            ESP_LOGE("NVS", "Error (%s) Can not write/set value: %s", esp_err_to_name(retVal), stringVal);
        }

        retVal = nvs_commit(nvsHandle);
        if (retVal != ESP_OK) {
            ESP_LOGE("NVS", "Error (%s) Can not commit - write", esp_err_to_name(retVal));
        } else {
            ESP_LOGI("NVS", "Write Commit Done!");
        }
    }

    nvs_close(nvsHandle);
}
char* NVS_Read_String(const char* name, const char* key)
{
        nvs_handle_t nvsHandle;
    esp_err_t retVal;
    size_t required_size = 0;
    char* data = NULL;

    retVal = nvs_open(name, NVS_READWRITE, &nvsHandle);
    if (retVal != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS handle for Write", esp_err_to_name(retVal));
    } else {
        printf("opening NVS Read handle Done \r\n");

        // First call to get the required size
        retVal = nvs_get_str(nvsHandle, key, NULL, &required_size);
        if (retVal != ESP_OK) {
            ESP_LOGE("NVS", "Error (%s) getting size of value: %s", esp_err_to_name(retVal), key);
        } else {
            // Allocate memory for the data
            data = malloc(required_size);
            if (data == NULL) {
                ESP_LOGE("NVS", "Error allocating memory for value: %s", key);
            } else {
                // Second call to actually get the data
                retVal = nvs_get_str(nvsHandle, key, data, &required_size);
                if (retVal == ESP_OK) {
                    ESP_LOGW("NVS", "*****(%s) Can read/get value: %s", esp_err_to_name(retVal), data);
                } else {
                    ESP_LOGE("NVS", "Error (%s) Can not read/get value: %s", esp_err_to_name(retVal), data);
                }
            }
        }

        retVal = nvs_commit(nvsHandle);
        if (retVal != ESP_OK) {
            ESP_LOGE("NVS", "Error (%s) Can not commit - read", esp_err_to_name(retVal));
        }
    }

    return data;
}