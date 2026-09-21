#include "logger.hpp"
#include <iostream>
#include <thread>

int main()
{
    // TODO: 构造Logger实例
    Logger logger;

    // TODO: 定义lambda生产者函数
    // 参数id：线程编号；循环多次调用 logger.log() 打日志
    auto writer = [&logger](int id) {
        // TODO: 循环，多次调用 logger.log(LOG_INFO, 格式化字符串, 参数)
        for(int i = 0; i < 2000; ++i) {
            // TODO: 调用logger.log(...)
        }
    };

    // TODO: 创建3个生产者线程，执行writer
    std::thread t1(writer, 1);
    std::thread t2(writer, 2);
    std::thread t3(writer, 3);

    // TODO: join等待全部生产者写完日志
    t1.join();
    t2.join();
    t3.join();

    std::cout << "finish write\n";

    // TODO：logger离开作用域，析构函数自动做：通知后台消费线程退出、join后台线程、释放mempool/ringbuffer资源
    return 0;
}
