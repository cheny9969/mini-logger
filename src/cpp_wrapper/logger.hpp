#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <memory>
#include <thread>
#include "mempool.h"
#include "ringbuffer.h"

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

class Logger {
public:
    Logger();
    ~Logger();

    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const char* fmt, ...);

private:
    void backend_consumer(); // 后台刷盘线程

    MemPool* pool_;
    RingBuffer* ring_;
    std::unique_ptr<std::thread> backend_thread_;
    bool running_;
};

#endif
