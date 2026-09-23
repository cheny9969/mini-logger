// test_rb.c
// MPSC 多生产者‑单消费者 无锁环形队列 Demo
// 编译: gcc test_rb.c -o test_rb -pthread -std=c11
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#define RING_CAPACITY 8

typedef struct Block {
    int value;
} Block;

typedef struct RingBuffer {
    _Atomic unsigned int head;   // 生产者写索引，多线程修改
    _Atomic unsigned int tail;   // 消费者读索引，仅单线程修改
    Block* buf[RING_CAPACITY];
} RingBuffer;

static inline unsigned int ring_mod(unsigned int v)
{
    return v % RING_CAPACITY;
}

RingBuffer* ringbuf_create(void)
{
    RingBuffer* rb = malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    atomic_init(&rb->head, 0U);
    atomic_init(&rb->tail, 0U);
    for (int i = 0; i < RING_CAPACITY; i++) {
        rb->buf[i] = NULL;
    }
    return rb;
}

void ringbuf_destroy(RingBuffer* rb)
{
    if (rb) free(rb);
}

// 多生产者入队；成功0，满返回‑1
int ringbuf_enqueue(RingBuffer* rb, Block* blk)
{
    unsigned int head, next_head;
    unsigned int tail;

    for (;;) {
        head = atomic_load_explicit(&rb->head, memory_order_relaxed);
        next_head = ring_mod(head + 1);

        tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
        if (next_head == tail) {
            // 队列满
            return -1;
        }

        if (atomic_compare_exchange_weak_explicit(
                &rb->head, &head, next_head,
                memory_order_release, memory_order_relaxed))
        {
            rb->buf[head] = blk;
            return 0;
        }
        // CAS失败，重试
    }
}

// 单消费者出队；空返回NULL
Block* ringbuf_dequeue(RingBuffer* rb)
{
    unsigned int head = atomic_load_explicit(&rb->head, memory_order_acquire);
    unsigned int tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    if (head == tail) {
        return NULL;
    }

    Block* blk = rb->buf[tail];
    rb->buf[tail] = NULL;
    unsigned int next_tail = ring_mod(tail + 1);
    atomic_store_explicit(&rb->tail, next_tail, memory_order_release);
    return blk;
}

// -------- 测试 --------
#define PRODUCER_THREADS 3
#define PER_PRODUCE_CNT 20

RingBuffer* g_rb;

void* producer_fn(void* arg)
{
    int tid = *(int*)arg;
    free(arg);

    for (int i = 0; i < PER_PRODUCE_CNT; i++) {
        Block* b = malloc(sizeof(Block));
        b->value = tid * 1000 + i;

        while (ringbuf_enqueue(g_rb, b) != 0) {
            // 队列满，短暂自旋
            usleep(1000);
        }
        printf("[P%d] produce %d\n", tid, b->value);
    }
    return NULL;
}

int main(void)
{
    g_rb = ringbuf_create();

    pthread_t pth[PRODUCER_THREADS];
    for (int i = 0; i < PRODUCER_THREADS; i++) {
        int* tid = malloc(sizeof(int));
        *tid = i;
        pthread_create(&pth[i], NULL, producer_fn, tid);
    }

    // 单消费者主线程
    int recv_cnt = 0;
    int total = PRODUCER_THREADS * PER_PRODUCE_CNT;
    while (recv_cnt < total) {
        Block* b = ringbuf_dequeue(g_rb);
        if (b != NULL) {
            printf("[C] consume %d\n", b->value);
            free(b);
            recv_cnt++;
        } else {
            usleep(500);
        }
    }

    for (int i = 0; i < PRODUCER_THREADS; i++) {
        pthread_join(pth[i], NULL);
    }

    ringbuf_destroy(g_rb);
    printf("done, total=%d\n", recv_cnt);
    return 0;
}
