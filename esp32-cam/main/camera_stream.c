#include "camera_stream.h"

/*
FUNCTION: init_camera
ARGS:     None
RETURN    None
USAGE:    Used to initialize a ESP32-CAM camera. Choose the right configuration for your camera and initialize it in camera_stream.h
ERRORS:   See wifi functions @https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
*/
static esp_err_t init_camera()
{
  //initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK)
  {
    #ifdef DEBUG
      printf("Camera Init Failed");
    #endif
    return err;
  }

  return ESP_OK;
}


/*
FUNCTION: init_sdcard
ARGS:     None
RETURN    None
USAGE:    Initialize the sd card
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-reference/peripherals/sdmmc_host.html?highlight=sdmmc_slot_config_t#
*/
static void init_sdcard()
{
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 3,
  };
  sdmmc_card_t *card;

  #ifdef DEBUG
      printf("Mounting SD card...\n");
  #endif
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK)
  {
    #ifdef DEBUG
      printf("SD card mount successfully!");
    #endif
  }
  else
  {
    #ifdef DEBUG
      printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
    #endif
  }
}


/*
FUNCTION: get_images
ARGS:     None
RETURN    None
USAGE:    Used to establish the inital frame rate by taking pictures in a controlled manner
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
static void IRAM_ATTR get_images(void * args){
  /*  USE IN PLACE OF FREERTOS QUEUE
  ring_buf_t _image_buf;
  _image_buf->buf = calloc(IMAGE_BUFF_SIZE * IMAGE_SIZE, sizeof(char));
  if(_image_buf->buf == NULL){
    #ifdef DEBUG
      printf("Failed to allocate memory to the image buffer. Retrying in 3s ...");
    #endif
    vTaskDelay(3000);
    xTaskCreate(get_images, "TAKEPICS", 4096, NULL, 5, NULL);
  }
  // Set up the buffer to be empty
  _image_buf->len = IMAGE_BUFF_SIZE * IMAGE_SIZE;
  _image_buf->head = 0; // get_images(here) controls the head
  _image_buf->tail = 0; // socket sending function moves the tail to update
  _image_buf->free_space = IMAGE_BUFF_SIZE * IMAGE_SIZE;
  for(;;){
    
    // Set a delay here to limit the number of frames transmitted
    if(free_space < IMAGE_SIZE){
      vTaskDelay(20); // Give the sending process the time to send a frame
      continue
    }
    camera_fb_t * pic = esp_camera_fb_get();
    
    memcpy(_image_buf->buf + head, pic->buf,pic->len);
    _image_buf->head = (_image_buf->head + pic->len) % _image_buf->len;
    _image_buf->free_space = head > tail? _image_buf->head - _image_buf->tail: _image_buf->len - (tail - head);
    }
  */
  static camera_fb_t * pic;
  static char * temp_buf;
  temp_buf = calloc(1, IMAGE_SIZE);
  int sock = *(int*)args;
  int num_bytes;
  
  
  for(;;){
    //num_messages = uxQueueSpacesAvailable((QueueHandle_t)*imageQueueHandle_ptr);
    
    pic = esp_camera_fb_get();
    #ifdef DEBUG
      printf("pic len: %d\npic width: %d\npic height: %d",strlen((char *)pic->buf), pic->width,pic->height);
    #endif
    num_bytes = send(sock, (const char *)pic->buf, strlen((char *)pic->buf),0);
    #ifdef DEBUG
    printf("sent %d bytes\n",num_bytes);
    #endif
    vTaskDelay(500);
    
    esp_camera_fb_return(pic);
  
  }
}


/*
FUNCTION: open_socket
ARGS:     None
RETURN    None
USAGE:    Used to initialize a esp32 server socket for a client esp32-cam connection
ERRORS:   See socket functions 
*/
int open_socket(void)
{
  esp_ip4_addr_t * server_addr = NULL;
  struct sockaddr_in main_controller_addr;
  int sock = 0;
  esp_err_t err;

  #ifdef DEBUG
    printf("Trying to open socket\n\n");
  #endif
  //mdns_query_a("TATOW_CONTROLLER", 2000, server_addr);
  //printf("\nIP: %s\n\n", ip4addr_ntoa((ip4_addr_t*)server_addr));
  
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0){
    #ifdef DEBUG
      printf("\nFailserver_addred to create socket\n");
    #endif
    return;
  }

  main_controller_addr.sin_family = AF_INET;
  main_controller_addr.sin_addr.s_addr = inet_addr("192.168.10.1");
  main_controller_addr.sin_port = htons(MYPORT);

  err = connect(sock,(struct sockaddr *)&main_controller_addr, sizeof(main_controller_addr));
  while(err != 0){
    #ifdef DEBUG
      printf("\nFailed to connect socket\nRetrying...\n\n");
    #endif
    vTaskDelay(3000);
    err = connect(sock,(struct sockaddr *)&main_controller_addr, sizeof(main_controller_addr));
  }

  return sock;
}


/*
FUNCTION: initialize_wifi
ARGS:     None
RETURN    None
USAGE:    Used to initialize the esp32 as a station
          Set the station provisioning details at the camera_stream.h for function(wifi names & passwords) !(move to file with time left)
ERRORS:   See wifi functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#wi-fi
*/
void initialise_wifi(void)
{

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // initialize the tcp stack
  tcpip_adapter_init();

  wifi_config_t * sta_config = calloc(1, sizeof(wifi_config_t));
  
  // Set up the wifi
  const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init((&cfg)));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  // Configure the (STA) station
  strcpy((char *)sta_config->sta.ssid, STA_SSID);
  strcpy((char *)sta_config->sta.password, STA_PASSWORD);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, sta_config));
  
  ESP_ERROR_CHECK(esp_wifi_start());
  
  ESP_ERROR_CHECK(esp_wifi_connect());
  
  printf("Connecting\n");
  
  // stop DHCP server
  ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));

  // assign a static IP to the network interface
  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  IP4_ADDR(&info.ip, 192, 168, 10, 2);
  IP4_ADDR(&info.gw, 192, 168, 10, 2);
  IP4_ADDR(&info.netmask, 255, 255, 255, 0);
  
  ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &info));

  // start the DHCP server
  ESP_ERROR_CHECK(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
  
  //initialize mDNS service
  esp_err_t err = mdns_init();
  if (err) {
      printf("MDNS Init failed: %d\n", err);
      return;
  }
  mdns_hostname_set("TATOW_CAM1");
  vTaskDelay(500); // Wait for DHCP and DNS to settle down
}
//////////////////////////////////////////////////////////////////////////


esp_err_t camera_start(void){
    if(CAM_PIN_PWDN != -1){
        gpio_init();
        gpio_set_direction(CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
        gpio_set_level(CAM_PIN_PWDN, 0);
    }
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        #ifdef DEBUG
        printf("Camera Init Failed");
        #endif
        return err;
    }

    return ESP_OK;
}


esp_err_t camera_capture(){
    //acquire a frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        #ifdef DEBUG
        printf("Camera Capture Failed");
        #endif
        return ESP_FAIL;
    }
    //replace this with your own function
    //process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);
  
    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    return ESP_OK;
}

esp_err_t jpg_stream_httpd_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            #ifdef DEBUG
                printf("Camera Capture Failed");
            #endif
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                #ifdef DEBUG
                    printf("JPEG compression failed");
                #endif
                
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        #ifdef DEBUG
            printf("MJPG: %uKB %ums (%.1ffps)",
                (uint32_t)(_jpg_buf_len/1024),
                (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
        #endif
    }

    last_frame = 0;
    return res;
}