/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_check.h"
#include <time.h>
#include <sys/time.h>

#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#include "esp_camera.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "pages.h"
#endif  // !CONFIG_IDF_TARGET_LINUX

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */
static httpd_handle_t server = NULL;

static const char *TAG = "main";
static int retry_count = 0;

esp_err_t save_wifi_credentials(
    const char *ssid,
    const char *password);


static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, wifi_page, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[256];

    int len = httpd_req_recv(
        req,
        buf,
        sizeof(buf) - 1
    );

    if (len <= 0) {
        return ESP_FAIL;
    }

    buf[len] = 0;

    ESP_LOGI(TAG, "POST: %s", buf);

    char ssid[33];
    char password[65];

    sscanf(buf,
    "ssid=%32[^&]&password=%64s",
    ssid,
    password);

    save_wifi_credentials(
    ssid,
    password
    );



    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "PASS: %s", password);

    httpd_resp_set_type(req, "text/html");

    httpd_resp_sendstr(
        req,
        "<html>"
        "<body style='font-family:Arial;text-align:center;padding-top:50px'>"
        "<h2>Konfiguracja zapisana</h2>"
        "<p>Urządzenie uruchamia się ponownie...</p>"
        "</body>"
        "</html>"
    );

    ESP_LOGI(TAG, "Saving WiFi");

    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();

    return ESP_OK;
}

static esp_err_t control_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req,
                    control_page,
                    HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t led_on_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "LED OFF");
    gpio_set_level(GPIO_NUM_2, 1);

    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

static esp_err_t led_off_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "LED ON");
    gpio_set_level(GPIO_NUM_2, 0);

    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");

    httpd_resp_send(
        req,
        (const char *)fb->buf,
        fb->len
    );

    esp_camera_fb_return(fb);

    return ESP_OK;
}

static esp_err_t camera_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req,
                    camera_page,
                    HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t info_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    httpd_resp_sendstr(req,
        "{"
        "\"device\":\"ESP32_CAMERA\","
        "\"name\":\"Salon\","
        "\"version\":\"1.0\""
        "}");

    return ESP_OK;
}
esp_err_t save_wifi_credentials(
    const char *ssid,
    const char *password)
{
    nvs_handle_t nvs;

    ESP_ERROR_CHECK(
        nvs_open(
            "wifi",
            NVS_READWRITE,
            &nvs
        )
    );

    ESP_ERROR_CHECK(
        nvs_set_str(
            nvs,
            "ssid",
            ssid
        )
    );

    ESP_ERROR_CHECK(
        nvs_set_str(
            nvs,
            "pass",
            password
        )
    );

    ESP_ERROR_CHECK(
        nvs_commit(nvs)
    );

    nvs_close(nvs);

    return ESP_OK;
}

bool load_wifi_credentials(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t pass_size)
{
    nvs_handle_t nvs;

    if (nvs_open(
            "wifi",
            NVS_READONLY,
            &nvs) != ESP_OK)
    {
        return false;
    }

    esp_err_t err1 =
        nvs_get_str(
            nvs,
            "ssid",
            ssid,
            &ssid_size
        );

    esp_err_t err2 =
        nvs_get_str(
            nvs,
            "pass",
            password,
            &pass_size
        );

    nvs_close(nvs);

    return err1 == ESP_OK &&
           err2 == ESP_OK;
}

static void register_handlers(httpd_handle_t server)
{
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler
    };

    httpd_uri_t control = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = control_handler
    };

    httpd_uri_t led_on = {
        .uri = "/led/on",
        .method = HTTP_GET,
        .handler = led_on_handler
    };

    httpd_uri_t led_off = {
        .uri = "/led/off",
        .method = HTTP_GET,
        .handler = led_off_handler
    };

    httpd_uri_t capture = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = capture_handler
    };

    httpd_uri_t camera = {
    .uri = "/camera",
    .method = HTTP_GET,
    .handler = camera_page_handler
    };

    httpd_uri_t info = {
    .uri = "/info",
    .method = HTTP_GET,
    .handler = info_handler
};

    httpd_register_uri_handler(server, &info);
    httpd_register_uri_handler(server, &capture);
    httpd_register_uri_handler(server, &control);
    httpd_register_uri_handler(server, &led_on);
    httpd_register_uri_handler(server, &led_off);
    httpd_register_uri_handler(server, &camera);
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
}

static httpd_handle_t start_webserver(void)
{
    ESP_LOGI(TAG, "Starting webserver");

    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server,
                    &config) == ESP_OK)
    {
        register_handlers(server);

        ESP_LOGI(TAG, "Webserver started");
    }

    return server;
}
static void start_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_SETUP",
            .ssid_len = 11,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_AP
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_AP,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    ESP_LOGI(TAG,
             "AP started");
}

static void connect_wifi(
    const char *ssid,
    const char *password)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );

    wifi_config_t wifi_config = {0};

    strncpy(
        (char*)wifi_config.sta.ssid,
        ssid,
        sizeof(wifi_config.sta.ssid)
    );

    strncpy(
        (char*)wifi_config.sta.password,
        password,
        sizeof(wifi_config.sta.password)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    ESP_ERROR_CHECK(
        esp_wifi_connect()
    );
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected to AP");
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG,
                 "IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        if (server == NULL)
        {
            server = start_webserver();
        }
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    { 

        if(retry_count < 5)
        {
            retry_count++;

            ESP_LOGW(TAG,
                    "Reconnect attempt %d",
                    retry_count);

            vTaskDelay(pdMS_TO_TICKS(1000));
    
        }
        else
        {
            ESP_LOGI(TAG, "restarting due to lost connection");

            esp_restart();

        }
    }

 
}



static void init_camera(void)
{
    camera_config_t config = {
        .pin_pwdn  = -1,
        .pin_reset = -1,
        .pin_xclk  = 10,

        .pin_sccb_sda = 40,
        .pin_sccb_scl = 39,

        .pin_d7 = 48,
        .pin_d6 = 11,
        .pin_d5 = 12,
        .pin_d4 = 14,
        .pin_d3 = 16,
        .pin_d2 = 18,
        .pin_d1 = 17,
        .pin_d0 = 15,

        .pin_vsync = 38,
        .pin_href  = 47,
        .pin_pclk  = 13,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,

        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 30,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
        
    };

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "Camera init failed: 0x%x",
                 err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();

    ESP_LOGI(TAG,
             "Camera PID: 0x%04x",
             s->id.PID);

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb)
    {
        ESP_LOGE(TAG, "Failed to capture image");
        return;
    }

    ESP_LOGI(TAG,
             "JPEG size: %u bytes",
             fb->len);

    esp_camera_fb_return(fb);
}
void app_main(void)
{   
    ESP_LOGI(TAG,
         "Free heap: %u",
         (unsigned)esp_get_free_heap_size());

    ESP_LOGI(TAG,
         "Free SPIRAM: %u",
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    init_camera();

    char ssid[33];
    char password[65];

    if(load_wifi_credentials(
        ssid,
        sizeof(ssid),
        password,
        sizeof(password)))
    {
        ESP_LOGI(TAG, "Found saved WiFi");
        ESP_LOGI(TAG, "SSID: %s", ssid);
        ESP_ERROR_CHECK(
            esp_event_handler_register(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &wifi_event_handler,
                NULL
            )
        );

        ESP_ERROR_CHECK(
            esp_event_handler_register(
                IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &wifi_event_handler,
                NULL
            )
        );  
        
        ESP_ERROR_CHECK(
            esp_event_handler_register(
                WIFI_EVENT,
                WIFI_EVENT_STA_DISCONNECTED,
                &wifi_event_handler,
                NULL
            )
        ); 
         
        ESP_LOGI(TAG, "SSID: %s", ssid);
        ESP_LOGI(TAG, "PASS: %s", password);     
        connect_wifi(ssid, password);
    }
    else
    {
        start_softap();
        start_webserver();
    }
    
}