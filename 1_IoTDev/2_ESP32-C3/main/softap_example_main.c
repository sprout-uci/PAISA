/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (-1)
#define ECHO_TEST_CTS (-1)

#define ECHO_UART_PORT_NUM (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE (CONFIG_EXAMPLE_TASK_STACK_SIZE)

#define BUF_SIZE (1024)

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define RETRY_NUMBER 10

#define MSG_END_CHAR "MSGEND"
#define ACK_END_CHAR "ACKEND"
#define BRD_END_CHAR "BRDEND"
#define NSC_END_CHAR "NSCEND"

/* NOTE: You are required to set SSID and PASSWORD of the network where MFR is staying */
#define SSID 		"SAMPLE_SSID"
#define PASSWORD 	"SAMPLE_PASSWORD"
#define MFR_IP_ADDR "SAMPLE_MFR_IP_ADDRESS"

static int s_retry_num = 0;

static const char *TAG = "wifi broadcast";

int64_t get_time_ms()
{
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < RETRY_NUMBER)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void udp_connection_w_mfr(uint8_t *data, int len)
{

    // Define the remote server IP address and port
    const char *mfr_ip = MFR_IP_ADDR;
    const int mfr_port = 10000;

    // Create a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return;
    }

    // Set the socket options
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Set the socket destination address and port
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(mfr_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(mfr_port);

    // Send the data
    int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, (socklen_t)sizeof(dest_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Failed to send UDP packet: errno %d", errno);
    }

    socklen_t fromlen = 0;

    // Receive data from the server
    int cnt = 0;
    for (cnt = 0; cnt < RETRY_NUMBER; cnt++)
    {
        len = recvfrom(sock, data, BUF_SIZE, 0, (struct sockaddr *)&dest_addr, &fromlen);
        if (len > 0)
        {
            ESP_LOGI(TAG, "Received %d bytes", len);
            break;
        }
    }

    if (cnt == RETRY_NUMBER)
    {
        ESP_LOGE(TAG, "Failed to receive UDP packet: errno %d", errno);
        return;
    }

    // Write data back to the UART
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)data, len);

    while (1)
    {
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len > 0 && (strncmp((const char *)data + len - strlen(ACK_END_CHAR), ACK_END_CHAR, strlen(ACK_END_CHAR)) == 0))
        {
            // Send the data
            int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE(TAG, "Failed to send UDP packet: errno %d", errno);
            }
            break;
        }
    }

    // Close the socket
    close(sock);
}

void send_temperature_to_server(uint8_t *data, int len)
{

    // Define the remote server IP address and port
    const char *mfr_ip = MFR_IP_ADDR;
    const int mfr_port = 10001;

    // Create a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return;
    }

    // Set the socket options
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Set the socket destination address and port
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(mfr_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(mfr_port);

    // Send the data
    int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, (socklen_t)sizeof(dest_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Failed to send UDP packet: errno %d", errno);
    }

    // Close the socket
    close(sock);
}

void wifi_start_sta(void)
{
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    ESP_LOGI(TAG, "It passes waiting event");

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 SSID, PASSWORD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 SSID, PASSWORD);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void wifi_start_softap(void)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            //            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .ssid_len = 0,
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
}

#define WIFI_BRD_AVAIL 1
#define WIFI_BRD_NOT_AVAIL 0
int wifi_brd_flag = WIFI_BRD_NOT_AVAIL;
uint8_t beacon_raw[BUF_SIZE] = {
    0x80, 0x00,
    0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x7c, 0xdf, 0xa1, 0xbc, 0x45, 0x74,
    0x7c, 0xdf, 0xa1, 0xbc, 0x45, 0x74,
    0xe0, 0x01,
    0x5c, 0x35, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
    0x64, 0x00,
    0x31, 0x04,
    0x00, 0x05, 0x50, 0x41, 0x49, 0x53, 0x41, // SSID: PAISA
    0x01, 0x08, 0x8b, 0x96, 0x82, 0x84, 0x0c, 0x18, 0x30, 0x60,
    0x03, 0x01, 0x01,
    0x05, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0xdd, 0x0b, 0x00, 0x14, 0x6c, 0x08, // Initial Length: 70 bytes
};
uint8_t beacon_raw_size = 0;

static void announce_task(void *arg)
{
#ifdef EVALUATION
    int i = 0;
#endif

    while (1)
    {
        if (wifi_brd_flag == WIFI_BRD_AVAIL && beacon_raw_size)
        {
#ifdef EVALUATION
            int64_t t = get_time_ms();
#endif
            ESP_ERROR_CHECK(esp_wifi_80211_tx(WIFI_IF_AP, beacon_raw, beacon_raw_size, true));
#ifdef EVALUATION
            if (i % 100 == 0)
            {

                ESP_LOGI(TAG, "time for esp_wifi_80211_tx: %08lld\n", get_time_ms() - t);
                i = 0;
            }
            i++;
#endif
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void uart_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
    ESP_LOGI(TAG, "UART init done");

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    while (1)
    {
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);

        if (len > 6)
        {
            ESP_LOGD(TAG, "Recv str length: %d", len);
            if (strncmp((const char *)data + len - strlen(MSG_END_CHAR), MSG_END_CHAR, strlen(MSG_END_CHAR)) == 0)
            {
                wifi_brd_flag = WIFI_BRD_NOT_AVAIL;
                wifi_start_sta();
                udp_connection_w_mfr(data, len);
                wifi_start_softap();
            }
            else if (strncmp((const char *)data + len - strlen(BRD_END_CHAR), BRD_END_CHAR, strlen(BRD_END_CHAR)) == 0)
            {
                wifi_brd_flag = WIFI_BRD_NOT_AVAIL;
                len -= 6;
#ifdef EVALUATION
                int64_t t = get_time_ms();
#endif

                // Set vendor-specific IE
                beacon_raw[65] = len + 4;
                memcpy(beacon_raw + 70, data, len);
                beacon_raw_size = len + 70;
                wifi_brd_flag = WIFI_BRD_AVAIL;

#ifdef EVALUATION
                ESP_LOGI(TAG, "time for UART-BROADCAST: %08lld\n", get_time_ms() - t);
#endif
            }
            else if (strncmp((const char *)data + len - strlen(NSC_END_CHAR), NSC_END_CHAR, strlen(NSC_END_CHAR)) == 0)
            {
                wifi_brd_flag = WIFI_BRD_NOT_AVAIL;
                int64_t t = get_time_ms();
                wifi_start_sta();
                send_temperature_to_server(data, len);
                wifi_start_softap();

                ESP_LOGI(TAG, "time for NSC: %08lld\n", get_time_ms() - t);
            }
            else // ERROR
            {
                ESP_LOGE(TAG, "not expected UART message\n");
            }
        }
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init();
    wifi_start_softap();

    xTaskCreate(uart_task, "uart_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
    xTaskCreate(announce_task, "announce_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}
