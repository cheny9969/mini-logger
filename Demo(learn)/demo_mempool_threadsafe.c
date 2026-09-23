#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

#define BLOCK_SIZE 128
#define BLOCK_CNT 32
#define THREAD_NUM 4
#define ALLOC_CNT 10000

typedef struct Block {
    struct Block* next;
    char data[BLOCK_SIZE];
} Block;

typedef struct MemPool {
    Block* free_list;
    void* start;
    pthread_mutex_t lock;
} MemPool;

MemPool* mempool_create(void) {
    //%zu专门用于打印 size_t 类型，无符号整数
    /*
     * size_t、ssize_t、uintptr_t 全部是标准库头文件 typedef 出来，定义在 <stddef.h> 、<stdint.h>
     * 64位Linux下实际大致是这样： typedef unsigned long size_t;
     * 平台	               size_t 实际等价于
     * 32 位系统	            unsigned int（4 字节）
     * 64 位Linux	        unsigned long（8 字节）
     * 64 位Windows MSVC    unsigned __int64
     * 类型	              格式符
     * size_t	          %zu,sizeof/offsetof 返回值
     * ssize_t(有符号)	  %zd,size，read/write 返回
     * uint64_t	          %"PRIu64"（需要<inttypes.h>）
     */
    printf("pthread_mutex_t size :%zu\n",sizeof(pthread_mutex_t));
    void* mem = malloc(sizeof(Block) * BLOCK_CNT);
    if (mem == NULL) return NULL;

    MemPool* pool = malloc(sizeof(MemPool));
    if (pool == NULL) {
        free(mem);
        return NULL;
    }
    pthread_mutex_init(&pool->lock, NULL);
    pool->start = mem;
    pool->free_list = NULL;

    Block* cur = (Block*)mem;
    for (int i = 0; i < BLOCK_CNT; i++) {
        cur->next = pool->free_list;
        pool->free_list = cur;
        cur++;
    }
    return pool;
}

void* block_alloc(MemPool* pool) {
    pthread_mutex_lock(&pool->lock);
    void* ret = NULL;
    if (pool->free_list != NULL) {
        Block* blk = pool->free_list;
        pool->free_list = blk->next;
        ret = blk->data;
    }
    pthread_mutex_unlock(&pool->lock);
    return ret;
}

void block_free(MemPool* pool, void* data_ptr) {
    pthread_mutex_lock(&pool->lock);
    Block* blk = (Block*)((char*)data_ptr - offsetof(Block, data));
    blk->next = pool->free_list;
    pool->free_list = blk;
    pthread_mutex_unlock(&pool->lock);
}

void mempool_destroy(MemPool* pool) {
    pthread_mutex_destroy(&pool->lock);
    free(pool->start);
    free(pool);
}

// 线程工作函数
void* worker(void* arg) {
    MemPool* pool = arg;
    for (int i = 0; i < ALLOC_CNT; i++) {
        void* p = block_alloc(pool);
        if (p) {
            block_free(pool, p);
        }
    }
    return NULL;
}

int main(void) {
    MemPool* pool = mempool_create();
    pthread_t threads[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, worker, pool);
    }
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
    }

    mempool_destroy(pool);
    printf("all done\n");
    return 0;
}
