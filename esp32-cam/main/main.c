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
#include "esp_task_wdt.h"
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

//#define CONFIG_FREERTOS_HZ 1000
//#define CONFIG_ESP_INT_WDT 10000
#define TOTAL_CALL_AMOUNT 200 // Used with Performance Monitor
#define PERFMON_TRACELEVEL -1 // -1 - will ignore trace level
#define DEV_DEBUG_PRESENT
#define MOTOR1 16
#define MOTOR2 17

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
    printf("Hello Zack!\n");
    
    //learning_led();
    //learning_semaphore();
    //learning_task_handlers();
    //learning_queue();
    //learning_eventGroups();

    mutexed_float_t * motor_memory = setup_motor_encoders();
    //perfMon((void *)setup_motor_encoders);
    for(;;){
        vTaskDelay(3000/ portTICK_PERIOD_MS); // 3000 ms delay
        printf("Speed = %f\n", motor_memory->mem[2]);
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
    static mutexed_float_t * m = 0x0;
    static pthread_mutex_t tempSem = 0x0;
    static esp_err_t err = ESP_OK;
    m = (mutexed_float_t * ) params;
    
    tempSem = (pthread_mutex_t) (((pthread_mutex_t *)m->mutexSemaphores)[0]);
    err = pthread_mutex_trylock(&tempSem);
    if(err != ESP_OK){
        printf("Interupt collision. We need this to work.\n");
    }else{
        m->mem[0] = m->mem[0] + 374;
        pthread_mutex_unlock(&tempSem);
        printf("Ticks2: %f\n", m->mem[0]);
    }
}

void update_motor_encoder2(void * params){
    static mutexed_float_t * m = 0x0;
    static pthread_mutex_t tempSem = 0x0;
    static esp_err_t err = ESP_OK;
    m = (mutexed_float_t * ) params;

    tempSem = (pthread_mutex_t) (((pthread_mutex_t *)m->mutexSemaphores)[1]);
    err = pthread_mutex_trylock(&tempSem);
    if(err != ESP_OK){
        printf("Interupt collision. We need this to work.\n");
    }else{
        m->mem[1] = m->mem[1] + 374;
        pthread_mutex_unlock(&tempSem);
        printf("Ticks2: %f\n", m->mem[1]);
    }
}

void measure_speed_calc(void * params){
    static mutexed_float_t * m = 0x0;
    static float motor1 = 1000;
    static float motor2 = 1000;
    static uint64_t time_dif = 1;
    static pthread_mutex_t tempSem = 0x0;
    static struct timeval tv_now;
    static esp_err_t err = ESP_OK;
    static int count = 0;

    m = (mutexed_float_t * ) params;

    for(;;){
        printf("Calculating Speed...\n\n");
        // Execute before managing memory locks
        gettimeofday(&tv_now, NULL);
        time_dif = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec - m->mem[3];
        
        err = gpio_intr_disable(MOTOR1);
        if(err != ESP_OK){
            printf("Handles error %x by discarding the attempt at stopping interrupt.\n",err);
            continue;
        }
        err = gpio_intr_disable(MOTOR2);
        if(err != ESP_OK){
            printf("Handles error %x by discarding the attempt at stopping interrupt.\n",err);
            continue;
        }
        
        // | You hold the keys to the memory associated with motor speed calculation here  |  //
        // |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |  //
        // V    V    V    V    V    V    V    V    V    V    V    V    V    V    V    V    V  //
        do{
            // Attempt to grab next mutex locking the memory used to update our speed calculation algorithm
            // Be careful once you get the value because it needs to be fast so that we dont block the incoming data for long
            // You have a memory element! Quickly do a couple of operations! //
            switch(count){
                case 0:
                    motor1 = m->mem[count]; //  LD STR
                    tempSem = (pthread_mutex_t) (((pthread_mutex_t *)m->mutexSemaphores)[count]);
                    err = pthread_mutex_trylock(&tempSem);
                    if(err != ESP_OK){
                        printf("Handles error %x by discarding the attempt at calculating speed.\n",err);
                        break;
                    }
                    
                    m->mem[count] = 1000; //  ADD 1 STR
                    
                    // You have a memory element! Quickly give it back! //
                    err = pthread_mutex_unlock(&tempSem);
                    break;
                case 1:
                    motor2 = m->mem[count]; // LD STR
                    tempSem = (pthread_mutex_t) (((pthread_mutex_t *)m->mutexSemaphores)[count]);
                    err = pthread_mutex_trylock(&tempSem);
                    if(err != ESP_OK){
                        printf("Handles error %x by discarding the attempt at calculating speed.\n",err);
                        break;
                    }

                    m->mem[count] = 1000; //  ADD 1 STR //  ADD 1 STR
                    // You have a memory element! Quickly give it back! //
                    err = pthread_mutex_unlock(&tempSem);
                    break;
                case 2:
                    tempSem = (pthread_mutex_t) (((pthread_mutex_t *)m->mutexSemaphores)[count]);
                    err = pthread_mutex_trylock(&tempSem);
                    if(err != ESP_OK){
                        break;
                    }
                     // Hall feedback is 374-> 34:1 gear ratio with 11 ticks per round (11*34=374)
                    m->mem[count] = ((motor1 + motor2) + 2) / time_dif / 748; // MATHS
                    
                    #ifdef DEV_DEBUG_PRESENT 
                    //printf("Speed: %f\n",m->mem[3]);
                    #endif

                    err = pthread_mutex_unlock(&tempSem);
                    break;
                case 3:
                    gettimeofday(&tv_now, NULL);
                    tempSem = (pthread_mutex_t) (((pthread_mutex_t *)m->mutexSemaphores)[count]);
                    err = pthread_mutex_trylock(&tempSem);
                    if(err != ESP_OK){
                        break;
                    }
                    
                    m->mem[count] = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
                    
                    err = pthread_mutex_unlock(&tempSem);
                    break;
            }
            count += 1;
        }while(count <= 3);

        err = gpio_intr_enable(MOTOR1);
        if(err != ESP_OK){
            printf("Interrupt not scheduled. Err %x.\n",err);
            break;
        }
        
        err = gpio_intr_enable(MOTOR2);
        if(err != ESP_OK){
            printf("Interrupt not scheduled. Err %x.\n",err);
            break;
        }
        // Reset count and wait for a bit before we recalculate
        count = 0;
        printf("Next Caclulaion in 5s\n");
        vTaskDelay(5000/ portTICK_PERIOD_MS);
    }
    printf("Should not get here\n");
}

void setup_motor_encoder_pins(mutexed_float_t * mem){
    // Allocate memory for encoder_pin configuration
    esp_err_t err = ESP_OK;
    
    // Install the GPIO ISR handler service; allows per-pin interrupt handlers
    gpio_uninstall_isr_service();
    
    err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    ESP_ERROR_CHECK(err);
    //err = gpio_install_isr_service(ESP_INTR_FLAG_SHARED);

    // Initialize pin 14 as an input
    err = gpio_set_direction(MOTOR1, (gpio_mode_t) 1);
    ESP_ERROR_CHECK(err);

    // Initialize pin 15 as an input
    err = gpio_set_direction(MOTOR2, (gpio_mode_t) 1);
    ESP_ERROR_CHECK(err);

    // Set Interrupt on pin as an input
    err = gpio_set_intr_type(MOTOR1,GPIO_INTR_POSEDGE);
    ESP_ERROR_CHECK(err);

    // Set Interrupt on pin as an input
    err = gpio_set_intr_type(MOTOR2,GPIO_INTR_POSEDGE);
    ESP_ERROR_CHECK(err);
    
    // Add interupt handler to pin14; register the posedge interrupt
    err = gpio_isr_handler_add(MOTOR1, update_motor_encoder1, (void *) mem);
    ESP_ERROR_CHECK(err);

    // Add interupt handler to pin15; register the posedge interrupt
    err = gpio_isr_handler_add(MOTOR2, update_motor_encoder2, (void *) mem);
    ESP_ERROR_CHECK(err);

    // Enable the Rising edge interrupts on pin 14
    err = gpio_intr_enable(MOTOR1);
    ESP_ERROR_CHECK(err);

    // Enable the Rising edge interrupts on pin 15
    err = gpio_intr_enable(MOTOR2);
    ESP_ERROR_CHECK(err);

    // ISRs activated to 'update_motor_encoder' as ASYNC functions
    printf("Motor Encoder Interrupts Configured\n\n");
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

