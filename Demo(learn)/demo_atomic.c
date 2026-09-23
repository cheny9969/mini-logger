#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define THREAD_NUM 4
#define LOOP_CNT 100000

_Atomic long long cnt = 0;

void* add_func(void* arg) {
    for(int i=0; i<LOOP_CNT; i++) {
        atomic_fetch_add(&cnt, 1);
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREAD_NUM];
    for(int i=0; i<THREAD_NUM; i++) {
        pthread_create(&t[i], NULL, add_func, NULL);
    }
    for(int i=0; i<THREAD_NUM; i++) {
        pthread_join(t[i], NULL);
    }
    printf("cnt = %lld, expect %d\n", cnt, THREAD_NUM * LOOP_CNT);
    return 0;
}
