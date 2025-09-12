#ifndef LOG_LOG_H
#define LOG_LOG_H

#include <string.h>
#include <unistd.h>

#define LOGGER_CONFIG "dlog.properties"

// 每条日志的最大长度
#define MAX_BUFFER 4096
// 是否使用异步日志
#define ASYNC_LOG 0  // 0-同步日志，1-异步日志
// 日志内存池大小（同步时建议和线程个数一致；异步时尽量大一点）
#define LOG_BUFFER_POOL_SIZE 500

// 是否启用调试日志，启用后会打印更多内部状态到stderr
// #define DLOG_DEBUG
#if defined(DLOG_DEBUG)
#include <stdio.h>
#define DLOG_DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DLOG DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DLOG_DEBUG_PRINT(fmt, ...)
#endif
#define DLOG_ERROR_PRINT(fmt, ...) fprintf(stderr, "[DLOG ERROR] " fmt, ##__VA_ARGS__)

// 注册日志模块，返回实例
#define LOG_MODULE_INIT(module) log_module_init(#module)

#define CHECK(x,m,handle) if((x) == (m)){   \
                           handle;          \
                         }

/* Log level definitions */
typedef enum {
    UNKNOWN = 0,
    LOG_DEBUG = 1,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level;

/* Log output type definitions */
typedef enum {
    OUTPUT_FILE = 0,
    OUTPUT_SCREEN,
    OUTPUT_NONE
} log_type;

/* Public interface */
void *log_module_init(const char *module_name);
void log_msg(void *logger, log_level logLevel, const char *format, ... );
#if (ASYNC_LOG)
void fflush_async_log();
#endif

/* Debug interface */
void log_buffer_debug_info();

#endif //LOG_LOG_H