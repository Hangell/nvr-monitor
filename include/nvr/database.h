#ifndef NVR_DATABASE_H
#define NVR_DATABASE_H

#include <stddef.h>
#include "nvr/config.h"

typedef struct sqlite3 sqlite3;
typedef struct { sqlite3 *handle; char path[512]; } NvrDatabase;

int nvr_database_open(NvrDatabase *database, const char *path,
                      char *error, size_t error_size);
int nvr_database_load(NvrDatabase *database, NvrConfig *config,
                      char *error, size_t error_size);
int nvr_database_insert_camera(NvrDatabase *database, const NvrCameraConfig *camera,
                               char *error, size_t error_size);
int nvr_database_replace_all(NvrDatabase *database, const NvrConfig *config,
                             char *error, size_t error_size);
void nvr_database_close(NvrDatabase *database);

#endif
