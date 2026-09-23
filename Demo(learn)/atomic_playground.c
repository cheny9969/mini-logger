/*
 * ============================================================================
 *  atomic_playground.c  ——  C11 <stdatomic.h> 一站式学习 Demo
 * ============================================================================
 *
 *
 * GCC 没有把`atomic_load`做成独立库函数，全部通过**`__atomic_*`系列 builtin**实现。
 * builtin 是编译器的扩展，不属于 C 标准，是编译器内置的 “魔法函数”。
 * builtin 不是放在 .so 里的函数，编译器在编译期直接把 builtin 调用翻译成汇编。
 * atomic_store(&global_ptr, d, memory_order_release)
 * -> __atomic_store_n(&global_ptr, d, memory_order_release)
 * builtin（built-in function，内置函数）：
 * 是 GCC/Clang**编译器内部硬编码识别的特殊函数名**。
 * 遇到 builtin，编译器**不生成 call 指令，直接翻译成对应的汇编**。
 * 不是动态库函数，没有函数实体，编译期展开。
 * __builtin_popcount、__atomic_load_n 全都是 builtin。

>
> 区分：
>
>
> - 普通函数：call xxx，跳到内存里的函数代码；
> - builtin：编译器看到名字，直接替换成一串汇编，无 call。
 *  编译:
 *      gcc -std=c11 -pthread atomic_playground.c -o atomic_playground
 *  运行:
 *      ./atomic_playground
 *
 *  覆盖知识点（按出现顺序）:
 *    [0] atomic(T) 类型语法到底怎么读   —— 重点拆解 atomic(Data *)
 *    [1] atomic_init / atomic_load / atomic_store
 *    [2] 五种 memory_order: relaxed / acquire / release / acq_rel / seq_cst
 *    [3] acquire/release 配对做"对象安全发布"
 *    [4] atomic_fetch_add / fetch_sub / fetch_or   (特化 RMW)
 *    [5] atomic_compare_exchange_weak vs strong, 虚假失败
 *    [6] 失败时 expected 参数会被改写（新手头号坑）
 *    [7] ABA 问题与 TaggedPtr(指针+版本号) DCAS 解法
 *    [8] 无锁栈 push / pop 完整实现
 *    [9] 自旋等待 (spin-wait) 一个标志位
 *   [10] 对比: 用互斥锁实现同一个栈, 直观感受无锁代码长什么样
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

/* ===========================================================================
 * [0] 先把你之前卡在的那行语法彻底讲清楚
 * ===========================================================================
 *
 *   atomic(Data *) global_ptr = NULL;
 *
 *  stdatomic.h 里定义了一个类型泛型宏:
 *      #define atomic(T)  _Atomic (T)
 *
 *  注意 T 外面那对括号! 这是 C11 的第二种 _Atomic 语法 —— "类型构造器":
 *      _Atomic (类型名)   →  把整个类型名包装成"原子版本"
 *
 *  所以:
 *      atomic(Data *)   →   _Atomic (Data *)
 *
 *  读成中文: "指向 struct Data 的指针, 本身是原子的"。
 *
 *  ⚠️ 最容易搞混的地方:
 *      _Atomic Data *p;     // 没有括号! 这是 _Atomic 限定符形式,
 *                           // 等价于 const Data *p 的读法:
 *                           // "p 是一个指针, 指向 _Atomic(原子的) Data"
 *                           // —— 即指针本身不原子, 指向的对象才原子
 *
 *      _Atomic (Data *) p; // 有括号! "p 是一个原子指针, 指向普通 Data"
 *                           // —— 这才是我们想要的
 *
 *  所以永远用 atomic(T) 宏, 别手写 _Atomic 限定符形式, 否则会读错。
 *
 *  另外 stdatomic.h 还预置了一批原子类型别名, 不用自己写 atomic():
 *      atomic_int          atomic_uint
 *      atomic_long         atomic_ulong
 *      atomic_size_t
 *      atomic_int64_t      atomic_uint64_t
 *      atomic_bool
 *      ...
 *  这些在 [4] 里会用到。
 * =========================================================================== */

/* 业务节点: 无锁栈里装的东西 */
typedef struct Node {
    int           value;
    struct Node   *next;
} Node;

/* ===========================================================================
 * [1] 全局原子变量的两种初始化方式
 * ===========================================================================
 *
 *  方式 A: 编译期初始化 (只对标量类型方便)
 *      atomic_int           g_cnt = 0;
 *      atomic_uintptr_t g_ptr = 0;
 *
 *  方式 B: 运行期 atomic_init (对非平凡类型, 比如 _Atomic(TaggedPtr))
 *      TaggedPtr init = { .ptr = NULL, .ver = 0 };
 *      atomic_init(&g_head, init);
 *
 *  ⚠️ 不能直接写:
 *      atomic(TaggedPtr) g_head = { NULL, 0 };   // ❌ 编译错误, 原子对象不能用普通初始化器
 * =========================================================================== */

/* ===========================================================================
 * [2][3] acquire / release 配对: "对象安全发布"
 * ===========================================================================
 *
 *  场景: 一个线程负责 malloc + 填好一块 Data, 然后把指针"发布"出去;
 *        另一个线程读到指针后, 才能安全地访问它指向的内容。
 *
 *  为什么不能直接用普通指针赋值?
 *      编译器 / CPU 允许指令重排:
 *        线程1:  store global_ptr = data;        // 发布指针
 *                store data->a = 100;           // 填内容
 *                store data->b = 200;
 *        线程2:  load p = global_ptr;
 *                load p->a;   // 💥 可能读到 0, 因为 "填内容" 被重排到 "发布指针" 之后了
 *
 *  release / acquire 这一对内存屏障就是用来堵这个洞的:
 *
 *    线程1 (生产者)                     线程2 (消费者)
 *    -----------                       ------------
 *    data->a = 100;   ┐
 *    data->b = 200;   │ release:
 *                     │ 保证 store 之前的所有写, 不会跑到 store 后面
 *    store(global_ptr, data,           load p = load(global_ptr,
 *          memory_order_release);   ───►       memory_order_acquire);
 *                                                │ acquire:
 *                                                │ 保证 load 之后的所有读,
 *                                                │ 不会跑到 load 前面
 *                                                ▼
 *                                          p->a, p->b   (一定是 100, 200)
 * =========================================================================== */

typedef struct {
    int  a;
    int  b;
} Data;

/* 原子指针: 指向普通 Data, 指针本身原子
 *
 * ⚠️ GCC/Clang 实测注意:
 *   stdatomic.h 里的 atomic 宏通常就是 #define atomic _Atomic,
 *   那么 atomic(Data *) 展开成 _Atomic Data * —— 这是"指向原子Data的指针",
 *   不是我们要的"原子指针"!
 *
 *   想要"指针本身原子", 必须用带括号的 _Atomic 类型构造器形式:
 *       _Atomic (Data *)
 *   这正是 C11 标准里 atomic(T) 宏"应当"展开成的样子, 但不同编译器实现有差异,
 *   所以写指针类型时直接用 _Atomic (T) 最稳。
 */
_Atomic (Data *)  g_published_ptr = NULL;

void producer_fill_and_publish(void)
{
    Data *d = malloc(sizeof(Data));
    d->a = 111;
    d->b = 222;

    /* [3] release: 上面两句普通写, 一个都不许跑到这一行后面 */
    //atomic_store 和 atomic_load 默认 memory_order_seq_cst
    atomic_store_explicit(&g_published_ptr, d, memory_order_release);

    printf("[producer] 已发布数据, ptr=%p (a=%d b=%d)\n",
           (void *)d, d->a, d->b);
}

void consumer_read_published(void)
{
    /* [3] acquire: 这一行 load 之后再去碰 *p, 不许提前 */
    Data *p = atomic_load_explicit(&g_published_ptr, memory_order_acquire);

    if (p) {
        printf("[consumer] 读到 ptr=%p, a=%d b=%d\n", (void *)p, p->a, p->b);
    } else {
        printf("[consumer] 还没发布\n");
    }
}

/* ===========================================================================
 * [4] atomic_fetch_add / fetch_sub / fetch_or  —— 特化 RMW 原语
 * ===========================================================================
 *
 *  语义:
 *      old = *obj;
 *      *obj = old + arg;
 *      return old;        // 返回的是"修改之前"的旧值!
 *
 *  跟 CAS 的关系:
 *      它底层可能就是一条硬件指令 (x86: lock add), 也可能内部用 CAS 循环 (ARM)。
 *      但对你来说, 一行就搞定, 不用自己写 do-while。
 *
 *  什么时候用 relaxed?
 *      如果这个计数器只是个"统计数字", 它的值不依赖任何其他内存,
 *      也没有别的线程要"看到计数器等于 N 时就说明某段数据准备好了",
 *      那就用 memory_order_relaxed —— 只保证原子, 不做任何屏障, 最快。
 * =========================================================================== */

/* 纯计数, relaxed 足够 */
/* 注: GCC 在 -std=c11 下没有预定义 atomic_uint64_t 别名,
 *     直接用 _Atomic uint64_t 即可 (等价写法) */
_Atomic uint64_t  g_push_cnt = 0;
_Atomic uint64_t  g_pop_cnt  = 0;
atomic_ulong      g_flag_bits = 0;
_Atomic(uint64_t)  g_demo_cnt  = 0;   /* [4] 自己用, 不污染栈统计 */

void demo_fetch_ops(void)
{
    /* 纯计数, relaxed 足够; 用独立变量, 别污染栈的 push 统计 */
    uint64_t old = atomic_fetch_add_explicit(&g_demo_cnt, 1, memory_order_relaxed);
    printf("[fetch] g_demo_cnt 旧值=%lu, 新值=%lu\n",
           (unsigned long)old,
           (unsigned long)atomic_load_explicit(&g_demo_cnt, memory_order_relaxed));

    /* 位标志: 把第 3 位置 1 */
    atomic_fetch_or_explicit(&g_flag_bits, 1UL << 3, memory_order_relaxed);
    printf("[fetch] g_flag_bits = 0x%lx (bit3 已置位)\n",
           (unsigned long)atomic_load_explicit(&g_flag_bits, memory_order_relaxed));
}

/* ===========================================================================
 * [5][6][7][8] 无锁栈: 重点中的重点
 * ===========================================================================
 *
 *  为什么需要 TaggedPtr? —— ABA 问题
 *  ----------------------------------
 *  假设栈头是一个普通原子指针 head:
 *
 *    线程1:  old = head;                 // old == A
 *            new->next = old;
 *                                       // 此时线程2 飞速跑完:
 *                                       //   pop A
 *                                       //   pop B
 *                                       //   push A' (malloc 复用了 A 的地址!)
 *            CAS(head, A, new);          // head 现在确实等于 A
 *                                        // 但 A->next 已经不是原来那个节点了!
 *                                        // 链表结构被破坏, 💥
 *
 *  根因: CAS 只比较"指针值", 不看"它指向的东西有没有被改过"。
 *
 *  解法: 把 (指针, 版本号) 打包成一个结构体, 一起 CAS:
 *        就算地址回到 A, 版本号已经 +N, CAS 必然失败。
 *
 *  这叫 DCAS (Double-Word CAS), x86_64 需要 cmpxchg16b (现代 CPU 都支持)。
 * =========================================================================== */

typedef struct {
    Node     *ptr;
    uint64_t  ver;      // 每次改动 +1
} TaggedPtr;

/* [7] 原子的 TaggedPtr —— 同样用 _Atomic (T) 带括号形式
 *     必须用 atomic_init 运行期初始化, 不能编译期直接 = {…} */
_Atomic (TaggedPtr)  g_stack_head;

/*
 * [5] atomic_compare_exchange_weak 到底是什么?
 * --------------------------------------------
 *  函数原型 (简化):
 *      bool atomic_compare_exchange_weak(
 *              Atomic(T) *obj,       // 要操作的原子变量地址
 *              T         *expected,  // 你"期待"它现在是什么 (注意是指针!)
 *              T          desired,    // 如果等于 expected, 就改成这个
 *              memory_order success, // CAS 成功时用的内存序
 *              memory_order failure  // CAS 失败时用的内存序 (只做读, 用 acquire)
 *          );
 *
 *  返回值:
 *      true  → *obj 本来就等于 *expected, 已经原子地改成 desired
 *      false → *obj 不等于 *expected
 *
 * [6] 新手头号坑: 失败时, *expected 会被改写!
 *      失败返回的瞬间, 你传进去的 expected 变量, 已经被自动填充成
 *      *obj 的当前真实值。所以 do-while 循环里不用重新 load, 直接用就行。
 *
 * [5] weak vs strong:
 *      weak 允许"虚假失败" —— 值明明相等, 也返回 false (ARM/PowerPC 可能发生)
 *      strong 不会虚假失败。
 *
 *      规则:
 *        - 已经在 do-while 循环里   → 用 weak (更快, 虚假失败就再转一圈)
 *        - 只尝试一次, 不循环       → 必须用 strong (否则虚假失败会让你逻辑错乱)
 * =========================================================================== */

void lockless_stack_push(int val)
{
    Node *n = malloc(sizeof(Node));
    n->value = val;

    TaggedPtr old_tp;
    TaggedPtr new_tp;

    do {
        /* [8] 读栈头: 只做读, acquire 足够 */
        old_tp = atomic_load_explicit(&g_stack_head, memory_order_acquire);

        n->next = old_tp.ptr;

        new_tp.ptr = n;
        new_tp.ver = old_tp.ver + 1;     /* 版本号永远 +1 */

        /*
         * [5] weak 必须在循环里。
         * 成功: 读+写, 用 acq_rel;
         * 失败: 只做了一次读比较, 用 acquire;
         *       并且失败时 old_tp 已经被写成最新值, 下一轮直接用。
         */
    } while (!atomic_compare_exchange_weak_explicit(
                &g_stack_head,
                &old_tp,
                new_tp,
                memory_order_acq_rel,
                memory_order_acquire));

    atomic_fetch_add_explicit(&g_push_cnt, 1, memory_order_relaxed);
}

Node *lockless_stack_pop(void)
{
    TaggedPtr old_tp;
    TaggedPtr new_tp;

    do {
        old_tp = atomic_load_explicit(&g_stack_head, memory_order_acquire);

        if (old_tp.ptr == NULL) {
            return NULL;                /* 栈空 */
        }

        new_tp.ptr = old_tp.ptr->next;
        new_tp.ver = old_tp.ver + 1;

    } while (!atomic_compare_exchange_weak_explicit(
                &g_stack_head,
                &old_tp,
                new_tp,
                memory_order_acq_rel,
                memory_order_acquire));

    atomic_fetch_add_explicit(&g_pop_cnt, 1, memory_order_relaxed);
    return old_tp.ptr;
}

/* ===========================================================================
 * [9] 自旋等待 (spin-wait): 等一个原子标志位
 * ===========================================================================
 *
 *  常见场景: 主线程做初始化, 子线程就绪后把 ready 置 1,
 *           主线程原地自旋等 ready 变 1。
 *
 *  注意: 必须用 acquire 读, 否则可能读到 ready==1 但它之前写的数据还没准备好。
 * =========================================================================== */

atomic_bool  g_ready = ATOMIC_VAR_INIT(false);

void *worker_thread(void *arg)
{
    (void)arg;
    sleep(1);                          /* 假装做初始化 */

    /* release: 把 sleep 期间准备好的东西一起"发布"出去 */
    atomic_store_explicit(&g_ready, true, memory_order_release);
    return NULL;
}

void demo_spin_wait(void)
{
    pthread_t th;
    pthread_create(&th, NULL, worker_thread, NULL);

    printf("[spin] 开始等 g_ready...\n");

    /* acquire 读; 失败就继续转 */
    while (!atomic_load_explicit(&g_ready, memory_order_acquire)) {
        /*
         * 生产代码里这里会加:
         *   - sched_yield()   让出 CPU
         *   - 或 _mm_pause() / __asm__("yield")  防止自旋空转耗电
         * 这里为了 demo 简洁, 直接忙等。
         */
    }

    printf("[spin] g_ready == true, 继续往下走\n");
    pthread_join(th, NULL);
}

/* ===========================================================================
 * [10] 对比版: 同一根栈, 用互斥锁写一遍
 * ===========================================================================
 *
 *  肉眼对比: 无锁版本你要操心 acquire/release/ABA/版本号;
 *            锁版本只要 pthread_mutex_lock 一下, 编译器/库帮你做了所有屏障。
 *
 *  什么时候用锁, 什么时候用无锁?
 *    - 锁竞争激烈 (很多线程抢同一把锁) → 用无锁, 减少内核态切换
 *    - 锁竞争几乎为零 → 直接用锁, 代码简单可靠
 *    - 无锁最大的坑不是性能, 是 ABA / 内存回收 (hazard pointer / EBR),
 *      那是另一门大课, 本 demo 不展开。
 * =========================================================================== */

pthread_mutex_t  g_mu    = PTHREAD_MUTEX_INITIALIZER;
Node           *g_locked_head = NULL;

void locked_stack_push(int val)
{
    Node *n = malloc(sizeof(Node));
    n->value = val;

    pthread_mutex_lock(&g_mu);
    n->next = g_locked_head;
    g_locked_head = n;
    pthread_mutex_unlock(&g_mu);
}

Node *locked_stack_pop(void)
{
    pthread_mutex_lock(&g_mu);
    Node *n = g_locked_head;
    if (n) {
        g_locked_head = n->next;
    }
    pthread_mutex_unlock(&g_mu);
    return n;
}

/* ===========================================================================
 *  测试线程: 疯狂往无锁栈里塞东西
 * =========================================================================== */

#define THREAD_NUM        4
#define PUSH_PER_THREAD   200

void *hammer_thread(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < PUSH_PER_THREAD; i++) {
        lockless_stack_push((int)(id * 10000 + i));
    }
    return NULL;
}

/* ===========================================================================
 *  main
 * =========================================================================== */

int main(void)
{
    /* 初始化 TaggedPtr 原子栈头 */
    TaggedPtr init = { .ptr = NULL, .ver = 0 };
    atomic_init(&g_stack_head, init);

    printf("========== [3] acquire/release 对象发布 ==========\n");
    consumer_read_published();          /* 还没发布 */
    producer_fill_and_publish();
    consumer_read_published();
    free((void *)atomic_load_explicit(&g_published_ptr, memory_order_acquire));
    atomic_store_explicit(&g_published_ptr, NULL, memory_order_release);

    printf("\n========== [4] fetch_add / fetch_or ==========\n");
    demo_fetch_ops();

    printf("\n========== [9] 自旋等待 ==========\n");
    demo_spin_wait();

    printf("\n========== [5-8] 多线程打无锁栈 ==========\n");
    pthread_t th[THREAD_NUM];
    for (long i = 0; i < THREAD_NUM; i++) {
        pthread_create(&th[i], NULL, hammer_thread, (void *)i);
    }
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(th[i], NULL);
    }

    unsigned long push_n = atomic_load_explicit(&g_push_cnt, memory_order_relaxed);
    printf("push 总数 = %lu (预期 %d)\n",
           push_n, THREAD_NUM * PUSH_PER_THREAD);

    /* 全部弹出来 free 掉 */
    Node *n;
    int popped = 0;
    while ((n = lockless_stack_pop()) != NULL) {
        free(n);
        popped++;
    }
    printf("pop 总数 = %d (预期 %d)\n", popped, THREAD_NUM * PUSH_PER_THREAD);
    printf("pop_cnt = %lu\n",
           (unsigned long)atomic_load_explicit(&g_pop_cnt, memory_order_relaxed));

    printf("\n========== [10] 锁版本对比 (只测 3 次 push/pop) ==========\n");
    locked_stack_push(1);
    locked_stack_push(2);
    locked_stack_push(3);
    while ((n = locked_stack_pop())) {
        printf("  locked pop -> %d\n", n->value);
        free(n);
    }

    printf("\n全部完成 ✅\n");
    return 0;
}
