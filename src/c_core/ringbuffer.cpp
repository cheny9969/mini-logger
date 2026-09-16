#include "ringbuffer.h"
#include <stdlib.h>

RingBuffer* ringbuf_create(void)
{
    // TODO: malloc RingBuffer，head/tail初始化为0
}

void ringbuf_destroy(RingBuffer* rb)
{
    // TODO: free ringbuffer
}

int ringbuf_enqueue(RingBuffer* rb, Block* blk)
{
    // TODO: 无锁入队，多生产者
    // 注意 memory order
}

Block* ringbuf_dequeue(RingBuffer* rb)
{
    // TODO: 无锁出队，单消费者
}
