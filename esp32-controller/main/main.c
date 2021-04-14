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
#include <unistd.h>
#include "esp_timer.h"
#include "esp_sleep.h"
#include "perfmon.h"
#include "motor_cntrl.h"
#include "camera_stream.c"

//#define CONFIG_FREERTOS_HZ 1000
//#define CONFIG_ESP_INT_WDT 10000
#define TOTAL_CALL_AMOUNT 200 // Used with Performance Monitor
#define PERFMON_TRACELEVEL -1 // -1 - will ignore trace level
#define DEV_DEBUG_PRESENT


//#define PERF_MON_EN

// Function Declarations //
#ifdef PERF_MON_EN
void perfMon(void * called_function);
#endif



// Table with dedicated performance counters
#ifdef PERF_MON_EN
static uint32_t pm_check_table[] = {
    XTPERF_CNT_CYCLES, XTPERF_MASK_CYCLES, // total cycles
    XTPERF_CNT_INSN, XTPERF_MASK_INSN_ALL, // total instructions
    XTPERF_CNT_D_LOAD_U1, XTPERF_MASK_D_LOAD_LOCAL_MEM, // Mem read
    XTPERF_CNT_D_STORE_U1, XTPERF_MASK_D_STORE_LOCAL_MEM, // Mem write
    XTPERF_CNT_BUBBLES, XTPERF_MASK_BUBBLES_ALL &(~XTPERF_MASK_BUBBLES_R_HOLD_REG_DEP),  // wait for other reasons
    XTPERF_CNT_BUBBLES, XTPERF_MASK_BUBBLES_R_HOLD_REG_DEP,           // Wait for register dependency
    XTPERF_CNT_OVERFLOW, XTPERF_MASK_OVERFLOW,               // Last test cycle
};
#endif

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


    //float * speed = setup_motor_encoders();
    motor_t * motors;
    int tempDuty1 = 35;
    int tempDuty2 = 30;
    int inc1 = -5;
    int inc2 = -5;

    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    	// initialize the tcp stack
	tcpip_adapter_init();


    start_stream();

    motor_encoders_queue = xQueueCreate(4, sizeof(int * ));

    xTaskCreate(update_motor_speed, "MTRSPD", 4096, NULL, 5, NULL);
    motors = setup_motor_pins();

    //perfMon((void *)setup_motor_encoders);
    for(;;){
        vTaskDelay(2000/ portTICK_PERIOD_MS); // 3000 ms delay
        motors[0].speed = (motors[0].encoder_mem[0].speed + motors[0].encoder_mem[1].speed + .001) / 2;
        motors[1].speed = (motors[1].encoder_mem[0].speed + motors[1].encoder_mem[1].speed + .001) / 2;
        
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A,tempDuty1);
        mcpwm_set_duty(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM0A,tempDuty2);
        gpio_set_level(MOTOR1_DIR, (inc1 > 0)? 1:0);
        gpio_set_level(MOTOR2_DIR, (inc2 > 0)? 1:0);
        
        if (tempDuty1 > 79){
            inc1 = -5;
        }else if(tempDuty1 < 21){
            inc1 = 5;
        }

        if (tempDuty1 > 79){
            inc2 = -5;
        }else if(tempDuty1 < 21){
            inc2 = 5;
        } 

        tempDuty1 = tempDuty1;// + inc1;
        tempDuty2 = tempDuty2;// + inc2;

        printf("Duty Cycle 1: %d\n", tempDuty1 ); 
        printf("Duty Cycle 2: %d\n", tempDuty2 );
    }
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

