#ifndef LOG_LOG_H
#define LOG_LOG_H

#include <string.h>
#include <unistd.h>

#define LOGGER_CONFIG "dlog.properties"

// 注册日志模块，返回实例
#define LOG_MODULE_INIT(module) log_module_init(#module)

// 对外提供的一些必要的定义
#define MAX_BUFFER 20480

#define CHECK(x,m,handle) if((x) == (m)){   \
                           handle;          \
                         }

/* Log level definitions */
typedef enum {
    LOG_DEBUG = 0,
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

#endif //LOG_LOG_H