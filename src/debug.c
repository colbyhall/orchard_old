#include "debug.h"

typedef struct Logger {
    File_Handle file;

    b32 is_initialized;
} Logger;

static Logger* g_logger = 0;

#define LOGS_PATH "logs/"

static const char* log_severity_string[] = {
    "Verbose",
    "Warning",
    "Error",
};

void output_log(Log_Severity severity, const char* fmt, ...) {
    char formatted_msg[2048];
    va_list args;
    va_start(args, fmt);
    const int formatted_msg_len = vsprintf(formatted_msg, fmt, args); // @CRT
    va_end(args);

    const System_Time time = g_platform->local_time();
    const u16 hour = time.hour > 12 ? time.hour - 12 : time.hour;

    char final_msg[2048];
    const int final_msg_len = sprintf(final_msg, "[%02u:%02u:%02u] %s : %s\n", hour, time.minute, time.second, log_severity_string[severity], formatted_msg); // @CRT

    printf(final_msg); // @CRT

    if (g_logger->file.os_handle) {
        g_platform->write_file(g_logger->file, (u8*)final_msg, (int)final_msg_len);
    }
}

b32 init_logger(Platform* platform) {
    g_logger = mem_alloc_struct(platform->permanent_arena, Logger);
    if (g_logger->is_initialized) return true;

    if (!g_platform->file_metadata(string_from_raw(LOGS_PATH), 0)) {
        if (!g_platform->create_directory(string_from_raw(LOGS_PATH))) {
            o_log_error("[Logger] Failed to create path when initializing log system. Will only print to debug console.");
            return false;
        }
    }

    const System_Time time = g_platform->local_time();
    const u16 hour = time.hour > 12 ? time.hour - 12 : time.hour;

    char file_name[2048];
    sprintf(file_name, "%02u_%02u_%04u_%02u_%02u.log", time.month, time.day_of_month, time.year, hour, time.minute);

    char path[2048];
    sprintf(path, "%s%s", LOGS_PATH, file_name);

    if (!g_platform->open_file(string_from_raw(path), FF_Write | FF_Create, &g_logger->file)) {
        o_log_error("[Logger] Failed to create log file when initializing log system. Will only print to debug console");
        return false;
    }

    return true;
}

void shutdown_logger(void) {
    g_platform->close_file(&g_logger->file);
}