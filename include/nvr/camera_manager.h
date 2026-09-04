#ifndef NVR_CAMERA_MANAGER_H
#define NVR_CAMERA_MANAGER_H

#include <stddef.h>
#include "nvr/camera.h"

typedef struct { NvrCamera *cameras; size_t count; int started; } NvrCameraManager;
int nvr_camera_manager_init(NvrCameraManager *manager, const NvrConfig *config);
int nvr_camera_manager_add(NvrCameraManager *manager, const NvrCameraConfig *config);
int nvr_camera_manager_update(NvrCameraManager *manager, size_t index,
                              const NvrCameraConfig *config);
int nvr_camera_manager_remove(NvrCameraManager *manager, size_t index);
int nvr_camera_manager_start(NvrCameraManager *manager);
void nvr_camera_manager_stop(NvrCameraManager *manager);
void nvr_camera_manager_destroy(NvrCameraManager *manager);

#endif
