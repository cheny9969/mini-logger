#ifndef OPAQUE_DEMO_H
#define OPAQUE_DEMO_H

// 前向声明，不暴露结构体内容
typedef struct MyObj MyObj;

MyObj* obj_create(int val);
void obj_set(MyObj* obj, int v);
int obj_get(MyObj* obj);
void obj_destroy(MyObj* obj);

#endif
