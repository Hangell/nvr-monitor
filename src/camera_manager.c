#include "nvr/camera_manager.h"
#include <stdlib.h>

int nvr_camera_manager_init(NvrCameraManager *manager, const NvrConfig *config) {
    if (!manager || !config) return -1;
    *manager = (NvrCameraManager){0};
    if (!config->count) return 0;
    manager->cameras = calloc(config->count, sizeof(*manager->cameras));
    if (!manager->cameras) return -1;
    for (size_t i = 0; i < config->count; ++i) {
        if (nvr_camera_init(&manager->cameras[i], &config->cameras[i])) {
            manager->count = i; nvr_camera_manager_destroy(manager); return -1;
        }
        manager->count++;
    }
    return 0;
}

int nvr_camera_manager_start(NvrCameraManager *manager) {
    if (!manager) return -1;
    for (size_t i = 0; i < manager->count; ++i) {
        if (nvr_camera_start(&manager->cameras[i])) {
            for (size_t j = 0; j < i; ++j) nvr_camera_stop(&manager->cameras[j]);
            return -1;
        }
    }
    manager->started = 1;
    return 0;
}
void nvr_camera_manager_stop(NvrCameraManager *manager) {
    if (manager) {
        for (size_t i = 0; i < manager->count; ++i) nvr_camera_stop(&manager->cameras[i]);
        manager->started = 0;
    }
}
void nvr_camera_manager_destroy(NvrCameraManager *manager) {
    if (!manager) return;
    for (size_t i = 0; i < manager->count; ++i) nvr_camera_destroy(&manager->cameras[i]);
    free(manager->cameras); *manager = (NvrCameraManager){0};
}

static int rebuild(NvrCameraManager *manager, const NvrCameraConfig *items, size_t count) {
    int restart = manager->started;
    nvr_camera_manager_destroy(manager);
    NvrConfig config = {(NvrCameraConfig *)items, count};
    if (nvr_camera_manager_init(manager, &config)) return -1;
    return restart ? nvr_camera_manager_start(manager) : 0;
}

int nvr_camera_manager_add(NvrCameraManager *manager, const NvrCameraConfig *config) {
    if (!manager || !config || manager->count >= 9) return -1;
    size_t count = manager->count + 1;
    NvrCameraConfig *items = malloc(count * sizeof(*items));
    if (!items) return -1;
    for (size_t i = 0; i < manager->count; ++i) items[i] = manager->cameras[i].config;
    items[count - 1] = *config;
    int result = rebuild(manager, items, count); free(items); return result;
}

int nvr_camera_manager_remove(NvrCameraManager *manager, size_t index) {
    if (!manager || index >= manager->count) return -1;
    size_t count = manager->count - 1;
    NvrCameraConfig *items = count ? malloc(count * sizeof(*items)) : NULL;
    if (count && !items) return -1;
    for (size_t from = 0, to = 0; from < manager->count; ++from)
        if (from != index) items[to++] = manager->cameras[from].config;
    int result = rebuild(manager, items, count); free(items); return result;
}

int nvr_camera_manager_update(NvrCameraManager *manager, size_t index,
                              const NvrCameraConfig *config) {
    if(!manager||!config||index>=manager->count)return -1;
    NvrCameraConfig *items=malloc(manager->count*sizeof(*items));if(!items)return -1;
    for(size_t i=0;i<manager->count;i++)items[i]=i==index?*config:manager->cameras[i].config;
    int result=rebuild(manager,items,manager->count);free(items);return result;
}
