#include "config-manager.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#define WIFI_SSID "Ranger-h"
#define WIFI_PASSWORD "p@ssw0rd"
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

    // NVS_Write_String(NVS_NAMESPACE, "wifi_ssid", WIFI_SSID);
    // NVS_Write_String(NVS_NAMESPACE, "wifi_password", WIFI_PASSWORD);
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
    size_t required_size;
    char* data = "";

    // ESP_LOGW("NVS", "Show Value-> name: %s, key: %s, len:>%d", name, key, len);

    retVal = nvs_open(name, NVS_READWRITE, &nvsHandle);
    if (retVal != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS handle for Write", esp_err_to_name(retVal));
    } else {
        printf("opening NVS Read handle Done \r\n");
        nvs_get_str(nvsHandle, key, NULL, &required_size);
        data = malloc(required_size);
        retVal = nvs_get_str(nvsHandle, key, data, &required_size);
        if (retVal == ESP_OK) {
            ESP_LOGW("NVS", "*****(%s) Can read/get value: %s", esp_err_to_name(retVal), data);
        } else
            ESP_LOGE("NVS", "Error (%s) Can not read/get value: %s", esp_err_to_name(retVal), data);

        retVal = nvs_commit(nvsHandle);
        if (retVal != ESP_OK) {
            ESP_LOGE("NVS", "Error (%s) Can not commit - read", esp_err_to_name(retVal));
        } else {
            ESP_LOGI("NVS", "Read Commit Done!");
        }
    }

    nvs_close(nvsHandle);

    return *data;
}