#include "mempool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>


MemPool* mempool_create(void)
{
    MemPool* pool = NULL;
    // TODO 1: malloc MemPool实例，判空

    // TODO 2: 计算整块内存大小，malloc连续内存给pool->start，判空失败要回滚释放pool

    // TODO 3: 初始化pool各个成员 total_size / block_count / free_list = NULL

    // TODO 4: pthread_mutex_init 初始化互斥锁，失败要做资源回滚

    // TODO 5: 循环把所有Block串进free_list空闲链表

    return pool;
}


void mempool_destroy(MemPool* pool)
{
    if(pool == NULL)
        return;

    // TODO 1: pthread_mutex_destroy销毁锁
    // TODO 2: free大块内存 pool->start
    // TODO 3: free pool本体
}


void* block_alloc(MemPool* pool)
{
    if(pool == NULL)
        return NULL;

    // TODO 1: pthread_mutex_lock 加锁

    void* ret = NULL;
    // TODO 2: 如果free_list不为空，摘下链表头Block；返回 &blk->data；否则返回NULL

    // TODO 3: pthread_mutex_unlock 解锁

    return ret;
}


void block_free(MemPool* pool, void* ptr)
{
    if(pool == NULL || ptr == NULL)
        return;

    // TODO 1: pthread_mutex_lock 加锁

    // TODO 2: offsetof 从data指针回退得到Block*
    // TODO 3: 简单校验Block地址是否属于本内存池范围
    // TODO 4: 将block插入free_list链表头部

    // TODO 5: pthread_mutex_unlock 解锁
}
