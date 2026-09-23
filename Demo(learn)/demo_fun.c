#include <stdio.h>
#include <pthread.h>

// ========== 函数实体（存放在代码段 .text，有真正的机器指令） ==========

/**
 * @brief 普通函数：返回值为void，旧式C语法声明 void foo()
 * 注意C语言中 void foo() 不等于没有参数！代表参数未做限定，可以传入任意参数
 * C语言想要严格无参数，应该写成 void foo(void);
 */
void foo()
{
    printf("run foo\n");
}

/**
 * 1. 线程入口签名强制：`void* func(void* arg)`，仅**一个`void*`形参**，x86‑64 通过`rdi`寄存器传递该指针值。
 * 2. 寄存器只传**指针（地址）**，**不传数据本身**；多参数用结构体打包，数据存内存，线程凭指针访问内存。
 * 3. 内存二选一：
 *  - 堆`malloc`：推荐，生命周期可控；子线程 free / 主线程`pthread_join`后 free。
 *  - 主线程栈：禁止，函数退出栈销毁，野指针 UB。
 * 4. 不利用其余通用寄存器夹带数据：pthread 跳板会覆盖寄存器，ABI 不支持。
 * 5. 全局变量可共享数据，但多线程会互相覆盖，不适合批量创建线程。
 * 6. 禁止修改入口原型，业务类型在函数内部强转，原型不匹配属于 UB。
 */

/**
 * @brief pthread标准线程入口函数
 * 返回值必须是 void*，接收唯一一个 void* 类型参数
 * 这个返回的void*后续可以被 pthread_join 获取到
 * @param arg 由 pthread_create 第四个参数传递进来的指针
 * @return void* 线程的返回值
 */
void* bar(void* arg)
{
    // 打印接收到的参数地址
    printf("run bar, arg=%p\n", arg);
    // 返回一个指针值，线程return等价于线程退出，返回值交给pthread库保存
    return (void*)0x8888;
}

int main(void)
{
    // -------------------------- 函数指针变量定义 --------------------------
    // fp1 是函数指针变量：指向 返回值为void、参数不限定 的函数
    // 把foo函数名赋值给fp1：C语言中函数名会隐式转换成该函数的入口地址（.text段虚拟地址）
    void (*fp1)() = foo;

    // fp2 是函数指针变量，正是pthread_create第三个参数需要的类型
    // 指向：返回void*，接收一个void*参数的函数
    // 将bar函数的机器码首地址保存到变量fp2里面
    void* (*fp2)(void*) = bar;


    // -------------------------- 通过函数指针间接调用函数 --------------------------
    fp1();          // 间接调用foo，汇编：call 寄存器；等价直接调用 foo();

    // 使用fp2调用bar，传入参数0x1234；接收函数返回的void*
    void* ret = fp2((void*)0x1234);
    printf("bar返回值: %p\n", ret);


    // -------------------------- 打印地址，观察函数指针存的内容 --------------------------
    // foo 函数名，隐式转为函数入口地址
    printf("foo 函数入口地址:%p\n", foo);
    // bar 函数名，隐式转为函数入口地址
    printf("bar 函数入口地址:%p\n", bar);

    // fp1变量里面存的值 = foo的机器码起始虚拟地址
    printf("fp1变量存储的值:%p\n", fp1);
    // fp2变量里面存的值 = bar的机器码起始虚拟地址
    printf("fp2变量存储的值:%p\n", fp2);


    // -------------------------- pthread_create 实际使用场景 --------------------------
    pthread_t t;         // t用来输出保存新建线程ID

    /**
     * pthread_create 参数解析
     * 第1个参数 &t：输出，把新线程ID写到变量t中
     * 第2个参数 NULL：线程属性，NULL代表使用系统默认属性(joinable)
     * 第3个参数 bar：传入线程入口函数指针，类型必须为 void*(*)(void*)
     *      bar不带括号！带括号 bar() 代表直接执行函数，传入函数返回值，是严重错误
     * 第4个参数 (void*)0x5678：传递给线程入口bar的arg参数，仅仅拷贝指针本身，不会拷贝指向的内存
     */
    pthread_create(&t, NULL, bar, (void*)0x5678);

    /**
     * pthread_join(&t, &ret)
     * 阻塞等待线程t执行完毕；主线程停在这里，不再向下执行
     * &ret：接收线程return/pthread_exit返回的void*指针
     * 如果填NULL代表丢弃线程返回值
     */
    pthread_join(t, &ret);

    // 打印从线程拿到的返回指针
    printf("线程join拿到返回：%p\n", ret);

    return 0;
}
