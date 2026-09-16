#include "mempool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

MemPool* mempool_create(void)
{
    // TODO: 1. 计算总内存大小
    // TODO: 2. malloc一块连续内存
    // TODO: 3. 把所有Block串成free_list链表
    // 返回 pool
}

void mempool_destroy(MemPool* pool)
{
    // TODO: 释放整块内存
}

void* block_alloc(MemPool* pool)
{
    // TODO: 从free_list取一块，返回block->data
}

void block_free(MemPool* pool, void* ptr)
{
    // TODO: 将block放回free_list头部
}
