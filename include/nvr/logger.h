#ifndef NVR_LOGGER_H
#define NVR_LOGGER_H

typedef enum { NVR_LOG_DEBUG, NVR_LOG_INFO, NVR_LOG_WARN, NVR_LOG_ERROR } NvrLogLevel;
void nvr_log(NvrLogLevel level, const char *format, ...);

#endif
