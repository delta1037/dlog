#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/dlog.h"

#define CONFIG_PATH_SIZE 128

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
    char config_path[CONFIG_PATH_SIZE];
    snprintf(config_path, sizeof(config_path), "./%s", LOGGER_CONFIG);
    
    FILE* file = fopen(config_path, "r");
    if (!file){
        fprintf(stderr, "Error opening config file: %s\n", config_path);
        return;
    }
    
    char buffer[MAX_BUFFER];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (buffer[0] == '#') continue;
        
        char key[256], value[256];
        if (sscanf(buffer, "%255s = %255s", key, value) != 2) continue;
        
        // Check if this is the config for our module
        char search_str[256];
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
            snprintf(filename, 1024, "./%s", value);
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
    static __thread char buffer[32];  // TLS实现线程安全
    struct timeval tv;
    struct tm tm_info;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_info);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
    snprintf(buffer + 19, sizeof(buffer) - 19, ".%03ld", tv.tv_usec / 1000);

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
        char default_filename[256];
        snprintf(default_filename, sizeof(default_filename), "%s_default.log", logger_name);
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
        logger_ctl_inst->capacity = 10;
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
    char filename[1024] = "default.log";
    
    logger_ctl_get_config(module_name, &type, &level, filename);
    
    // Create and register logger
    logger_t* loger = logger_create(module_name, level, type, filename);
    if (!loger) return NULL;
    
    // Resize arrays if needed
    if (ctl->count >= ctl->capacity) {
        ctl->capacity *= 2;
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

    char buffer[MAX_BUFFER];
    va_list args;
    va_start(args, format);
    int64_t written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);  // 立即结束变参访问

    if (written < 0) {
        fprintf(stderr, "Error: vsnprintf failed\n");
        strncpy(buffer, "[LOG FORMAT ERROR]", sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
    } else if (written >= (int64_t)sizeof(buffer)) {
        buffer[sizeof(buffer) - 1] = '\0';  // 确保截断后仍以NULL结尾
        fprintf(stderr, "Warning: log truncated (needed %ld bytes)\n", written);
    }

    logger_log_message((logger_t*)logger, level, buffer);
}