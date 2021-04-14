#include "camera_stream.c"

#define TOTAL_CALL_AMOUNT 200 // Used with Performance Monitor
#define PERFMON_TRACELEVEL -1 // -1 - will ignore trace level
#define DEV_DEBUG_PRESENT


// Main loop //
void app_main(void)
{
  //Initialize NVS
  //static httpd_handle_t server = NULL;
  esp_err_t ret = nvs_flash_init();
  QueueHandle_t imageStreamHandle = 0;
  BaseType_t getImagesHandle = 0;

  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // See __imageBuf in camera_stream.h
  imageStreamHandle = xQueueCreate(IMAGE_BUFF_SIZE, IMAGE_SIZE);

  printf("Hello Zack!\n");
  initialise_wifi();
  //init_sdcard();
  init_camera();
  #ifdef DEBUG
    printf("camera successfully init\n");
  #endif
  
  // Create the task without using any dynamic memory allocation.
  getImagesHandle = xTaskCreate(
                  get_images,       // Function that implements the task.
                  "TAKEPICS",          // Text name for the task.
                  STACK_SIZE,      // Stack size in bytes, not words.
                  &imageStreamHandle,    // Parameter passed into the task.
                  5,// Priority at which the task is created.
                  &getImagesHandle );  // Variable to hold the task's data structure.
  
  #ifdef DEBUG
    printf("camera taking pics\n");
  #endif
  
        
}


#ifdef PERF_MON_EN
void perfMon(void * called_function){
    printf("Start");
    printf("Start test with printing all available statistic");
    xtensa_perfmon_config_t pm_config = {};
    pm_config.counters_size = sizeof(xtensa_perfmon_select_mask_all) / sizeof(uint32_t) / 2;
    pm_config.select_mask = xtensa_perfmon_select_mask_all;
    pm_config.repeat_count = TOTAL_CALL_AMOUNT;
    pm_config.max_deviation = 1;
    pm_config.call_function = called_function;
    pm_config.callback = xtensa_perfmon_view_cb;
    pm_config.callback_params = stdout;
    pm_config.tracelevel = PERFMON_TRACELEVEL;
    xtensa_perfmon_exec(&pm_config);
}
#endif

