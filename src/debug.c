#include "debug.h"
#include "draw.h"

typedef struct Logger {
    File_Handle file;

    b32 is_initialized;
} Logger;

static Logger* logger = 0;

#define LOGS_PATH "logs/"

static const char* log_severity_string[] = {
    "Verbose",
    "Warning",
    "Error",
};

void output_log(Log_Severity severity, const char* fmt, ...) {
    char* formatted_msg = mem_alloc_array(g_platform->frame_arena, u8, str_len(fmt) + 512);
    va_list args;
    va_start(args, fmt);
    const int formatted_msg_len = vsprintf(formatted_msg, fmt, args); // @CRT
    va_end(args);

    const System_Time time = g_platform->local_time();
    const u16 hour = time.hour > 12 ? time.hour - 12 : time.hour;

    fprintf(severity == LS_Error ? stderr : stdout, formatted_msg); // @CRT
    printf("\n");

    char* final_msg = mem_alloc_array(g_platform->frame_arena, u8, formatted_msg_len + 512);
    const int final_msg_len = sprintf(final_msg, "[%02u:%02u:%02u] %s : %s\n", hour, time.minute, time.second, log_severity_string[severity], formatted_msg); // @CRT

    if (logger->file.os_handle) {
        g_platform->write_file(logger->file, (u8*)final_msg, final_msg_len);
    }
}

b32 init_logger(Platform* platform) {
    logger = mem_alloc_struct(platform->permanent_arena, Logger);
    if (logger->is_initialized) return true;

    if (!g_platform->file_metadata(from_cstr(LOGS_PATH), 0)) {
        if (!g_platform->create_directory(from_cstr(LOGS_PATH))) {
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

    if (!g_platform->open_file(from_cstr(path), FF_Write | FF_Create, &logger->file)) {
        o_log_error("[Logger] Failed to create log file when initializing log system. Will only print to debug console");
        return false;
    }

    return true;
}

void shutdown_logger(void) {
    g_platform->close_file(&logger->file);
}


