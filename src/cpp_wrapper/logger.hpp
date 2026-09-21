#ifndef LOGGER_HPP
#define LOGGER_HPP

/**
 * @file logger.hpp
 * @brief 异步日志库 C++上层封装接口
 * @details
 * 整体分层：底层纯C实现内存池mempool、环形缓冲区ringbuffer；
 * C++ Logger类做面向对象封装，启动后台IO线程实现异步落盘。
 * 业务线程调用log()不会阻塞磁盘IO；日志先推入无锁环形队列，后台线程负责写文件。
 * 禁止拷贝对象，一个实例对应一套内存池+环形队列+后台线程。
 */

#include <memory>
#include <thread>
#include <string>

// 引入底层C模块头文件，MemPool、RingBuffer结构体由此声明
#include "mempool.h"
#include "ringbuffer.h"

/**
 * @brief 日志等级枚举
 * LOG_DEBUG 调试信息
 * LOG_INFO 普通提示信息
 * LOG_WARN 警告信息
 * LOG_ERROR 错误信息
 */
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

/**
 * @class Logger
 * @brief 异步日志对外主类
 * @note 使用流程：
 * 1. 定义Logger对象（仅构造空壳，不创建底层资源）
 * 2. 调用init() 传入日志文件路径，初始化内存池、环形队列、启动后台消费线程
 * 3. 调用log()系列接口输出日志
 * 4. 调用shutdown() 停止线程、刷完剩余日志、释放全部资源
 *    若用户忘记手动shutdown，析构函数会兜底调用shutdown
 * @note 禁止拷贝构造、拷贝赋值，避免多个对象管理同一套C底层资源造成双重释放
 */
class Logger {
public:
    /**
     * @brief 构造函数
     * @note 仅构造C++对象外壳；内存池、环形队列、线程全部推迟到init()创建
     */
    Logger();

    /**
     * @brief 析构函数
     * @note 兜底调用shutdown()，防止用户忘记手动释放资源
     */
    ~Logger();

    // 删除拷贝构造函数，禁止复制Logger实例，防止裸指针多次释放
    Logger(const Logger&) = delete;
    // 删除拷贝赋值运算符
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 初始化日志库，创建底层资源，启动后台写盘线程
     * @param log_file_path 输出日志的文件路径
     * @return 成功返回true；失败返回false
     * @note 不要在构造函数做资源初始化：构造没有返回值，失败无法反馈错误
     */
    bool init(const std::string& log_file_path);

    /**
     * @brief 关闭日志库，停止后台线程，刷完队列残留日志，释放内存池、环形队列
     * @note 业务代码优先手动调用shutdown；析构仅做兜底防护
     */
    void shutdown();

    /**
     * @brief 输出日志，printf风格可变参数接口
     * @param level 日志等级 LogLevel
     * @param fmt 格式化字符串
     * @param ... 可变参数列表
     * @note 业务线程调用，不会阻塞磁盘IO；日志消息送入环形队列后立刻返回
     */
    void log(LogLevel level, const char* fmt, ...);

private:
    /**
     * @brief 后台消费线程主函数
     * @details while循环：从环形队列取出日志块，执行文件写入，归还内存块到内存池
     * running_标志控制循环退出
     */
    void backend_consumer();

    MemPool* pool_;        ///< 底层C内存池句柄，原始指针，由mempool_create / mempool_destroy管理
    RingBuffer* ring_;     ///< 底层C环形缓冲区句柄，原始指针，由ringbuf_create / ringbuf_destroy管理

    ///< 独占智能指针管理后台写盘线程；线程资源堆分配；禁止拷贝，shutdown中执行join等待结束
    std::unique_ptr<std::thread> backend_thread_;

    bool running_;         ///< 后台线程运行标记；true代表线程循环继续执行；false通知线程退出
};

#endif
