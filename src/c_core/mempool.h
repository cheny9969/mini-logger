#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stddef.h>
#include <stdint.h>

// 固定大小内存块，你可以调整BLOCK_SIZE
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
} MemPool;

// 创建内存池，一次性申请大块内存
MemPool* mempool_create(void);
void mempool_destroy(MemPool* pool);

// 分配/归还内存块
void* block_alloc(MemPool* pool);
void block_free(MemPool* pool, void* ptr);

#endif
