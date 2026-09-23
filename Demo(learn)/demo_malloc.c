#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // 向libc申请堆内存
    char* buf = malloc(128);
    if (buf == NULL) {
        perror("malloc fail");
        return 1;
    }

    buf[0] = 'H';
    buf[1] = 'i';
    buf[2] = '\0';
    printf("%s\n", buf);

    free(buf);
    // buf现在是野指针！下面这行是未定义行为，不要启用
    // buf[0] = 'x';

    return 0;
}
