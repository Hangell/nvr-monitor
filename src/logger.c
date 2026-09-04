#include "nvr/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void nvr_log(NvrLogLevel level, const char *format, ...) {
    static const char *names[] = { "DEBUG", "INFO", "WARN", "ERROR" };
    time_t now = time(NULL);
    struct tm tm_now;
    char timestamp[32];
    localtime_r(&now, &tm_now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);
    fprintf(stderr, "%s %-5s ", timestamp, names[level]);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}
