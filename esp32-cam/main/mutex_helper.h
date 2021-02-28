#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"

typedef struct{
        void * mem;
        SemaphoreHandle_t mutexSemaphore;
        void * read; // func handle for protected read
        void * write; // func handle for protected write
}locked_mem_t;

locked_mem_t * create_mutex_mem(int numItems, int sizeof_mem, void * readFunc, void * writeFunc){
    // Create a locked memory data structure
    locked_mem_t * m = calloc(1, sizeof(locked_mem_t));
    // create the lock semaphore/flag
    m->mutexSemaphore = xSemaphoreCreateMutex();
    // create the protected memory based on the arguments (same as calloc)
    m->mem = calloc(numItems, sizeof_mem);
    m->read = read_task;
    m->write = write_task;
    return m;
}

unsigned int read_uint_task(void * memory){
    // xSemaphoreTake Locks the memory ; only waits 10ms to read memory before returning invalid val '{32{1'b1}}
    val = 0xffffffff; // Not valid
    if(xSemaphoreTake(m->mutexSemaphore, 10/portTICK_PERIOD_MS)){
        val = m->mem;
        xSemaphoreGive(mutexBus);
    }
    return val;
}

/*
        Input: void ** args
                args[0] = address to locked_mem_t
                args[1] = address to value that belongs in locked memory (can be address of register)
        Output: none* (args[0]->mem will be updated to the value in address of args[1] 
*/
void write_uint_task(void ** args){
    m = (locked_mem_t*) args[0];
    value = (void *) args[1];
    // xSemaphoreTake Locks the memory ; only waits 10ms to write to memory
    if(xSemaphoreTake(m->mutexSemaphore, 10/portTICK_PERIOD_MS)){
        m->mem = *value; // Access the memory that was passed in as the 2nd argument and assign the value to mem
        xSemaphoreGive(m->mutexSemaphore);
    }
}
