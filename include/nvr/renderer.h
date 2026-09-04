#ifndef NVR_RENDERER_H
#define NVR_RENDERER_H

#include <SDL.h>
#include "nvr/camera_manager.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture **textures;
    uint64_t *serials;
    size_t texture_count;
    int fullscreen_camera;
    size_t layout_slots;
} NvrRenderer;

#define NVR_MENU_HEIGHT 44

int nvr_renderer_init(NvrRenderer *renderer, const char *title, int width, int height,
                      size_t camera_count);
void nvr_renderer_draw(NvrRenderer *renderer, NvrCameraManager *manager);
void nvr_renderer_toggle_fullscreen_camera(NvrRenderer *renderer, int camera_index);
int nvr_renderer_resize_cameras(NvrRenderer *renderer, size_t camera_count);
void nvr_renderer_destroy(NvrRenderer *renderer);

#endif
