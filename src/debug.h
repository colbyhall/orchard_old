#ifndef DEBUG_H
#define DEBUG_H

#include "platform.h"

typedef enum Log_Severity {
    LS_Verbose,
    LS_Warning,
    LS_Error,
} Log_Severity;

void output_log(Log_Severity Log_Severity, const char* fmt, ...);

#define o_log(str, ...)         output_log(LS_Verbose, str, __VA_ARGS__)
#define o_log_verbose(str, ...) output_log(LS_Verbose, str, __VA_ARGS__)
#define o_log_warning(str, ...) output_log(LS_Warning, str, __VA_ARGS__)
#define o_log_error(str, ...)   output_log(LS_Error, str, __VA_ARGS__)

b32 init_logger(Platform* platform);
void shutdown_logger(void);



#endif /* DEBUG_H */