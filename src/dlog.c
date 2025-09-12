#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>

#include "../include/dlog.h"

/* 文件路径 */
#define DEFAULT_FILEPATH_SIZE 128
#define MAX_CONFIG_LINE_SIZE 256
#define MAX_CONFIG_KEY_SIZE (MAX_CONFIG_LINE_SIZE / 2 - 2)
#define MAX_CONFIG_VALUE_SIZE (MAX_CONFIG_LINE_SIZE / 2 - 2)
#define DEFAULT_LOG_SUFFIX "default.log"
/* 时间格式 */
#define TIME_STRING_BUFFER_SIZE 32
#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define TIME_FORMAT_PLAIN "%Y%m%d_%H%M%S"
#define MILLISECOND_FORMAT ".%03ld"
/* 初始容量和增长因子 */
#define INITIAL_LOGGER_CAPACITY 10
#define CAPACITY_GROWTH_FACTOR 2
/* 错误消息 */
#define LOG_FORMAT_ERROR_MSG "[LOG FORMAT ERROR]"
#define TRUNCATION_WARNING_MSG "Warning: log truncated (needed %ld bytes)\n"

/* Logger structure */
typedef struct {
    log_level level;
    log_type type;
    char* filename;
    FILE* file;
    pthread_mutex_t filemutex;
} logger_t;

/* Logger control structure */
typedef struct {
    logger_t** loggers;
    char** module_names;
    int count;
    int capacity;
} logger_ctl_t;

static logger_ctl_t* logger_ctl_inst = NULL;

static void logger_ctl_get_config(const char* name, log_type* type, log_level* level, char* filename) {
    char config_path[DEFAULT_FILEPATH_SIZE];
    snprintf(config_path, sizeof(config_path), "./%s", LOGGER_CONFIG);
    
    FILE* file = fopen(config_path, "r");
    if (!file){
        DLOG_ERROR_PRINT("Error opening config file: %s\n", config_path);
        return;
    }
    
    char buffer[MAX_CONFIG_LINE_SIZE];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (buffer[0] == '#') continue;
        
        char key[MAX_CONFIG_KEY_SIZE], value[MAX_CONFIG_VALUE_SIZE];
        if (sscanf(buffer, "%125s = %125s", key, value) != 2) continue;
        
        // Check if this is the config for our module
        char search_str[MAX_CONFIG_KEY_SIZE];
        snprintf(search_str, sizeof(search_str), ".%s.", name);
        if (!strstr(key, search_str)) continue;
        
        if (strstr(key, ".log_level")) {
            if (strcmp(value, "DEBUG") == 0) {
                *level = LOG_DEBUG;
            } else if (strcmp(value, "WARN") == 0) {
                *level = LOG_WARN;
            } else if (strcmp(value, "INFO") == 0) {
                *level = LOG_INFO;
            } else if (strcmp(value, "ERROR") == 0) {
                *level = LOG_ERROR;
            } else if (strcmp(value, "FATAL") == 0) {
                *level = LOG_FATAL;
            }
        } else if (strstr(key, ".log_type")) {
            if (strcmp(value, "SCREEN") == 0) {
                *type = OUTPUT_SCREEN;
            } else if (strcmp(value, "FILE") == 0) {
                *type = OUTPUT_FILE;
            }
        } else if (strstr(key, ".log_file")) {
            snprintf(filename, MAX_CONFIG_VALUE_SIZE, "%s", value);
        }
    }
    
    fclose(file);
}

static const char* get_level_str(uint8_t level) {
    switch(level) {
        case LOG_ERROR: return "[ERRO]";
        case LOG_WARN:  return "[WARN]";
        case LOG_INFO:  return "[INFO]";
        case LOG_DEBUG: return "[DEBG]";
        case LOG_FATAL: return "[FTAL]";
        default:        return "[NONE]";
    }
}

static int is_greater_than_level(logger_t* logger, uint8_t level) {
    return (uint8_t)logger->level <= level;
}

static const char* get_time_string(char* buffer) {
    struct timeval tv;
    struct tm tm_info;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_info);

    strftime(buffer, TIME_STRING_BUFFER_SIZE, TIME_FORMAT, &tm_info);
    snprintf(buffer + 19, TIME_STRING_BUFFER_SIZE - 19, MILLISECOND_FORMAT, tv.tv_usec / 1000);

    return buffer;
}

static const char* get_time_string_plain(char* buffer) {
    struct timeval tv;
    struct tm tm_info;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_info);
    strftime(buffer, TIME_STRING_BUFFER_SIZE, TIME_FORMAT_PLAIN, &tm_info);
    return buffer;
}

static void log_to_file(logger_t* logger, uint8_t level, const char* time_str, const char* message) {
    if (!logger->file) return;
    pthread_mutex_lock(&logger->filemutex);
    fprintf(logger->file, "%s %s %s\n", time_str, get_level_str(level), message);
    fflush(logger->file);
    // 日志滚动
    long pos = ftell(logger->file);
    if (pos < 0) {
        DLOG_ERROR_PRINT("Error getting file position for log file: %s\n", logger->filename);
        pthread_mutex_unlock(&logger->filemutex);
        return;
    }
    if (pos <= MAX_LOG_FILE_SIZE) { // 10MB
        pthread_mutex_unlock(&logger->filemutex);
        return;
    } else {
        fclose(logger->file);
        char backup_filename[DEFAULT_FILEPATH_SIZE];
        char time_str[TIME_STRING_BUFFER_SIZE];
        get_time_string_plain(time_str);
        snprintf(backup_filename, sizeof(backup_filename), "%s.%s", logger->filename, time_str);
        if (rename(logger->filename, backup_filename) != 0) {
            DLOG_ERROR_PRINT("Failed to rename log file: %s -> %s (errno: %d)\n",
                    logger->filename, backup_filename, errno);
        }
        logger->file = fopen(logger->filename, "a+");
        if (!logger->file) {
            DLOG_ERROR_PRINT("Error reopening log file: %s\n", logger->filename);
        }
    }
    pthread_mutex_unlock(&logger->filemutex);
}

static void log_to_screen(uint8_t level, const char* time_str, const char* message) {
    printf("%s %s %s\n", time_str, get_level_str(level), message);
}

struct log_buffer_meta {
    int used;
    int get_count;
    int release_count;
};
struct log_buffer {
    logger_t* logger;
    log_level level;
    char* time_str;
    char* message;
    struct log_buffer_meta *meta;
};
// buffer pool lock
static pthread_mutex_t buffer_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct log_buffer pool[LOG_BUFFER_POOL_SIZE] = {0};
static volatile int start_index = 0;
// buffer pool
struct log_buffer *get_buffer() {
    pthread_mutex_lock(&buffer_pool_mutex);
    for (int i = 0; i < LOG_BUFFER_POOL_SIZE; i++){
        int idx = (start_index + i) % LOG_BUFFER_POOL_SIZE;
        DLOG_DEBUG_PRINT("Checking buffer index: %d meta: %p\n", idx, pool[idx].meta);
        if (pool[idx].meta == NULL) {
            pool[idx].time_str = (char*)malloc(TIME_STRING_BUFFER_SIZE);
            pool[idx].message = (char*)malloc(MAX_BUFFER);
            pool[idx].meta = (struct log_buffer_meta*)malloc(sizeof(struct log_buffer_meta));
            pool[idx].meta->used = 1;
            pool[idx].meta->get_count = 1;
            pool[idx].meta->release_count = 0;
            start_index = (idx + 1) % LOG_BUFFER_POOL_SIZE;
            pthread_mutex_unlock(&buffer_pool_mutex);
            DLOG_DEBUG_PRINT("Allocating new buffer at index: %d, %p\n", idx, &pool[idx]);
            return &pool[idx];
        } else if (pool[idx].meta->used == 0) {
            pool[idx].meta->used = 1;
            pool[idx].meta->get_count += 1;
            start_index = (idx + 1) % LOG_BUFFER_POOL_SIZE;
            pthread_mutex_unlock(&buffer_pool_mutex);
            DLOG_DEBUG_PRINT("Reusing buffer at index: %d, %p\n", idx, &pool[idx]);
            return &pool[idx];
        }
        DLOG_DEBUG_PRINT("Buffer index %d is in use\n", idx);
    }
    pthread_mutex_unlock(&buffer_pool_mutex);
    DLOG_DEBUG_PRINT("Error: All log buffers are in use\n");
    return NULL;  // 池已满
}
void release_buffer(struct log_buffer *buffer) {
    pthread_mutex_lock(&buffer_pool_mutex);
    DLOG_DEBUG_PRINT("Releasing buffer: %p\n", buffer);
    if (buffer && buffer->meta) {
        buffer->meta->used = 0;
        buffer->meta->release_count += 1;
        buffer->logger = NULL;
        buffer->level = UNKNOWN;
        buffer->time_str[0] = '\0';  // 清空数据
        buffer->message[0] = '\0';  // 清空数据
    }
    pthread_mutex_unlock(&buffer_pool_mutex);
}
void log_buffer_debug_info() {
    pthread_mutex_lock(&buffer_pool_mutex);
    for (int i = 0; i < LOG_BUFFER_POOL_SIZE; i++){
        if (pool[i].meta) {
            if(pool[i].meta->used) {
                DLOG_DEBUG_PRINT("Buffer %d - in use\n", i);
            }
            if(pool[i].meta->get_count != pool[i].meta->release_count) {
                DLOG_DEBUG_PRINT("Warning: Buffer %d - get_count (%d) != release_count (%d)\n", 
                        i, pool[i].meta->get_count, pool[i].meta->release_count);
            }
        } else {
            DLOG_DEBUG_PRINT("Buffer %d - uninitialized\n", i);
        }
    }
    pthread_mutex_unlock(&buffer_pool_mutex);
}

void logger_log_message(struct log_buffer *log) {
    if (!is_greater_than_level(log->logger, log->level)) {
        return;
    }
    
    switch (log->logger->type) {
        case OUTPUT_FILE:
            log_to_file(log->logger, log->level, log->time_str, log->message);
            break;
        case OUTPUT_SCREEN:
            log_to_screen(log->level, log->time_str, log->message);
            break;
        case OUTPUT_NONE:
            break;
    }
}

#if (ASYNC_LOG)
// 日志队列节点
struct log_queue{
    struct log_buffer *log;
    struct log_queue *next;
    struct log_queue *prev;
};
// 日志队列头
static struct log_queue *log_head = NULL;
static struct log_queue *log_tail = NULL;
// 日志队列访问条件变量和互斥锁
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_cond = PTHREAD_COND_INITIALIZER;
// 异步线程启动标志
static volatile int async_thread_started = 0;

// 异步线程
static pthread_t thread_id;
void *async_log_thread_func(void* arg) {
    (void)arg;  // Explicitly mark as unused
    // 绑定到CPU 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    DLOG_DEBUG_PRINT("Async log thread started\n");
    while (async_thread_started || log_head->prev != log_tail) {
        pthread_mutex_lock(&log_mutex);
        while(log_head->prev == log_tail) {
            // 队列为空，等待新日志
            DLOG_DEBUG_PRINT("Async log thread waiting for logs\n");
            pthread_cond_wait(&log_cond, &log_mutex);
        }
        // 取出队列头日志
        struct log_queue *current = log_head->prev;
        // 从队列中移除
        log_head->prev = current->prev;
        current->prev->next = log_head;
        pthread_mutex_unlock(&log_mutex);
        DLOG_DEBUG_PRINT("Processing log: %p\n", current->log);
        if (current) {
            logger_log_message(current->log);
            // 释放日志缓冲区
            release_buffer(current->log);
        }
    }
    return NULL;
}
#endif

logger_t* logger_create(const char* logger_name, log_level level, log_type type, const char* filename) {
    logger_t* log = (logger_t*)malloc(sizeof(logger_t));
    if (!log) return NULL;
    
    log->level = level;
    log->type = type;
    
    if (type == OUTPUT_FILE && (!filename || strlen(filename) == 0)) {
        char default_filename[DEFAULT_FILEPATH_SIZE];
        snprintf(default_filename, DEFAULT_FILEPATH_SIZE, "%s_%s", logger_name, DEFAULT_LOG_SUFFIX);
        log->filename = strdup(default_filename);
    } else {
        log->filename = strdup(filename ? filename : "");
    }
    if (type == OUTPUT_FILE) {
        log->file = fopen(log->filename, "a");
        if (!log->file) {
            DLOG_ERROR_PRINT("Error opening log file: %s\n", log->filename);
            free(log->filename);
            free(log);
            return NULL;
        }
        pthread_mutex_init(&log->filemutex, NULL);
    } else {
        log->file = NULL;
    }
    return log;
}


void logger_free(logger_t* logger) {
    if (logger) {
        free(logger->filename);
        if (logger->file) {
            fclose(logger->file);
            pthread_mutex_destroy(&logger->filemutex);
        }
        free(logger);
    }
}

logger_ctl_t* logger_ctl_instance() {
    if (!logger_ctl_inst) {
        logger_ctl_inst = (logger_ctl_t*)malloc(sizeof(logger_ctl_t));
        if (!logger_ctl_inst) return NULL;
        
        logger_ctl_inst->count = 0;
        logger_ctl_inst->capacity = INITIAL_LOGGER_CAPACITY;
        logger_ctl_inst->loggers = (logger_t**)malloc(sizeof(logger_t*) * logger_ctl_inst->capacity);
        logger_ctl_inst->module_names = (char**)malloc(sizeof(char*) * logger_ctl_inst->capacity);
    }
    return logger_ctl_inst;
}

void logger_ctl_free() {
    if (logger_ctl_inst) {
        for (int i = 0; i < logger_ctl_inst->count; i++) {
            logger_free(logger_ctl_inst->loggers[i]);
            free(logger_ctl_inst->module_names[i]);
        }
        free(logger_ctl_inst->loggers);
        free(logger_ctl_inst->module_names);
        free(logger_ctl_inst);
        logger_ctl_inst = NULL;
    }
}

void* logger_ctl_register_logger(const char* module_name) {
    logger_ctl_t* ctl = logger_ctl_instance();
    if (!ctl || !module_name) return NULL;
    
    // Check if already registered
    for (int i = 0; i < ctl->count; i++) {
        if (strcmp(ctl->module_names[i], module_name) == 0) {
            return ctl->loggers[i];
        }
    }
    
    // Get logger config
    log_type type = OUTPUT_SCREEN;
    log_level level = LOG_INFO;
    char filename[MAX_CONFIG_VALUE_SIZE] = DEFAULT_LOG_SUFFIX;
    logger_ctl_get_config(module_name, &type, &level, filename);
    
    // Create and register logger
    logger_t* loger = logger_create(module_name, level, type, filename);
    if (!loger) return NULL;
    
    // Resize arrays if needed
    if (ctl->count >= ctl->capacity) {
        ctl->capacity *= CAPACITY_GROWTH_FACTOR;
        ctl->loggers = (logger_t**)realloc(ctl->loggers, sizeof(logger_t*) * ctl->capacity);
        ctl->module_names = (char**)realloc(ctl->module_names, sizeof(char*) * ctl->capacity);
    }
    
    ctl->module_names[ctl->count] = strdup(module_name);
    ctl->loggers[ctl->count] = loger;
    ctl->count++;
    
    return loger;
}

void* log_module_init(const char* module_name) {
    if (!module_name) {
        printf("module name is NULL");
        return NULL;
    }
#if (ASYNC_LOG)
    static pthread_mutex_t async_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&async_thread_mutex);
    if (!async_thread_started) {
        async_thread_started = 1;
        // 初始化队列
        log_head = (struct log_queue*)malloc(sizeof(struct log_queue));
        log_tail = (struct log_queue*)malloc(sizeof(struct log_queue));
        log_head->prev = log_tail;
        log_tail->next = log_head;
        log_tail->log = NULL;

        // 启动异步线程
        if (pthread_create(&thread_id, NULL, async_log_thread_func, NULL) != 0) {
            DLOG_ERROR_PRINT("Error creating async log thread\n");
            async_thread_started = 0;
            pthread_mutex_unlock(&async_thread_mutex);
            return NULL;
        }
        pthread_detach(thread_id);  // 分离线程
    }
    pthread_mutex_unlock(&async_thread_mutex);
#endif
    return logger_ctl_register_logger(module_name);
}

void log_msg(void *logger, log_level level, const char *format, ...) {
    if (!logger || !format) {
        DLOG_ERROR_PRINT("Error: logger=%p, format=%p\n", logger, format);
        return;
    }

    // 日志消息格式化
    int try_count = 3;
    struct log_buffer *log_buffer = NULL;
    do{
        if(try_count < 3){
            DLOG_ERROR_PRINT("Retrying to get log buffer, attempts left: %d\n", try_count);
            usleep(100 * 1000); // 100ms
        }
        log_buffer = get_buffer();
    }while (log_buffer == NULL && try_count-- > 0);
    if (log_buffer == NULL) {
        DLOG_ERROR_PRINT("Error: Unable to get log buffer after multiple attempts\n");
        return;
    }

    va_list args;
    va_start(args, format);
    int64_t written = vsnprintf(log_buffer->message, MAX_BUFFER, format, args);
    va_end(args);  // 结束变参访问
    if (written < 0) {
        DLOG_ERROR_PRINT("Error: vsnprintf failed\n");
        strncpy(log_buffer->message, LOG_FORMAT_ERROR_MSG, MAX_BUFFER);
    } else if (written >= MAX_BUFFER) {
        log_buffer->message[MAX_BUFFER - 1] = '\0';  // 确保NULL结尾
        DLOG_ERROR_PRINT(TRUNCATION_WARNING_MSG, written);
    }
    // 准备日志消息
    log_buffer->logger = (logger_t*)logger;
    log_buffer->level = level;
    get_time_string(log_buffer->time_str);

#if (ASYNC_LOG)
    // 将日志消息添加到队列
    struct log_queue *new_node = (struct log_queue*)malloc(sizeof(struct log_queue));
    new_node->log = log_buffer;
    pthread_mutex_lock(&log_mutex);
    // 添加到队列尾
    new_node->next = log_tail->next;
    new_node->prev = log_tail;
    log_tail->next = new_node;
    new_node->next->prev = new_node;
    // 通知异步线程
    pthread_cond_signal(&log_cond);  // 确保发送信号
    pthread_mutex_unlock(&log_mutex);
#else
    // 发送日志消息
    logger_log_message(log_buffer);
    // 立即释放缓冲区
    release_buffer(log_buffer);
#endif
}

#if (ASYNC_LOG)
void fflush_async_log() {
    // 发送信号以确保所有日志都被处理
    pthread_mutex_lock(&log_mutex);
    pthread_cond_signal(&log_cond);
    pthread_mutex_unlock(&log_mutex);
    // 等待异步线程处理完毕
    async_thread_started = 0; // 停止线程
    while (log_head->prev != log_tail) {
        DLOG_DEBUG_PRINT("Waiting for async log thread to finish processing\n");
        usleep(100 * 1000); // 100ms
    }
}
#endif

// lib 析构函数
__attribute__((destructor))
static void log_library_destructor() {
#if (ASYNC_LOG)
    fflush_async_log();
#endif
    logger_ctl_free();
    // 释放日志缓冲区池
    for (int i = 0; i < LOG_BUFFER_POOL_SIZE; i++){ 
        if (pool[i].meta) {
            free(pool[i].time_str);
            free(pool[i].message);
            free(pool[i].meta);
            pool[i].meta = NULL;
            pool[i].time_str = NULL;
            pool[i].message = NULL;
        }
    }
    pthread_mutex_destroy(&buffer_pool_mutex);
}