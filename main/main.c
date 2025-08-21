#include <stdio.h>
#include <pthread.h>

#include "log.h"

// 简单屏幕输出测试
void simple_test() {
    d_mod_1_debug("This is a debug message from mod 1");
    d_mod_1_info("This is an info message from mod 1");
    d_mod_1_warn("This is a warning message from mod 1");
    d_mod_1_error("This is an error message from mod 1");

    d_mod_2_debug("This is a debug message from mod 2");
    d_mod_2_info("This is an info message from mod 2");
    d_mod_2_warn("This is a warning message from mod 2");
    d_mod_2_error("This is an error message from mod 2");
}

// 多线程测试
void* thread_test_func(void* arg) {
    const int *thread_idx = (const int*)arg;

    for (int i = 0; i < 500; i++) {
        d_mod_1_debug("Thread %d - Debug message %d", *thread_idx, i);
        d_mod_1_info("Thread %d - Info message %d", *thread_idx, i);
        d_mod_1_warn("Thread %d - Warning message %d", *thread_idx, i);
        d_mod_1_error("Thread %d - Error message %d", *thread_idx, i);

        d_mod_2_debug("Thread %d - Debug message %d", *thread_idx, i);
        d_mod_2_info("Thread %d - Info message %d", *thread_idx, i);
        d_mod_2_warn("Thread %d - Warning message %d", *thread_idx, i);
        d_mod_2_error("Thread %d - Error message %d", *thread_idx, i);
    }
    return NULL;
}
void multi_thread_test() {
    const int thread_count = 10;
    pthread_t threads[thread_count];
    const int thread_indices[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, thread_test_func, (void*)&thread_indices[i]) != 0) {
            fprintf(stderr, "Failed to create thread for thread idx: %d\n", thread_indices[i]);
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main() {
    // simple_test();
    multi_thread_test();
    return 0;
}