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
    esp_err_t err = ESP_OK;

    mutexed_float_t * m = calloc(1, sizeof(mutexed_float_t));
    while(m == NULL){
        m = calloc(1, sizeof(mutexed_float_t));
    }

    m->mem = NULL;
    m->config = NULL;
    m->mutexSemaphores = NULL;
    // Create each configuration
    while(m->config == NULL){
        m->config = calloc(numItems, sizeof(pthread_mutexattr_t));
    }

    // create the lock semaphore/flag
    while(m->mutexSemaphores == NULL){
        m->mutexSemaphores = calloc(numItems, sizeof(pthread_mutex_t));
    }
    

    printf("START\n");
    for(int i = 0; i < numItems; i++){
        pthread_mutexattr_settype(&m->config[i], PTHREAD_MUTEX_TIMED_NP);
        if (err != ESP_OK){
            printf("Failed to make mutex configuration %d",i);
            free(m->config);
            free(m->mutexSemaphores);
            free(m);
            return NULL;
        }
        pthread_mutex_init(&m->mutexSemaphores[i], &m->config[i]);
        if (err != ESP_OK){
            printf("Failed to make mutex %d",i);
            free(m->config);
            free(m->mutexSemaphores);
            free(m);
            return NULL;
        }
        printf("%d\n",i);
    }
    printf("MUTEXES INITIALIZED\n");

    // create the protected memory based on the arguments (same as calloc)
    while(m->mem == NULL){
        m->mem = calloc(numItems, sizeof(float));
    }
    
    return m;
}