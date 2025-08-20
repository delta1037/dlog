/**
*  @authors: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2025.08.20
 * @brief: 日志输出控制
*/
#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "../include/dlog.h"

static uint64_t get_thread_id(){
    return syscall(SYS_gettid);
}

static uint64_t get_process_id(){
    return getpid();
}

#define __FILENAME__ ( __builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__ )
#define DLOG_FORMAT_PREFIX "<%d,%d,%s,%s,%d> "
#define DLOG_VALUE_PREFIX  get_process_id(), get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__

#define d_mod_1_error(format, ...) log_msg(LOG_MODULE_INIT(d_mod_1), LOG_ERROR, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);
#define d_mod_1_info(format, ...)  log_msg(LOG_MODULE_INIT(d_mod_1), LOG_INFO, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);
#define d_mod_1_warn(format, ...)  log_msg(LOG_MODULE_INIT(d_mod_1), LOG_WARN, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);
#define d_mod_1_debug(format, ...) log_msg(LOG_MODULE_INIT(d_mod_1), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);

#define d_mod_2_error(format, ...) log_msg(LOG_MODULE_INIT(d_mod_2), LOG_ERROR, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);
#define d_mod_2_warn(format, ...)  log_msg(LOG_MODULE_INIT(d_mod_2), LOG_WARN, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);
#define d_mod_2_info(format, ...)  log_msg(LOG_MODULE_INIT(d_mod_2), LOG_INFO, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);
#define d_mod_2_debug(format, ...) log_msg(LOG_MODULE_INIT(d_mod_2), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, DLOG_VALUE_PREFIX, ##__VA_ARGS__);

#endif //LOG_H
