#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define MS_TO_S 1000
#define MS_TO_NS 1000000L

// Helper function to sleep the specified ms using clock_nanosleep. Returns clock_nanosleep error code
int sleep_ms(int ms)
{
    struct timespec sleep_timespec;
    sleep_timespec.tv_sec = ms / MS_TO_S;
    sleep_timespec.tv_nsec = (ms % MS_TO_S) * MS_TO_NS;
    // Use clock monotonic for an unchangeable reference
    return clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_timespec, NULL);

}

void* threadfunc(void* thread_param)
{

    int rc;
    // cast to obtain thread arguments from parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    if (thread_func_args == NULL) {
    	return NULL;
    }

    // Sleep wait_to_obtain_ms, checking for errors
    rc = sleep_ms(thread_func_args->wait_to_obtain_ms);
    if (rc != 0) {
    	thread_func_args->thread_complete_success = false;
    	return thread_param;    
    }
    
    // Obtain mutex, checking for errors
    rc = pthread_mutex_lock(thread_func_args->thread_mutex);
    if (rc != 0) {
    	thread_func_args->thread_complete_success = false;
    	return thread_param;    
    }
    
    // Sleep wait_to_release_ms, checking for errors
    rc = sleep_ms(thread_func_args->wait_to_release_ms);
    if (rc != 0) {
    	thread_func_args->thread_complete_success = false;
    	return thread_param;    
    }
    
    // Release the mutex, checking for errors
    rc = pthread_mutex_unlock(thread_func_args->thread_mutex);
        if (rc != 0) {
    	thread_func_args->thread_complete_success = false;
    	return thread_param;    
    }

    // Successful exit!
    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    int rc;
    
    // Allocate memory for thread_data
    struct thread_data *pass_data = malloc(sizeof(struct thread_data));
    if (pass_data == NULL) {
        return false;
    }
    
    // Setup arguments
    pass_data->wait_to_obtain_ms = wait_to_obtain_ms;
    pass_data->wait_to_release_ms = wait_to_release_ms;
    pass_data->thread_mutex = mutex;
    pass_data->thread_complete_success = false;
    
    // Pass thread_data to created thread using threadfunc() as entry point
    rc = pthread_create(thread, NULL, threadfunc, pass_data);
    if (rc != 0) {
        free(pass_data);
        return false;
    }
    

    // Return true if successful
    return true;
}

