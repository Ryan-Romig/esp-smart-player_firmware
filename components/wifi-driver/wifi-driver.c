#include "wifi-driver.h"
#include "../config-manager/include/config-manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/apps/netbiosns.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include <string.h>

#include "../../main/Global.h"

#include "mdns.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRY 10
#define WIFI_MAX_CONNECTION 20
bool isApMode = false;
int firstRun = 0;

#define SSID "ESP32-Access-Point"
#define PSK "p@ssw0rd"
static const char* TAG = "WIFI DRIVER";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */

static int s_retry_num = 0;

#define MDNS_HOST_NAME "PLAYER"

#define MDNS_INSTANCE "ESP_MDNS"

esp_err_t start_rest_server(const char* base_path);

static void init_mdns(void)
{
    mdns_init();
    mdns_hostname_set(MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = { { "board", "esp32" }, { "path", "/" } };

    ESP_ERROR_CHECK(mdns_service_add(
        "ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));

    netbiosns_init();
    netbiosns_set_name(MDNS_HOST_NAME);
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGI(TAG, "Can't connect");
            init_ap_mode();
            s_retry_num = 0;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        isWifiConnected = 1;

    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        isWifiConnected = 0;
    }
}

void init_sta_mode()
{
    ESP_LOGI(TAG, "starting station mode");

    if (firstRun > 0) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }
    char* ssid_ptr = NVS_Read_String(NVS_NAMESPACE, "wifi_ssid");
    char* password_ptr = NVS_Read_String(NVS_NAMESPACE, "wifi_password");
    if (ssid_ptr == NULL) {
        // Handle the error
        printf("Failed to read the SSID from NVS\n");
        init_ap_mode();
        return;
    }

    wifi_config_t wifi_config = {
    .sta = {
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
    };

    // Copy the strings into the wifi_config structure
    strncpy((char*)wifi_config.sta.ssid, ssid_ptr, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password_ptr, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    firstRun = 1;
}

void init_wifi()
{
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    init_mdns();
}
void print_ip()
{

    while (1) {
        printf("Entering print_ip()...\n");

        esp_netif_ip_info_t ip_info;

        esp_netif_t* netif = NULL;
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

        if (netif == NULL)
            printf("Pointer is NULL.\n");
        else {
            // If we got a pointer, show the address.
            printf("Pointer is: 0x%" PRIXPTR "\n", (uintptr_t)netif);

            // Copy the netif IP info into our variable.
            esp_netif_get_ip_info(netif, &ip_info);

            printf("My IP: " IPSTR "\n", IP2STR(&ip_info.ip));
            printf("My GW: " IPSTR "\n", IP2STR(&ip_info.gw));
            printf("My NETMASK: " IPSTR "\n", IP2STR(&ip_info.netmask));
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        break;
    }
}
void init_ap_mode()
{
    ESP_LOGI(TAG, "starting AP mode");
    if (firstRun > 0 && isApMode == false) {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }
    firstRun = 1;

    wifi_config_t wifi_config = {
        .ap = { .ssid = SSID,
            .ssid_len = strlen(SSID),
            .password = PSK,
            .max_connection = WIFI_MAX_CONNECTION,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK },
    };
    if (strlen(PSK) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    isApMode = true;
    ESP_LOGI(TAG, " AP mode started");
}