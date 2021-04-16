#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include "freertos/FreeRTOS.h"

// MOTOR1 setup pins
#define MOTOR1_ENCODER1 25 // A0
#define MOTOR1_ENCODER2 26 // A1
#define MOTOR1_PWM 22
#define MOTOR1_DIR 32

// MOTOR2 setup pins *A3 & A4 are off limits for wifi purposes ->see gpio.h
#define MOTOR2_ENCODER1 17 // 
#define MOTOR2_ENCODER2 21 // 
#define MOTOR2_PWM 23
#define MOTOR2_DIR 14

#define WHEEL_DIAMETER 1

// Capture Signal enable bits
#define CAP0_INT_EN BIT(27)  //Capture 0 interrupt bit
#define CAP1_INT_EN BIT(28)  //Capture 1 interrupt bit


typedef struct encoder_t{
    uint32_t ticks;
    uint32_t old_ticks;
    uint64_t time;
    float speed;
}encoder_t;

typedef struct motor_t{
    encoder_t * encoder_mem;
    float speed;
}motor_t;

xQueueHandle motor_encoders_queue;


motor_t * setup_motor_pins(void);
void update_motor_speed(void * args);
static void IRAM_ATTR motor_speed_handler(void * arg);

/*
FUNCTION: update_motor_speed
ARGS:     void * args (unused)
RETURN    None
USAGE:    Speed Calculation function manages 'instantaneous' speed. Likely needs to be sampled and smoothed. First to know when we have stopped (set the delay time to be less and add a timeout condition)
ERRORS:   See  functions @https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-reference/peripherals/mcpwm.html?highlight=mcpwm_isr_register#_CPPv418mcpwm_isr_register12mcpwm_unit_tPFvPvEPviP13intr_handle_t
*/
void update_motor_speed(void * args){
    static encoder_t * encoder_mem;
    static struct timeval tv_now;
    static uint64_t time;

    for(;;){
        xQueueReceive(motor_encoders_queue, &encoder_mem, portMAX_DELAY);
        // Get the time for speed calculations
        gettimeofday(&tv_now, NULL);
        time = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
        if(((time - encoder_mem[0].time) > 500000) | ((time - encoder_mem[1].time) > 500000)){
            encoder_mem[0].speed = ((encoder_mem[0].ticks - encoder_mem[0].old_ticks + 1) / 374.0) / (time - encoder_mem[0].time) * 1000000 * WHEEL_DIAMETER;
            encoder_mem[1].speed = ((encoder_mem[1].ticks - encoder_mem[1].old_ticks + 1) / 374.0) / (time - encoder_mem[1].time) * 1000000 * WHEEL_DIAMETER;
            
            // Reset the time to prevent overflows
            if(tv_now.tv_sec > 30){
                tv_now.tv_sec = 0 - tv_now.tv_sec;
                tv_now.tv_usec = 0 - tv_now.tv_usec;
                adjtime(&tv_now, NULL);
            }
            
            gettimeofday(&tv_now, NULL);
            time = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;

            // Update the last measured tick values
            encoder_mem[0].old_ticks = encoder_mem[0].ticks;
            encoder_mem[1].old_ticks = encoder_mem[1].ticks;

            // Update the time
            encoder_mem[0].time = time;
            encoder_mem[1].time = time; 
        }
    }
}

/*
FUNCTION: motor_speed_handler
ARGS:     void * args (encoder_t *)
RETURN    None
USAGE:    Initialized by setup_motor_pins as the posedge detection handler for the motor encoders.
          Every call to this function represents ~1 degree rotation in the motor.
ERRORS:   See  functions @https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-reference/peripherals/mcpwm.html?highlight=mcpwm_isr_register#_CPPv418mcpwm_isr_register12mcpwm_unit_tPFvPvEPviP13intr_handle_t
*/
static void IRAM_ATTR motor_speed_handler(void * arg){
    encoder_t * encoder_mem;
    uint32_t mcpwm_intr_status;

    encoder_mem = (encoder_t *) arg;
    mcpwm_intr_status = MCPWM0.int_st.val;

    if((mcpwm_intr_status & CAP0_INT_EN) | (mcpwm_intr_status & CAP1_INT_EN)){
        if (mcpwm_intr_status & CAP0_INT_EN){
            if(encoder_mem[0].ticks > 0x7FFFFFF0){
                encoder_mem[0].ticks = 0;
            }else{
                encoder_mem[0].ticks += 1;
            }
        }else{
            if(encoder_mem[1].ticks > 0x7FFFFFF0){
                encoder_mem[1].ticks = 0;
            }else{
                encoder_mem[1].ticks += 1;
            }
        }
        MCPWM0.int_clr.val = mcpwm_intr_status;
    }else{
        mcpwm_intr_status = MCPWM1.int_st.val;
        if (mcpwm_intr_status & CAP0_INT_EN){
            if(encoder_mem[0].ticks > 0x7FFFFFF0){
                encoder_mem[0].ticks = 0;
            }else{
                encoder_mem[0].ticks += 1;
            }
        }else{
            if(encoder_mem[1].ticks > 0x7FFFFFF0){
                encoder_mem[1].ticks = 0;
            }else{
                encoder_mem[1].ticks += 1;
            }
        }
        MCPWM1.int_clr.val = mcpwm_intr_status;
    }
    xQueueSendFromISR(motor_encoders_queue, &encoder_mem, NULL);
}

/*
FUNCTION: setup_motor_pins
ARGS:     None
RETURN    None
USAGE:    Initializes the MCPWM for esp32. Two PWM outputs with rising edge triggered encoder counter.
          Make sure to allocate the memory for the motor_encoder_queue before calling this function
ERRORS:   See MCPWM functions @https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/mcpwm.html?highlight=mcpwm#mcpwm
*/
motor_t * setup_motor_pins(){
    mcpwm_config_t pwm_config;
    encoder_t * encoders1 = calloc(2, sizeof(encoder_t));
    encoder_t * encoders2 = calloc(2, sizeof(encoder_t));
    motor_t * motors = calloc(2, sizeof(motor_t));

    motors[0].encoder_mem = encoders1;
    motors[1].encoder_mem = encoders2;

    // Initialize Motor Controller Unit 0 with 2 output lines and 2 input lines
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, MOTOR1_PWM);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_CAP_0, MOTOR1_ENCODER1);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_CAP_1, MOTOR1_ENCODER2);
    
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0A, MOTOR2_PWM);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM_CAP_0, MOTOR2_ENCODER1);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM_CAP_1, MOTOR2_ENCODER2);

    // inputs
    gpio_pulldown_en(MOTOR1_ENCODER1); 
    gpio_pulldown_en(MOTOR1_ENCODER2);
    gpio_pulldown_en(MOTOR2_ENCODER1); 
    gpio_pulldown_en(MOTOR2_ENCODER2);

    // Outputs
    // Direction Outputs
    gpio_set_direction(MOTOR1_DIR,GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR2_DIR,GPIO_MODE_OUTPUT);
    gpio_set_level(MOTOR1_DIR,1);
    gpio_set_level(MOTOR2_DIR,1);

    // PWM outputs for PWMxA and PWMxB
    pwm_config.frequency = 1000;
    pwm_config.cmpr_a = 10; // Duty cycle % Initialized
    pwm_config.cmpr_b = 10; // Duty cycle % Uninitialized
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0; // Active High Duty Cycle
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_0, &pwm_config);

    // Motor 1 Encoders Setup
    mcpwm_capture_enable(MCPWM_UNIT_0, MCPWM_SELECT_CAP0, MCPWM_POS_EDGE, 0);
    mcpwm_capture_enable(MCPWM_UNIT_0, MCPWM_SELECT_CAP1, MCPWM_POS_EDGE, 0);
    
    // Motor 2 Encoders Setup
    mcpwm_capture_enable(MCPWM_UNIT_1, MCPWM_SELECT_CAP0, MCPWM_POS_EDGE, 0);
    mcpwm_capture_enable(MCPWM_UNIT_1, MCPWM_SELECT_CAP1, MCPWM_POS_EDGE, 0);
    
    // Enable the interrupts for pos_edge 
    MCPWM0.int_ena.val = CAP0_INT_EN | CAP1_INT_EN;
    MCPWM1.int_ena.val = CAP0_INT_EN | CAP1_INT_EN;
    
    mcpwm_isr_register(MCPWM_UNIT_0, motor_speed_handler, encoders1, ESP_INTR_FLAG_IRAM, NULL);
    mcpwm_isr_register(MCPWM_UNIT_1, motor_speed_handler, encoders2, ESP_INTR_FLAG_IRAM, NULL);
    
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A, 50);
    mcpwm_set_duty(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM0A, 50);

    return motors;
}


