#include <stdio.h>
#include <stddef.h>

typedef struct Block {
    struct Block* next;
    char data[128];
} Block;

int main(void) {
    printf("offsetof(Block, next) = %zu\n", offsetof(Block, next));
    printf("offsetof(Block, data) = %zu\n", offsetof(Block, data));

    Block blk;
    char* data_ptr = blk.data;

    // data指针反向找回Block首地址，就是内存池block_free要用的逻辑
    Block* p = (Block*)((char*)data_ptr - offsetof(Block, data));
    printf("blk addr = %p, p addr = %p\n", &blk, p);

    return 0;
}
