#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include <esp_http_server.h>
#include "esp_camera.h"

// Setup Constants to Connect to Evans Scholars Wifi
#define STA_SSID "evans_scholars"
#define STA_SSID_LEN strlen(STA_SSID)
#define STA_PASSWORD "chickevans67" 
#define STA_PASSWORD_LEN len(STA_PASSWORD)

// Setup Constants to Connect to Main Controller
#define AP_SSID "Parking_Rover"
#define AP_SSID_LEN strlen(AP_SSID)
#define AP_PASSWORD "Parking_Rover" 
#define AP_PASSWORD_LEN len(AP_PASSWORD)

/*
static const char *TAG = "example:http_jpg";

typedef struct
{
  httpd_req_t *req;
  size_t len;
} jpg_chunking_t;


httpd_uri_t uri_handler_jpg = {
    .uri = "/jpg",
    .method = HTTP_GET,
    .handler = jpg_httpd_handler};

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
  jpg_chunking_t *j = (jpg_chunking_t *)arg;
  if (!index)
  {
    j->len = 0;
  }
  if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
  {
    return 0;
  }
  j->len += len;
  return len;
}

static esp_err_t jpg_httpd_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t fb_len = 0;
  int64_t fr_start = esp_timer_get_time();

  fb = esp_camera_fb_get();
  if (!fb)
  {
    ESP_LOGE(TAG, "Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  res = httpd_resp_set_type(req, "image/jpeg");
  if (res == ESP_OK)
  {
    res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  }

  if (res == ESP_OK)
  {
    fb_len = fb->len;
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  }
  esp_camera_fb_return(fb);
  int64_t fr_end = esp_timer_get_time();
  ESP_LOGI(TAG, "JPG: %uKB %ums", (uint32_t)(fb_len / 1024), (uint32_t)((fr_end - fr_start) / 1000));
  return res;
}
*/

httpd_handle_t start_webserver(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK)
  {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &uri_handler_jpg);
    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}

void stop_webserver(httpd_handle_t server)
{
  // Stop the httpd server
  httpd_stop(server);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  httpd_handle_t *server = (httpd_handle_t *)ctx;

  switch (event->event_id)
  {
  case SYSTEM_EVENT_STA_START:
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
    ESP_ERROR_CHECK(esp_wifi_connect());
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
    ESP_LOGI(TAG, "Got IP: '%s'",
             ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

    /* Start the web server */
    if (*server == NULL)
    {
      *server = start_webserver();
    }
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
    ESP_ERROR_CHECK(esp_wifi_connect());

    /* Stop the web server */
    if (*server)
    {
      stop_webserver(*server);
      *server = NULL;
    }
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void initialise_wifi(void *arg)
{
  wifi_config_t * ap_config = calloc(1, sizeof(wifi_config_t));
  wifi_config_t * sta_config = calloc(1, sizeof(wifi_config_t));

  strncpy((char *)ap_config->ap.ssid,AP_SSID,AP_SSID_LEN);
  strncpy((char *)ap_config->ap.password, AP_PASSWORD, AP_PASSWORD_LEN));
  ap_config->ap.authmode = WIFI_AUTH_WPA_PSK; // Move up to WPA2 for AES(slower)
  ap_config->ap.ssid_len = 0;
  ap_config->ap.max_connection = 4;
  ap_config->ap.channel = 0; /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/

  strncpy((char *)sta_config.sta.ssid, STA_SSID, STA_SSID_LEN);
  strncpy((char *)sta_config.sta.password, STA_PASSWORD, STA_PASSWORD_LEN));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));

  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, arg));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  
  ESP_ERROR_CHECK(esp_wifi_start());
}

void start_stream()
{
  static httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(nvs_flash_init());
  initialise_wifi(&server);
}