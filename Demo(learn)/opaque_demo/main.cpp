#include <iostream>
#include "opaque_demo.h"

int main() {
    MyObj* p = obj_create(100);
    std::cout << obj_get(p) << std::endl;
    obj_set(p, 200);
    std::cout << obj_get(p) << std::endl;
    obj_destroy(p);
    return 0;
}
