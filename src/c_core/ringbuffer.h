#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdatomic.h>
#include <stdint.h>
#include "mempool.h"

#define RING_CAPACITY 64

typedef struct RingBuffer {
    _Atomic uint32_t head;  // 生产者写入位置
    _Atomic uint32_t tail;  // 消费者读取位置
    Block* buf[RING_CAPACITY];
} RingBuffer;

RingBuffer* ringbuf_create(void);
void ringbuf_destroy(RingBuffer* rb);

// 多生产者入队；成功返回0，队列满返回-1
int ringbuf_enqueue(RingBuffer* rb, Block* blk);
// 单消费者出队；空返回NULL
Block* ringbuf_dequeue(RingBuffer* rb);

#endif
