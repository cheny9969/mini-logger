/**
 * @file safe_publish_demo.c
 * @brief 演示：普通指针并发问题 + _Atomic原子指针 + acquire/release安全发布
 * 笔记级别注释，每一个坑点都写清楚
 * 编译：gcc -std=c11 -pthread -O2 safe_publish_demo.c -o safe_publish_demo
 * 一定要开 -O2 优化！不开优化，编译器不会乱序，bug复现不出来！
 *
 * 现象说明：
 * 1. 不使用_Atomic普通全局指针：存在【编译器指令重排】，消费者读到半初始化对象
 * 2. _Atomic指针，但误用memory_order_relaxed：原子性有，但缺少屏障，依然脏数据
 * 3. _Atomic + release/store  + acquire/load：标准安全发布范式
 *
 * 重要概念区分：
 * 原子性：读写变量本身不撕裂；
 * 内存序(屏障)：阻止编译器/CPU乱序，保证"业务数据先准备好，再发布指针"
 * 原子性 ≠ 内存序！x86 movq硬件原子，编译器依然可以重排代码！
 */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// 业务结构体：我们要在线程构造好，然后通过全局指针发布给另一个线程
typedef struct {
    int field_a;
    int field_b;
    int field_c;
} BusinessData;

// ===================== 测试1：普通指针（错误版本） =====================
// 普通全局指针，没有 _Atomic
static BusinessData* g_raw_ptr = NULL;

// 生产者线程：分配对象，填充字段，赋值给全局指针发布
void* thread_producer_raw(void* arg)
{
    (void)arg;
    BusinessData* data = malloc(sizeof(BusinessData));

    // step1：填充结构体业务字段
    data->field_a = 111;
    data->field_b = 222;
    data->field_c = 333;

    // 重点风险：编译器开启-O2后，可以重排！
    // 允许把 g_raw_ptr = data; 挪到上面三个赋值之前执行！
    // 也就是：先对外发布指针，后填充成员！
    g_raw_ptr = data;

    printf("[producer_raw] 对象已经对外发布\n");
    return NULL;
}

// 消费者线程：读取全局指针，如果不为NULL就读取结构体字段
void* thread_consumer_raw(void* arg)
{
    (void)arg;
    // 循环自旋，等待g_raw_ptr不为NULL
    while(g_raw_ptr == NULL)
    {
        ; // 空自旋，没有任何同步
    }

    // 走到这里，g_raw_ptr不为空
    // ⚠️ 危险！有可能：指针已经发布，但是结构体成员还没写完（编译器重排）
    BusinessData* p = g_raw_ptr;
    printf("[consumer_raw] get ptr, a=%d b=%d c=%d\n",
            p->field_a, p->field_b, p->field_c);

    // 注意：这里存在UB未定义行为！读到垃圾值，也可能正常，看编译器优化
    return NULL;
}

// ===================== 测试2：_Atomic指针，但是错误使用 relaxed =====================
// 原子指针：指针变量本身读写是原子的；但是内存序给relaxed
_Atomic (BusinessData *) g_atomic_relaxed_ptr = NULL;

void* thread_producer_relaxed(void* arg)
{
    (void)arg;
    BusinessData* data = malloc(sizeof(BusinessData));
    data->field_a = 1001;
    data->field_b = 1002;
    data->field_c = 1003;

    // memory_order_relaxed：只保证指针读写原子，【没有任何编译器屏障】
    // 编译器依然可以把data成员赋值重排到atomic_store之后！
    // 也就是：先对外发布指针，后填充成员！
    //⚠️ 这是不安全的！
    /*
     *
     * _explicit 给你选择权，可以传入 release/acquire/relaxed；
     * 不带 _explicit 赋值，全部硬编码成最重的 seq_cst；
     * 安全发布范式，必须用
     * atomic_store_explicit(..., memory_order_release)
     * atomic_load_explicit(..., memory_order_acquire)等
     */
    atomic_store_explicit(&g_atomic_relaxed_ptr, data, memory_order_relaxed);
    printf("[producer_relaxed] 对象发布 done\n");
    return NULL;
}

void* thread_consumer_relaxed(void* arg)
{
    (void)arg;
    BusinessData* p = NULL;
    while( (p = atomic_load_explicit(&g_atomic_relaxed_ptr, memory_order_relaxed)) == NULL )
    {
        ;
    }
    // load也是relaxed：没有acquire屏障
    // 就算读到非空指针，访问 p->xxx 依然可能看到旧垃圾
    printf("[consumer_relaxed] a=%d b=%d c=%d\n", p->field_a, p->field_b, p->field_c);
    return NULL;
}

// ===================== 测试3：标准正确范式 _Atomic + release / acquire =====================
/**
 * 范式：安全发布 safe‑publication
 * store端：atomic_store(..., memory_order_release)
 * load端：atomic_load(..., memory_order_acquire)
 *
 * release语义(写端)：
 *      在本store之前的所有内存读写，编译器不能重排到store指令之后
 *      → 保证结构体的field_a/b/c一定先写完，才执行发布指针
 * acquire语义(读端)：
 *      在本load之后的所有内存读写，不能重排到load指令之前
 *      → 保证访问p->field 一定发生在读到指针之后
 */
_Atomic (BusinessData *) g_atomic_correct_ptr = NULL;

void* thread_producer_correct(void* arg)
{
    (void)arg;
    BusinessData* data = malloc(sizeof(BusinessData));
    data->field_a = 9001;
    data->field_b = 9002;
    data->field_c = 9003;

    // ✅ release屏障：上面对data成员的写，绝对不会跑到store之后
    atomic_store_explicit(&g_atomic_correct_ptr, data, memory_order_release);
    printf("[producer_correct] 对象发布 done\n");
    return NULL;
}

void* thread_consumer_correct(void* arg)
{
    (void)arg;
    BusinessData* p = NULL;
    // ✅ acquire屏障，和release配对
    while( (p = atomic_load_explicit(&g_atomic_correct_ptr, memory_order_acquire)) == NULL )
    {
        ;
    }
    // 只要拿到p，就100%保证data三个字段已经完整初始化完毕
    printf("[consumer_correct] a=%d b=%d c=%d\n", p->field_a, p->field_b, p->field_c);
    return NULL;
}


int main(void)
{
    pthread_t t_prod, t_cons;

    printf("========= 测试1：普通原始指针（存在重排风险，UB未定义行为）=========\n");
    g_raw_ptr = NULL;
    pthread_create(&t_prod, NULL, thread_producer_raw, NULL);
    pthread_create(&t_cons, NULL, thread_consumer_raw, NULL);
    pthread_join(t_prod, NULL);
    pthread_join(t_cons, NULL);
    free(g_raw_ptr);


    printf("\n========= 测试2：_Atomic + relaxed（原子有，屏障缺失，依然风险）=========\n");
    atomic_store_explicit(&g_atomic_relaxed_ptr, NULL, memory_order_relaxed);
    pthread_create(&t_prod, NULL, thread_producer_relaxed, NULL);
    pthread_create(&t_cons, NULL, thread_consumer_relaxed, NULL);
    pthread_join(t_prod, NULL);
    pthread_join(t_cons, NULL);
    BusinessData* tmp2 = atomic_load_explicit(&g_atomic_relaxed_ptr, memory_order_relaxed);
    free(tmp2);


    printf("\n========= 测试3：正确范式 release / acquire 配对 =========\n");
    atomic_store_explicit(&g_atomic_correct_ptr, NULL, memory_order_release);
    pthread_create(&t_prod, NULL, thread_producer_correct, NULL);
    pthread_create(&t_cons, NULL, thread_consumer_correct, NULL);
    pthread_join(t_prod, NULL);
    pthread_join(t_cons, NULL);
    BusinessData* tmp3 = atomic_load_explicit(&g_atomic_correct_ptr, memory_order_acquire);
    free(tmp3);

    printf("\n笔记重点总结：\n");
    printf("1. _Atomic(Data *) 保护【指针变量本身】，**不会保护指针指向的结构体内容**\n");
    printf("2. relaxed仅保证原子性，不阻止重排；发布对象必须release/acquire配对\n");
    printf("3. -O2优化一定要开，不开优化编译器不会重排，bug隐藏，测试不出问题\n");
    printf("4. x86硬件TSO很强，很多时候复现不出重排bug；ARM平台100%%暴露该问题\n");
    printf("5. 拿到原子load返回的指针p之后，p->field并发读写仍然需要锁/原子，这里只是保证初始化完成\n");

    return 0;
}
