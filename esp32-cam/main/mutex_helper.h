#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"
#include <pthread.h>


typedef struct{
        float * mem;
        pthread_mutexattr_t * config;
        pthread_mutex_t * mutexSemaphores;
}mutexed_float_t;

mutexed_float_t * create_mutex_uint64(int numItems){
    // Create a locked memory data structure
    mutexed_float_t * m = calloc(1, sizeof(mutexed_float_t));

    // Create each configuration
    m->mutexSemaphores = calloc(numItems, sizeof(pthread_mutexattr_t));
    printf("START\n");
    for(int i = 0; i < numItems; i++){
        pthread_mutexattr_settype(&m->config[i], PTHREAD_MUTEX_NORMAL);
        printf("%d\n",i);
    }
    printf("ATTR DONE\n");
    // create the lock semaphore/flag
    m->mutexSemaphores = calloc(numItems, sizeof(pthread_mutex_t));
    for(int i = 0; i < numItems; i++){
        pthread_mutex_init(&m->mutexSemaphores[i], &m->config[i]);
        printf("%d\n",i);
    }
    printf("MUTEX MADE\n");
    // create the protected memory based on the arguments (same as calloc)
    m->mem = calloc(numItems, sizeof(float));
    return m;
}