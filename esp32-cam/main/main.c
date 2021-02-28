/* WiFi station Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"

#define LED1 2
#define LED2 4

// Function Declarations //
///////////////////////////////////////////////////////////////////////
void learning_led(void);
void led_blink1(void * params);
void led_blink2(void * params);
void led_blink(void * params);
///////////////////////////////////////////////////////////////////////
void learning_semaphore(void);
void listenforHTTP(void * params);
void http_semaphore_task(void * params);
///////////////////////////////////////////////////////////////////////
void learning_task_handlers(void);
static void notification_sender(void * params);
static void notification_receiver(void * params);
///////////////////////////////////////////////////////////////////////
void learning_queue(void);
void listenForHTTPQ(void * params);
void httpQDissect(void * params);
///////////////////////////////////////////////////////////////////////

// Type Declarations //
typedef struct{
        int led;
        int time;
        int increment;
}led_params_t;

// Global Variables // 
SemaphoreHandle_t bin_semaphore;
static TaskHandle_t notification_receiverHandler = NULL;
xQueueHandle queue;

// Main loop //
void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf("Hello Zack!\n");
    //learning_led();
    //learning_semaphore();
    //learning_task_handlers();
    learning_queue();
}


///////////////////////////////////////////////////////////////////////
void learning_queue(void){
    queue = xQueueCreate(3,sizeof(int)); // q len, size of each element
    xTaskCreate(&listenForHTTPQ,"get http off Q", 2048*3, NULL, 2, &notification_receiverHandler);
    xTaskCreate(&httpQDissect,"handle http data", 2048*3, NULL, 1, NULL);
}
    
void listenForHTTPQ(void * params){
    int count = 0;
    for(;;){
        count++;
        printf("recieved http message\n");
        long ok = xQueueSend(queue, &count, 1000/ portTICK_PERIOD_MS);
        if(ok){
            printf("added message to queue\n");
        }else{
            printf("Failed to add message to queue\n");
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
    
}

void httpQDissect(void * params){
    for(;;){
        int rxInt;
        if (xQueueReceive(queue, &rxInt, 5000/portTICK_PERIOD_MS)){
                printf("doing something with http %d\n",rxInt);
        }
    }
}

///////////////////////////////////////////////////////////////////////
void learning_task_handlers(void){
    xTaskCreate(notification_receiver,"task recieve", 2048*3, NULL, 1, &notification_receiverHandler);
    xTaskCreate(notification_sender,"task send", 2048*3, NULL, 1, NULL);
}

static void notification_sender(void * params){
    for(;;){
        xTaskNotify(notification_receiverHandler, (1<<0), eSetBits);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        xTaskNotify(notification_receiverHandler, (1<<1),eSetBits);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        xTaskNotify(notification_receiverHandler, (1<<2),eSetBits);
        vTaskDelay(1000/portTICK_PERIOD_MS);

    }
}

static void notification_receiver(void * params){
    unsigned int * notificationValue = calloc(1,sizeof(int));
    for(;;){
        // 1st arg allows you to clear a certain bit/bits
        if(xTaskNotifyWait((0xffffff), 0, notificationValue,portMAX_DELAY)){
            printf("recieved %d\n", *notificationValue);
        }
    }
}


///////////////////////////////////////////////////////////////////////
void learning_semaphore(void){
    // Be careful how you set priorities, you want the semaphore handler task to preempt the get_Http task
    // listenforHTTP->priority < http_semaphore_task->priority for right operation
    bin_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(&http_semaphore_task,"do x w/ http\n", 1024, NULL, 2, NULL);
    xTaskCreate(&listenforHTTP,"get http\n", 1024, NULL, 1, NULL);
    
}


void listenforHTTP(void * params){
    for(;;){
        printf("recieved http message\n");
        xSemaphoreGive(bin_semaphore); // Make the semaphore available/ set the flag
        printf("processed http message\n");
        vTaskDelay( 5000 / portTICK_PERIOD_MS);
    }
}

void http_semaphore_task(void * params){
        for(;;){
            // Is my semaphore available on time?/ Is flag set on time?
            if(xSemaphoreTake( bin_semaphore, 2000/portTICK_PERIOD_MS)){
                printf("doing something with http\n");
            } else{
                printf("time_out\n");
            }
            
        }
}


///////////////////////////////////////////////////////////////////////
void learning_led(void){

    // Semaphores are used to communicate between two tasks; flags
    // Mutex: blocks all other tasks from reading from memory while mutex is in place
    led_params_t * ledParams1 = calloc(1,sizeof(led_params_t));
    ledParams1->led = LED1;
    ledParams1->time = 500;
    ledParams1->increment = 50;
    led_params_t * ledParams2 = calloc(1,sizeof(led_params_t));
    ledParams2->led = LED2;
    ledParams2->time = 500;
    ledParams2->increment = 50;
    
    gpio_set_direction(2, GPIO_MODE_OUTPUT);
    gpio_set_direction(4, GPIO_MODE_OUTPUT);

    // When a task is scheduled, it executes once. 
    // xTaskCreate(led_blink, "led_blink1", 512, ledParams1,1,NULL);
    xTaskCreatePinnedToCore(led_blink, "led_blink1", 512, ledParams1,1,NULL,0); // Min stack is 512 for led blink
    xTaskCreatePinnedToCore(led_blink, "led_blink2", 512, ledParams2,2,NULL,0); // bind to core 1
}

// When a task is scheduled, it executes once. 
// We implement an infinite loop to keep the tasks running.
void led_blink(void * params){
    led_params_t * l = (led_params_t*)params; // Type cast
    for(;;){
        gpio_set_level(l->led,1);
        vTaskDelay(l->time/ portTICK_PERIOD_MS);
        gpio_set_level(l->led,0);
        vTaskDelay(l->time/ portTICK_PERIOD_MS);
        l->time = l->time + l->increment;
        if (l->time < 51){
                l->increment = 75;
        }else if(l->time > 250){
                l->increment = -50;
        }
    }
}
