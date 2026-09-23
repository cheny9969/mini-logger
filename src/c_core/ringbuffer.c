#include "ringbuffer.h"
#include <stdlib.h>
#include <stdatomic.h>


struct RingBuffer {
    _Atomic uint32_t head;  // 生产者写入位置，多生产者修改
    _Atomic uint32_t tail;  // 消费者读取位置，仅单消费者修改
    Block* buf[RING_CAPACITY];
};


RingBuffer* ringbuf_create(void)
{
    RingBuffer* rb = (RingBuffer*)malloc(sizeof(RingBuffer));
    if(rb == NULL)
        return NULL;

    // TODO: atomic_store_explicit 初始化 head、tail = 0，memory_order_relaxed

    return rb;
}


void ringbuf_destroy(RingBuffer* rb)
{
    if(rb != NULL)
    {
        // TODO: 只free rb本体，Block归属于mempool，不能释放Block
        free(rb);
    }
}

// 多生产者无锁入队，返回0成功；队列满返回‑1
int ringbuf_enqueue(RingBuffer* rb, Block* blk)
{
    if(rb == NULL || blk == NULL)
        return -1;

    // TODO: for(;;)自旋循环
    //  1. acquire load head / tail
    //  2. 计算next_head，判断队列是否已满，满直接return -1
    //  3. atomic_compare_exchange_weak_explicit CAS尝试更新head
    //  4. CAS成功：写入buf[head]=blk，return 0
    //  5. CAS失败，进入下一轮循环

    return -1;
}

// 单消费者出队；队空返回NULL
Block* ringbuf_dequeue(RingBuffer* rb)
{
    if(rb == NULL)
        return NULL;

    Block* blk = NULL;
    // TODO:
    // 1. acquire load tail、head
    // 2. tail == head 代表队空，return NULL
    // 3. 取出 buf[tail]，可置空buf[tail]用于调试
    // 4. 计算 next_tail
    // 5. release store 更新 rb->tail = next_tail
    // 6. 返回 blk

    return blk;
}
