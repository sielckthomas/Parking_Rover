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
  static QueueHandle_t * imageQueueHandle_ptr;
  static camera_fb_t * pic;
  static char * temp_buf;
  temp_buf = calloc(1, IMAGE_SIZE);

  imageQueueHandle_ptr = (QueueHandle_t *) args;
  for(;;){
    pic = esp_camera_fb_get();
    if(uxQueueSpacesAvailable((QueueHandle_t)*imageQueueHandle_ptr) >= 1){
      pic = esp_camera_fb_get();
      #ifdef DEBUG
        printf("%d",pic->len);
      #endif
      memcpy(temp_buf, pic->buf, pic->len);
      xQueueSend(*imageQueueHandle_ptr,(void *) (pic->buf), 100);
      #ifdef DEBUG
        printf("picture!\n");
      #endif
      esp_camera_fb_return(pic);
    }
  }
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

}