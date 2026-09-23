#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#define BLOCK_SIZE 128
#define BLOCK_CNT 8

typedef struct Block {
    struct Block* next;
    int num;
    char data[BLOCK_SIZE];
} Block;

typedef struct MemPool {
    Block* free_list;
    void* start; // 整块内存起始地址，销毁用
} MemPool;

MemPool* mempool_create(void) {
    // 一次性申请一大块内存
    void* mem = malloc(sizeof(Block) * BLOCK_CNT);
    if (mem == NULL) return NULL;

    MemPool* pool = malloc(sizeof(MemPool));
    if (pool == NULL) {
        free(mem);
        return NULL;
    }
    pool->start = mem;
    pool->free_list = NULL;

    // 切分成Block，构建空闲链表
    Block* cur = (Block*)mem;
    for (int i = 0; i < BLOCK_CNT; i++) {
        cur->next = pool->free_list;
        cur->num = i;
        pool->free_list = cur;
        //cur++;
        //注：柔性数组不可用cur++；sizeof(Block)会不包含柔性数组的大小的
        cur = (Block*)((char*)cur + sizeof(Block));
    }
    return pool;
}

// 返回data指针给上层使用
void* block_alloc(MemPool* pool) {
    if (pool->free_list == NULL) {
        return NULL;
    }
    //从空闲链表找到第一个block
    Block* blk = pool->free_list;
    printf("block_alloc:alloc block %d\n", blk->num);
    //再让pool指向下一个block
    pool->free_list = blk->next;
    printf("block_alloc:the next block is :%d\n", pool->free_list->num);
    return blk->data;
}

void block_free(MemPool* pool, void* data_ptr) {
    // data指针反向找到Block头部
    Block* blk = (Block*)((char*)data_ptr - offsetof(Block, data));
    printf("block_free: free block %d\n", blk->num);

    //当前的pool->free_list是第三个block吧？
    blk->next = pool->free_list;
    printf("block_free:pool next block: %d\n",pool->free_list->num);
    //pool->free_list指向第一个block
    //那第二个block呢？从链上丢了？
    pool->free_list = blk;
    Block* blk2 = blk;
    while (blk2 != NULL) { //打印出空闲链表
        printf("block_free:free list block: %d\n", blk2->num);
        blk2 = blk2->next;
    }
}

void mempool_destroy(MemPool* pool) {
    free(pool->start); // 只释放整块内存！不逐个free Block
    free(pool);
}

int main(void) {
    MemPool* pool = mempool_create();

    //拿到第一个block的data
    void* p1 = block_alloc(pool);
    //拿到第二个block的data
    void* p2 = block_alloc(pool);
    printf("alloc p1=%p p2=%p\n", p1, p2);

    //把第一个block的data丢进去,反推block头部
    block_free(pool, p1);
    //这样又到了block的头部，拿到第一个block
    void* p3 = block_alloc(pool);
    printf("free p1 then alloc p3=%p\n", p3); // p3会复用p1地址

    mempool_destroy(pool);
    //此时p2虽未free，但整块内存已经free还给内存，p2为野指针了
    return 0;
}
