#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"

typedef struct{
        float * mem;
        SemaphoreHandle_t * mutexSemaphores;
}mutexed_float_t;

mutexed_float_t * create_mutex_uint64(int numItems){
    // Create a locked memory data structure
    mutexed_float_t * m = calloc(1, sizeof(mutexed_float_t));
    // create the lock semaphore/flag
    m->mutexSemaphores = calloc(numItems, sizeof(SemaphoreHandle_t));
    for(int i = 0; i < numItems; i++){
        m->mutexSemaphores[i] = xSemaphoreCreateMutex();
    }
    // create the protected memory based on the arguments (same as calloc)
    m->mem = calloc(numItems, sizeof(float));
    return m;
}