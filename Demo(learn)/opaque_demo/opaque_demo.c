#include "opaque_demo.h"
#include <stdlib.h>

struct MyObj {
    int data;
    // 以后可以加 _Atomic 变量，cpp完全看不到
};

MyObj* obj_create(int val) {
    MyObj* p = malloc(sizeof(MyObj));
    p->data = val;
    return p;
}
void obj_set(MyObj* obj, int v) { obj->data = v; }
int obj_get(MyObj* obj) { return obj->data; }
void obj_destroy(MyObj* obj) { free(obj); }
