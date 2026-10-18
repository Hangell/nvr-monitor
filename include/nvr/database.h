#ifndef NVR_DATABASE_H
#define NVR_DATABASE_H

#include <stddef.h>
#include "nvr/config.h"

typedef struct sqlite3 sqlite3;
typedef struct { sqlite3 *handle; char path[512]; } NvrDatabase;

typedef struct {
    int english;
    int hide_menu;
    char screenshot_directory[512];
    char recording_directory[512];
} NvrSettings;

int nvr_database_load_settings(NvrDatabase *database, NvrSettings *settings);
int nvr_database_save_settings(NvrDatabase *database, const NvrSettings *settings);

int nvr_database_open(NvrDatabase *database, const char *path,
                      char *error, size_t error_size);
int nvr_database_load(NvrDatabase *database, NvrConfig *config,
                      char *error, size_t error_size);
int nvr_database_insert_camera(NvrDatabase *database, const NvrCameraConfig *camera,
                               char *error, size_t error_size);
int nvr_database_update_camera(NvrDatabase *database, size_t index,
                               const NvrCameraConfig *camera,
                               char *error, size_t error_size);
int nvr_database_delete_camera(NvrDatabase *database, size_t index,
                               char *error, size_t error_size);
int nvr_database_replace_all(NvrDatabase *database, const NvrConfig *config,
                             char *error, size_t error_size);
void nvr_database_close(NvrDatabase *database);

#endif
