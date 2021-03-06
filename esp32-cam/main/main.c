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
#include "mutex_helper.h"
#include <unistd.h>
#include "esp_timer.h"
#include "esp_sleep.h"
#include "sdkconfig.h"

#include "perfmon.h"

#define TOTAL_CALL_AMOUNT 200
#define PERFMON_TRACELEVEL -1 // -1 - will ignore trace level

// Function Declarations //

mutexed_float_t * setup_motor_encoders();
void update_motor_encoder1( void * params);
void update_motor_encoder2( void * params);
void measure_speed_calc(void * params);
void setup_motor_encoder_pins(mutexed_float_t * mem);
void perfMon(void * called_function);


// Table with dedicated performance counters
static uint32_t pm_check_table[] = {
    XTPERF_CNT_CYCLES, XTPERF_MASK_CYCLES, // total cycles
    XTPERF_CNT_INSN, XTPERF_MASK_INSN_ALL, // total instructions
    XTPERF_CNT_D_LOAD_U1, XTPERF_MASK_D_LOAD_LOCAL_MEM, // Mem read
    XTPERF_CNT_D_STORE_U1, XTPERF_MASK_D_STORE_LOCAL_MEM, // Mem write
    XTPERF_CNT_BUBBLES, XTPERF_MASK_BUBBLES_ALL &(~XTPERF_MASK_BUBBLES_R_HOLD_REG_DEP),  // wait for other reasons
    XTPERF_CNT_BUBBLES, XTPERF_MASK_BUBBLES_R_HOLD_REG_DEP,           // Wait for register dependency
    XTPERF_CNT_OVERFLOW, XTPERF_MASK_OVERFLOW,               // Last test cycle
};



// Main loop //
void app_main(void)
{
    //Initialize NVS
    /* esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret); */
    SemaphoreHandle_t speedMutex;
    float speed = 0;
    printf("Hello Zack!\n");
    
    //learning_led();
    //learning_semaphore();
    //learning_task_handlers();
    //learning_queue();
    //learning_eventGroups();

    mutexed_float_t * motor_memory = setup_motor_encoders();
    //perfMon((void *)setup_motor_encoders);
    speedMutex = (SemaphoreHandle_t) (((SemaphoreHandle_t *)motor_memory->mutexSemaphores)[3]);
    for(;;){
        if(xSemaphoreTake(speedMutex, 100/portTICK_PERIOD_MS)){
            speed = motor_memory->mem[3];
            xSemaphoreGive(speedMutex);
            printf("Speed = %f", speed);
            vTaskDelay(3000/ portTICK_PERIOD_MS); // 3000 ms delay
        }
    }
}

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

void update_motor_encoder1(void * params){
    mutexed_float_t * m = (mutexed_float_t * ) params;
    SemaphoreHandle_t tempSem = (SemaphoreHandle_t) (((SemaphoreHandle_t *)m->mutexSemaphores)[0]);
    if(xSemaphoreTake(tempSem, 100/portTICK_PERIOD_MS)){
        m->mem[0] = m->mem[0] + 374;
        xSemaphoreGive(tempSem);
    }
}

void update_motor_encoder2(void * params){
    mutexed_float_t * m = (mutexed_float_t * ) params;
    SemaphoreHandle_t tempSem = (SemaphoreHandle_t) (((SemaphoreHandle_t *)m->mutexSemaphores)[1]);
    if(xSemaphoreTake(tempSem, 100/portTICK_PERIOD_MS)){
        m->mem[1] = m->mem[1] + 374;
        xSemaphoreGive(tempSem);
    }
}

void measure_speed_calc(void * params){
    mutexed_float_t * m = (mutexed_float_t * ) params;
    float motor1 = 1000;
    float motor2 = 1000;
    uint64_t time_dif = 1;
    SemaphoreHandle_t tempSem;
    struct timeval tv_now;
    uint64_t time_us = 0;


    for(;;){
        gettimeofday(&tv_now, NULL);
        time_us = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;

        tempSem = (SemaphoreHandle_t) (((SemaphoreHandle_t *)m->mutexSemaphores)[0]);
        if(xSemaphoreTake( tempSem, 100/portTICK_PERIOD_MS)){
            motor1 = m->mem[0];
            m->mem[0] = 1000;
            xSemaphoreGive(tempSem);
            tempSem = (SemaphoreHandle_t) (((SemaphoreHandle_t *)m->mutexSemaphores)[1]);
            if(xSemaphoreTake(tempSem, 100/portTICK_PERIOD_MS)){
                motor2 = m->mem[1];
                m->mem[1] = 1000;
                xSemaphoreGive(tempSem);
                tempSem = (SemaphoreHandle_t) (((SemaphoreHandle_t *)m->mutexSemaphores)[2]);
                if(xSemaphoreTake(tempSem, 100/portTICK_PERIOD_MS)){
                    time_dif = time_us - m->mem[2];
                    gettimeofday(&tv_now, NULL);
                    m->mem[2] = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
                    xSemaphoreGive(tempSem);
                    tempSem = (SemaphoreHandle_t) (((SemaphoreHandle_t *)m->mutexSemaphores)[3]);
                    if(xSemaphoreTake(tempSem, 100/portTICK_PERIOD_MS)){
                        // Hall feedback is 374-> 34:1 gear ratio with 11 ticks per round (11*34=374)
                        m->mem[3] = ((motor1 + motor2) + 2) / time_dif / 748;
                        xSemaphoreGive(tempSem);
                        printf("Speed: %f\n",m->mem[3]);
                        vTaskDelay(10000/ portTICK_PERIOD_MS);
                    }
                }
            }
        }
    }
}

void setup_motor_encoder_pins(mutexed_float_t * mem){
    // Allocate memory for encoder_pin configuration
    esp_err_t ret = ESP_OK;
    
    // Install the GPIO ISR handler service; allows per-pin interrupt handlers
    gpio_uninstall_isr_service();
    ESP_ERROR_CHECK(ret);
    ret = gpio_install_isr_service(ESP_INTR_FLAG_SHARED);
    ESP_ERROR_CHECK(ret);
    //ret = gpio_install_isr_service(ESP_INTR_FLAG_SHARED);

    // Initialize pin 15 as an input
     // Pin 15
    ret = gpio_set_direction(14, (gpio_mode_t) 1);
    ESP_ERROR_CHECK(ret);

    ret = gpio_set_direction(15, (gpio_mode_t) 1);
    ESP_ERROR_CHECK(ret);

    ret = gpio_set_intr_type(14,GPIO_INTR_POSEDGE);
    ESP_ERROR_CHECK(ret);

    ret = gpio_set_intr_type(15,GPIO_INTR_POSEDGE);
    ESP_ERROR_CHECK(ret);
    
    // Add interupt handler to pin16; register the posedge interrupt
    ret = gpio_isr_handler_add(14, update_motor_encoder1, (void *) mem);
    ESP_ERROR_CHECK(ret);

    // Add interupt handler to pin16; register the posedge interrupt
    ret = gpio_isr_handler_add(15, update_motor_encoder2, (void *) mem);
    ESP_ERROR_CHECK(ret);

    // Enable the Rising edge interrupts on pins 15&16; ISRs registered to 'update_motor_encoder' functions
    gpio_intr_enable(14);
    ESP_ERROR_CHECK(ret);

    ret = gpio_intr_enable(15);
    ESP_ERROR_CHECK(ret);

    printf("Configured\n\n");
}

mutexed_float_t * setup_motor_encoders(){

    
    mutexed_float_t * motor_encoder_mem = create_mutex_uint64(4);
    struct timeval tv_now;

    
    // Initialize Memory:
    motor_encoder_mem->mem[0] = 0;
    motor_encoder_mem->mem[1] = 0;
    motor_encoder_mem->mem[3] = 0;
    
    // Initialize GPIO Pins 15 and 16 to trigger interupts on rising edge detections
    setup_motor_encoder_pins(motor_encoder_mem);

    // Get the time where the interrupts were enabled to measure speed
    gettimeofday(&tv_now, NULL);
    uint64_t time_us = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
    motor_encoder_mem->mem[2] = time_us;

    // Schedule a task that is in charge of measuring the speed of a motor
    xTaskCreate(measure_speed_calc,"MTRSPD", 2048*3, motor_encoder_mem, 2, NULL);
    return motor_encoder_mem;
}

