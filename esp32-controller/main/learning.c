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
#include <unistd.h>
#include "esp_timer.h"
#include "esp_sleep.h"
#include "perfmon.h"
#include "driver/pcnt.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

// Global Variables // 
SemaphoreHandle_t bin_semaphore;
static TaskHandle_t notification_receiverHandler = NULL;
xQueueHandle queue;
EventGroupHandle_t evtGrp;
const int gotWIFI = BIT0;
const int gotUART = BIT1;

#define LED1 4
#define LED2 5

// Type Declarations //
typedef struct{
        int led;
        int time;
        int increment;
}led_params_t;

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
void learning_eventGroups(void);
void listenForWIFI_eventGroups(void * params);
void listenForUART_eventGroups(void * params);
void eventGroupTask(void * params);
///////////////////////////////////////////////////////////////////////
void overflow_motor_encoder( void * params);
void measure_speed_calc(void * params);
void setup_motor_encoder_pin();
float * setup_motor_encoders(void);
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
void learning_eventGroups(void){
    evtGrp = xEventGroupCreate();
    xTaskCreate(&listenForWIFI_eventGroups,"get wifi packet", 2048, NULL, 2, NULL);
    xTaskCreate(&listenForUART_eventGroups,"get UART packet", 2048, NULL, 2, NULL);
    xTaskCreate(&eventGroupTask,"handle packets", 2048, NULL, 1, NULL);
}

void listenForWIFI_eventGroups(void * params){
    for(;;){
        xEventGroupSetBits(evtGrp, gotWIFI);
        printf("got WIFI packet\n");
        vTaskDelay(2000/ portTICK_PERIOD_MS);
    }
}

void listenForUART_eventGroups(void * params){
    for(;;){
        xEventGroupSetBits(evtGrp, gotUART);
        printf("got UART packet\n");
        vTaskDelay(3000/ portTICK_PERIOD_MS);
    }
}

void eventGroupTask(void * params){
    for(;;){
        xEventGroupWaitBits(evtGrp, gotWIFI | gotUART, pdTRUE, pdTRUE, portMAX_DELAY);
        printf("received packets from WIFI and UART\n");
    }
}

///////////////////////////////////////////////////////////////////////
void learning_queue(void){
    queue = xQueueCreate(3,sizeof(int)); // q len, size of each element
    xTaskCreate(&listenForHTTPQ,"get http off Q", 2048*3, NULL, 2, NULL);
    xTaskCreate(&httpQDissect,"handle http data", 2048*3, NULL, 1, NULL);
}
    
void listenForHTTPQ(void * params){
    int count = 0;
    for(;;){
        count = count < 1000?count + 1:0;
        printf("recieved http message\n");
        long ok = xQueueSend(queue, &count, 1000/ portTICK_PERIOD_MS);
        if(ok){
            printf("added message to queue\n");
        }else{
            printf("Failed to add message to queue\n");
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
        // vTaskDelay(100/portTICK_PERIOD_MS); // forces us into failed to add
        // vTaskDelay(6000/portTICK_PERIOD_MS); // forces us into failed to remove
    }
}

void httpQDissect(void * params){
    for(;;){
        int rxInt;
        if (xQueueReceive(queue, &rxInt, 5000/portTICK_PERIOD_MS)){
            printf("doing something with http %d\n",rxInt);
        }else{
            printf("Failed to remove message to queue\n");
        }
        // vTaskDelay(2000/portTICK_PERIOD_MS); //forces us into failed to add
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

void overflow_motor_encoder(void * params){
    static esp_err_t err = ESP_OK;
    pcnt_config_t * m_config;
    m_config = (pcnt_config_t * ) params;
    err = pcnt_counter_clear(m_config->unit);
    ESP_ERROR_CHECK(err);
}


void measure_speed_calc(void * params){
    static float * speed = NULL;
    static short signed int motor1 = 10;
    static short signed int motor2 = 10;
    static uint64_t time_dif = 1;
    static uint64_t time_us = 1;
    static struct timeval tv_now;
    static esp_err_t err = ESP_OK;
    static int count = 0;

    speed = (float * ) params;

    for(;;){
        // Disable the pulse counters
        err = pcnt_counter_pause(PCNT_UNIT_0);
        ESP_ERROR_CHECK(err);
        err = pcnt_counter_pause(PCNT_UNIT_1);
        ESP_ERROR_CHECK(err);

        // Get the time
        gettimeofday(&tv_now, NULL);
        time_dif = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec - time_us;
        
        // | You hold the keys to the memory associated with motor speed calculation here  |  //
        // |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |  //
        // V    V    V    V    V    V    V    V    V    V    V    V    V    V    V    V    V  //
        do{
            // Attempt to grab next mutex locking the memory used to update our speed calculation algorithm
            // Be careful once you get the value because it needs to be fast so that we dont block the incoming data for long
            // You have a memory element! Quickly do a couple of operations! //
            switch(count){
                case 0:
                    err = pcnt_get_counter_value(PCNT_UNIT_0, &motor1);
                    ESP_ERROR_CHECK(err);
                    err = pcnt_counter_clear(PCNT_UNIT_0);
                    ESP_ERROR_CHECK(err);
                    break;
                case 1:
                    err = pcnt_get_counter_value(PCNT_UNIT_1, &motor2);
                    ESP_ERROR_CHECK(err);
                    err = pcnt_counter_clear(PCNT_UNIT_1);
                    ESP_ERROR_CHECK(err);
                    break;
                case 2:
                     // Hall feedback is 374-> 34:1 gear ratio with 11 ticks per round (11*34=374)
                    gettimeofday(&tv_now, NULL);
                    time_us = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
                    // Immediately resume counters            
                    err = pcnt_counter_resume(PCNT_UNIT_0);
                    ESP_ERROR_CHECK(err);

                    err = pcnt_counter_resume(PCNT_UNIT_1);
                    ESP_ERROR_CHECK(err);
                    break;
                case 3:
                    printf("ticks = %d\n", motor1 + motor2);
                    *speed = ((motor1 + motor2) + 2) / time_dif / 748; // MATHS
                    break;
            }
            count += 1;
        }while(count <= 3);
        
        
        // Reset count and wait for a bit before we recalculate
        count = 0;
        vTaskDelay(500/ portTICK_PERIOD_MS);
    }
    printf("Should not get here\n");
}


void setup_motor_encoder_pins(void){
    // Allocate memory for encoder_pin configuration
    esp_err_t err = ESP_OK;
    const pcnt_config_t pcnt_config1 = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = 4,
        .ctrl_gpio_num = 5,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT_0,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_KEEP, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = 374,
        .counter_l_lim = -1,
    };

    /* Initialize PCNT unit 0 */
    err = pcnt_unit_config(&pcnt_config1);
    ESP_ERROR_CHECK(err);
    err = pcnt_event_enable(pcnt_config1.unit, PCNT_EVT_H_LIM);
    ESP_ERROR_CHECK(err);
    err = pcnt_counter_pause(pcnt_config1.unit);
    ESP_ERROR_CHECK(err);
    err = pcnt_counter_clear(pcnt_config1.unit);
    ESP_ERROR_CHECK(err);

    
    const pcnt_config_t pcnt_config2 = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = 2,
        .ctrl_gpio_num = 5,
        .channel = PCNT_CHANNEL_1,
        .unit = PCNT_UNIT_1,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_KEEP,  // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,  // Keep the primary counter mode if high
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = 374,
        .counter_l_lim = -1,
    };

    /* Initialize PCNT unit 1 */
    err = pcnt_unit_config(&pcnt_config2);
    ESP_ERROR_CHECK(err);
    //pcnt_config.channel = 1;
    err = pcnt_event_enable(pcnt_config2.unit, PCNT_EVT_H_LIM);
    ESP_ERROR_CHECK(err);
    err = pcnt_counter_pause(pcnt_config2.unit);
    ESP_ERROR_CHECK(err);
    err = pcnt_counter_clear(pcnt_config2.unit);
    ESP_ERROR_CHECK(err);

    // Set up the interupt event handlers and counting mechanism
    err = pcnt_isr_service_install(0);
    ESP_ERROR_CHECK(err);

    err = pcnt_isr_handler_add(pcnt_config1.unit, overflow_motor_encoder,(void * ) &(pcnt_config1));
    ESP_ERROR_CHECK(err);
    err = pcnt_isr_handler_add(pcnt_config2.unit, overflow_motor_encoder,(void * ) &(pcnt_config2));
    ESP_ERROR_CHECK(err);

    err = pcnt_counter_resume(pcnt_config1.unit);
    ESP_ERROR_CHECK(err);

    err = pcnt_counter_resume(pcnt_config1.unit);
    ESP_ERROR_CHECK(err);
}


float * setup_motor_encoders(){
    //esp_err_t err = ESP_OK;
    float * speed = calloc(1,sizeof(float));
    // Initialize GPIO Pins 15 and 16 to trigger interupts on rising edge detections
    setup_motor_encoder_pins();

    // ISRs activated to 'update_motor_encoder' as ASYNC functions
    printf("Motor Encoder Interrupts Configured\n\n");
    
    // Schedule a task that is in charge of measuring the speed of a motor
    xTaskCreate(measure_speed_calc,"MTRSPD", 2048*3, speed, 2, NULL);
    return speed;
}


