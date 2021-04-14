#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <mdns.h>

// Setup Constants to Connect to Evans Scholars Wifi
#define STA_SSID "zPyConnection"
#define STA_SSID_LEN strlen(STA_SSID)
#define STA_PASSWORD "N@TIONALP@rk5" 
#define STA_PASSWORD_LEN strlen(STA_PASSWORD)

// Setup Constants to Connect to Main Controller
#define AP_SSID "Parking_Rover"
#define AP_SSID_LEN strlen(AP_SSID)
#define AP_PASSWORD "Parking_Rover"
#define AP_PASSWORD_LEN strlen(AP_PASSWORD)
#define MYADDR "127.0.0.1"
#define MYPORT 333
#define DEBUG
//#define DEBUG_I


static void initialize_wifi(void);
void run_on_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);
void wifi_rx_handler(void *buf, wifi_promiscuous_pkt_type_t type);
void schedule_wifi_handlers(void);

static const char TAG[50] = "example:http_jpg";
static const char _ap_ssid_[50] = AP_SSID;
static const char _sta_ssid_[50] = STA_SSID;
static const char _ap_password_[50] = AP_PASSWORD;
static const char _sta_password_[50] = STA_PASSWORD;


#define OPENSSL_EXAMPLE_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 106\r\n\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>OpenSSL example</title></head><body>\r\n" \
                                "OpenSSL server example!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

/*
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

