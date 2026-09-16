#include "logger.hpp"
#include <iostream>
#include <thread>

int main()
{
    Logger logger;

    // 多线程压测生产者
    auto writer = [&logger](int id) {
        for(int i = 0; i < 2000; ++i) {
            logger.log(LOG_INFO, "thread=%d, cnt=%d", id, i);
        }
    };

    std::thread t1(writer, 1);
    std::thread t2(writer, 2);
    std::thread t3(writer, 3);

    t1.join();
    t2.join();
    t3.join();

    std::cout << "finish write\n";
    // logger析构自动等待后台刷盘、释放资源
    return 0;
}
