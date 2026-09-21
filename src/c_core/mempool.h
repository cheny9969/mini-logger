#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#define BLOCK_SIZE 256
#define MEM_POOL_CNT 128

typedef struct Block {
    struct Block* next;
    char data[BLOCK_SIZE];
} Block;

typedef struct MemPool {
    Block* free_list;
    uint8_t* start;
    size_t total_size;
    int block_count;
    pthread_mutex_t mutex; // 新增互斥锁
} MemPool;

MemPool* mempool_create(void);
void mempool_destroy(MemPool* pool);

void* block_alloc(MemPool* pool);
void block_free(MemPool* pool, void* ptr);

#endif
