#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include "mempool.h"

#define RING_CAPACITY 64

// 前向声明：结构体对外部完全隐藏，外部只能操作指针
typedef struct RingBuffer RingBuffer;

RingBuffer* ringbuf_create(void);
void ringbuf_destroy(RingBuffer* rb);

// 多生产者入队；成功返回0，队列满返回-1
int ringbuf_enqueue(RingBuffer* rb, Block* blk);
// 单消费者出队；空返回NULL
Block* ringbuf_dequeue(RingBuffer* rb);

#endif
