#include "logger.hpp"
#include <cstdio>
#include <cstdarg>
#include <ctime>

Logger::Logger()
{
    // TODO: 创建mempool、ringbuffer，启动后台线程
}

Logger::~Logger()
{
    // TODO: 停止后台线程，等待消费完，释放资源
}

void Logger::log(LogLevel level, const char* fmt, ...)
{
    // TODO: va_list 格式化日志，申请block，入环形队列
}

void Logger::backend_consumer()
{
    // TODO: 循环从ring取block，写文件
}
