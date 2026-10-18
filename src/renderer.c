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

static int window_visible(SDL_Window *window) {
    return !(SDL_GetWindowFlags(window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
}

int nvr_renderer_camera_visible(NvrRenderer *view, size_t index) {
    if (index >= 9) return 0;
    if (view->detached[index]) return window_visible(view->detached[index]->window);
    return window_visible(view->window) &&
        (view->fullscreen_camera < 0 || (size_t)view->fullscreen_camera == index);
}

void nvr_renderer_camera_areas(NvrRenderer *view, size_t count, NvrRect areas[9]) {
    int width, height;
    SDL_GetWindowSize(view->window, &width, &height);
    memset(areas, 0, 9 * sizeof(*areas));
    size_t visible = 0;
    for (size_t i = 0; i < count && i < 9; ++i)
        if (!view->detached[i]) ++visible;
    NvrRect grid[9] = {0};
    nvr_layout_grid_slots(visible, view->layout_slots, width, height - NVR_MENU_HEIGHT, grid, 9);
    size_t slot = 0;
    for (size_t i = 0; i < count && i < 9; ++i) {
        if (view->detached[i]) continue;
        areas[i] = grid[slot++];
        areas[i].y += NVR_MENU_HEIGHT;
        if (view->fullscreen_camera == (int)i) areas[i] = (NvrRect){0, 0, width, height};
        else if (view->fullscreen_camera >= 0) areas[i] = (NvrRect){0};
    }
}

int nvr_renderer_detach(NvrRenderer *view, size_t index, const char *title) {
    if (index >= view->texture_count || index >= 9) return -1;
    if (view->detached[index]) return 0;
    NvrRenderer *child = calloc(1, sizeof(*child));
    if (!child) return -1;
    /* Balance SDL subsystem initialization without shutting down the main window. */
    if (nvr_renderer_init(child, title, 960, 540, 1)) { free(child); return -1; }
    view->detached[index] = child;
    SDL_DestroyTexture(view->textures[index]);
    view->textures[index] = NULL;
    if (view->fullscreen_camera == (int)index) view->fullscreen_camera = -1;
    return 0;
}

void nvr_renderer_attach(NvrRenderer *view, size_t index) {
    if (index >= 9 || !view->detached[index]) return;
    nvr_renderer_destroy(view->detached[index]);
    free(view->detached[index]);
    view->detached[index] = NULL;
}

static void prepare_window(NvrRenderer *view) {
    int width, height;
    SDL_GetWindowSize(view->window, &width, &height);
    int pixels_width, pixels_height;
    SDL_GetRendererOutputSize(view->renderer, &pixels_width, &pixels_height);
    if (width > 0 && height > 0)
        SDL_RenderSetScale(view->renderer, (float)pixels_width / width, (float)pixels_height / height);
    SDL_SetRenderDrawBlendMode(view->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(view->renderer, 14, 17, 22, 255);
    SDL_RenderClear(view->renderer);
}

void nvr_renderer_draw(NvrRenderer *view, NvrCameraManager *manager) {
    prepare_window(view);
    NvrRect areas[9];
    nvr_renderer_camera_areas(view, manager->count, areas);
    for (size_t i = 0; i < manager->count && i < 9; ++i) {
        if (!nvr_renderer_camera_visible(view, i)) continue;
        NvrRenderer *child = view->detached[i];
        if (child) {
            int width, height;
            SDL_GetWindowSize(child->window, &width, &height);
            prepare_window(child);
            draw_camera(child, &manager->cameras[i], 0, (NvrRect){0, 0, width, height});
        } else if (areas[i].width > 0 && areas[i].height > 0) {
            draw_camera(view, &manager->cameras[i], i, areas[i]);
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
    for (size_t i = 0; i < 9; ++i) nvr_renderer_attach(view, i);
    if (view->textures) for (size_t i = 0; i < view->texture_count; ++i) SDL_DestroyTexture(view->textures[i]);
    free(view->textures); free(view->serials); SDL_DestroyRenderer(view->renderer); SDL_DestroyWindow(view->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
    *view = (NvrRenderer){0};
}
