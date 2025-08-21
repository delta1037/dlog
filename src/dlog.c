#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

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
        fprintf(stderr, "Error opening config file: %s\n", config_path);
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
        case LOG_ERROR: return "[ERROR]";
        case LOG_WARN:  return "[WARN] ";
        case LOG_INFO:  return "[INFO] ";
        case LOG_DEBUG: return "[DEBUG]";
        case LOG_FATAL: return "[FATAL]";
        default:        return "[NONE] ";
    }
}

static int is_greater_than_level(logger_t* logger, uint8_t level) {
    return logger->level <= level;
}

static const char* get_time_string() {
    static __thread char buffer[TIME_STRING_BUFFER_SIZE];  // TLS实现线程安全
    struct timeval tv;
    struct tm tm_info;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_info);

    strftime(buffer, sizeof(buffer), TIME_FORMAT, &tm_info);
    snprintf(buffer + 19, sizeof(buffer) - 19, MILLISECOND_FORMAT, tv.tv_usec / 1000);

    return buffer;
}

static void log_to_file(logger_t* logger, uint8_t level, const char* message) {
    FILE* file = fopen(logger->filename, "a");
    if (!file) return;
    
    const char* time_str = get_time_string();
    fprintf(file, "%s %s %s\n", time_str, get_level_str(level), message);
    fclose(file);
}

static void log_to_screen(uint8_t level, const char* message) {
    const char* time_str = get_time_string();
    printf("%s %s %s\n", time_str, get_level_str(level), message);
}

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
    return log;
}

void logger_log_message(logger_t* logger, short level, const char* message) {
    if (!is_greater_than_level(logger, level)) {
        return;
    }
    
    switch (logger->type) {
        case OUTPUT_FILE:
            log_to_file(logger, level, message);
            break;
        case OUTPUT_SCREEN:
            log_to_screen(level, message);
            break;
        case OUTPUT_NONE:
            break;
    }
}

void logger_free(logger_t* logger) {
    if (logger) {
        free(logger->filename);
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
    return logger_ctl_register_logger(module_name);
}

void log_msg(void *logger, log_level level, const char *format, ...) {
    if (!logger || !format) {
        fprintf(stderr, "Error: logger=%p, format=%p\n", logger, format);
        return;
    }

    // 日志消息格式化
    char buffer[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    int64_t written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);  // 结束变参访问
    if (written < 0) {
        fprintf(stderr, "Error: vsnprintf failed\n");
        strncpy(buffer, LOG_FORMAT_ERROR_MSG, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
    } else if (written >= MAX_BUFFER) {
        buffer[sizeof(buffer) - 1] = '\0';  // 确保截断后仍以NULL结尾
        fprintf(stderr, TRUNCATION_WARNING_MSG, written);
    }

    // 发送日志消息
    logger_log_message((logger_t*)logger, level, buffer);
}