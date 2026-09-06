#include "nvr/renderer.h"
#include "nvr/layout.h"
#include "nvr/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Surface *load_application_icon(void) {
    const char *override = getenv("NVR_ICON_PATH");
    const char *paths[] = {
        override,
        NVR_SOURCE_ICON_PATH,
        NVR_INSTALLED_ICON_PATH,
        "assets/icons/nvr-monitor-window.bmp",
        "../share/nvr-monitor/assets/icons/nvr-monitor-window.bmp"
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (paths[i] && paths[i][0]) {
            SDL_Surface *icon = SDL_LoadBMP(paths[i]);
            if (icon) return icon;
        }
    }
    char *base = SDL_GetBasePath();
    if (base) {
        const char *relative_paths[] = {
            "../share/nvr-monitor/assets/icons/nvr-monitor-window.bmp",
            "../assets/icons/nvr-monitor-window.bmp"
        };
        for (size_t i = 0; i < sizeof(relative_paths) / sizeof(relative_paths[0]); ++i) {
            char candidate[1024];
            int used = snprintf(candidate, sizeof(candidate), "%s%s", base, relative_paths[i]);
            if (used >= 0 && (size_t)used < sizeof(candidate)) {
                SDL_Surface *icon = SDL_LoadBMP(candidate);
                if (icon) { SDL_free(base); return icon; }
            }
        }
        SDL_free(base);
    }
    return NULL;
}

int nvr_renderer_init(NvrRenderer *view, const char *title, int width, int height,
                      size_t camera_count) {
    if (!view || SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) return -1;
    *view = (NvrRenderer){.fullscreen_camera = -1, .texture_count = camera_count};
    view->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (view->window) {
        SDL_Surface *icon=load_application_icon();
        if(icon){Uint32 transparent=SDL_MapRGB(icon->format,255,0,255);SDL_SetColorKey(icon,SDL_TRUE,transparent);SDL_SetWindowIcon(view->window,icon);SDL_FreeSurface(icon);}
    }
    if (view->window) view->renderer = SDL_CreateRenderer(view->window, -1,
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!view->renderer && view->window) view->renderer = SDL_CreateRenderer(view->window, -1, 0);
    view->textures = calloc(camera_count ? camera_count : 1, sizeof(*view->textures));
    view->serials = calloc(camera_count ? camera_count : 1, sizeof(*view->serials));
    if (!view->window || !view->renderer || !view->textures || !view->serials) {
        nvr_renderer_destroy(view); return -1;
    }
    return 0;
}

static void draw_camera(NvrRenderer *view, NvrCamera *camera, size_t i, NvrRect area) {
    NvrVideoFrame frame = {0};
    if (nvr_frame_queue_take_latest(&camera->frames, &frame)) {
        int tw = 0, th = 0;
        if (view->textures[i]) SDL_QueryTexture(view->textures[i], NULL, NULL, &tw, &th);
        if (!view->textures[i] || tw != frame.width || th != frame.height) {
            SDL_DestroyTexture(view->textures[i]);
            view->textures[i] = SDL_CreateTexture(view->renderer, SDL_PIXELFORMAT_ARGB8888,
                                                   SDL_TEXTUREACCESS_STREAMING, frame.width, frame.height);
        }
        if (view->textures[i]) {
            SDL_UpdateTexture(view->textures[i], NULL, frame.pixels, frame.pitch);
            view->serials[i] = frame.serial;
        }
        nvr_video_frame_free(&frame);
    }
    if (view->textures[i]) {
        int w, h; SDL_QueryTexture(view->textures[i], NULL, NULL, &w, &h);
        NvrRect fit = nvr_layout_fit(area, w, h);
        SDL_Rect destination = {fit.x, fit.y, fit.width, fit.height};
        SDL_RenderCopy(view->renderer, view->textures[i], NULL, &destination);
    }
    NvrCameraState state = nvr_camera_state(camera);
    camera->displayed_state = state;
    if (state == NVR_CAMERA_ONLINE) SDL_SetRenderDrawColor(view->renderer, 40, 190, 90, 255);
    else if (state == NVR_CAMERA_CONNECTING || state == NVR_CAMERA_RECONNECTING) SDL_SetRenderDrawColor(view->renderer, 240, 170, 30, 255);
    else SDL_SetRenderDrawColor(view->renderer, 210, 50, 50, 255);
    SDL_Rect border = {area.x + 1, area.y + 1, area.width - 2, area.height - 2};
    SDL_RenderDrawRect(view->renderer, &border);
}

void nvr_renderer_draw(NvrRenderer *view, NvrCameraManager *manager) {
    int width, height; SDL_GetRendererOutputSize(view->renderer, &width, &height);
    SDL_SetRenderDrawColor(view->renderer, 14, 17, 22, 255); SDL_RenderClear(view->renderer);
    if (view->fullscreen_camera >= 0 && (size_t)view->fullscreen_camera < manager->count) {
        draw_camera(view, &manager->cameras[view->fullscreen_camera], (size_t)view->fullscreen_camera,
                    (NvrRect){0, 0, width, height});
    } else if (manager->count) {
        NvrRect *areas = malloc(manager->count * sizeof(*areas));
        if (areas) {
            size_t slots = view->layout_slots ? view->layout_slots : manager->count;
            if (slots < manager->count) slots = manager->count;
            nvr_layout_grid_slots(manager->count, slots, width, height - NVR_MENU_HEIGHT,
                                  areas, manager->count);
            for (size_t i = 0; i < manager->count; ++i) areas[i].y += NVR_MENU_HEIGHT;
            for (size_t i = 0; i < manager->count; ++i) draw_camera(view, &manager->cameras[i], i, areas[i]);
            free(areas);
        }
    }
}

void nvr_renderer_toggle_fullscreen_camera(NvrRenderer *view, int index) {
    if (view) view->fullscreen_camera = view->fullscreen_camera == index ? -1 : index;
}
int nvr_renderer_resize_cameras(NvrRenderer *view, size_t count) {
    if (!view) return -1;
    SDL_Texture **textures = calloc(count ? count : 1, sizeof(*textures));
    uint64_t *serials = calloc(count ? count : 1, sizeof(*serials));
    if (!textures || !serials) { free(textures); free(serials); return -1; }
    size_t shared=count<view->texture_count?count:view->texture_count;
    for (size_t i = 0; i < shared; ++i) {
        textures[i] = view->textures[i]; serials[i] = view->serials[i];
    }
    for(size_t i=count;i<view->texture_count;i++)SDL_DestroyTexture(view->textures[i]);
    free(view->textures); free(view->serials);
    view->textures = textures; view->serials = serials; view->texture_count = count;
    return 0;
}
void nvr_renderer_destroy(NvrRenderer *view) {
    if (!view) return;
    if (view->textures) for (size_t i = 0; i < view->texture_count; ++i) SDL_DestroyTexture(view->textures[i]);
    free(view->textures); free(view->serials); SDL_DestroyRenderer(view->renderer); SDL_DestroyWindow(view->window);
    SDL_Quit(); *view = (NvrRenderer){0};
}
