#ifndef NVR_CONFIG_H
#define NVR_CONFIG_H

#include <stddef.h>

#define NVR_NAME_MAX 64
#define NVR_HOST_MAX 256
#define NVR_USER_MAX 128
#define NVR_PATH_MAX 256
#define NVR_ENV_MAX 128
#define NVR_PASSWORD_MAX 256

typedef enum {
    NVR_TRANSPORT_TCP = 0,
    NVR_TRANSPORT_UDP = 1
} NvrTransport;

typedef struct {
    char name[NVR_NAME_MAX];
    char host[NVR_HOST_MAX];
    unsigned short port;
    char username[NVR_USER_MAX];
    char password[NVR_PASSWORD_MAX];
    char password_env[NVR_ENV_MAX];
    char grid_path[NVR_PATH_MAX];
    char main_path[NVR_PATH_MAX];
    NvrTransport transport;
} NvrCameraConfig;

typedef struct {
    NvrCameraConfig *cameras;
    size_t count;
} NvrConfig;

int nvr_config_load(const char *path, NvrConfig *config, char *error, size_t error_size);
int nvr_config_save(const char *path, const NvrConfig *config, char *error, size_t error_size);
void nvr_config_free(NvrConfig *config);
const char *nvr_transport_name(NvrTransport transport);

#endif
