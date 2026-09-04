#include "nvr/app.h"
#include "nvr/camera_manager.h"
#include "nvr/config.h"
#include "nvr/logger.h"
#include "nvr/renderer.h"
#include "nvr/ui.h"
#include <SDL.h>
#include <libavutil/log.h>

int nvr_app_run(const char *config_path) {
    /* FFmpeg can include credential-bearing URLs in diagnostics. Our own logs are sanitized. */
    av_log_set_level(AV_LOG_QUIET);
    NvrConfig config;
    char error[256];
    if (nvr_config_load(config_path, &config, error, sizeof(error))) {
        nvr_log(NVR_LOG_ERROR, "Configuração: %s (%s)", error, config_path);
        return 1;
    }
    NvrCameraManager manager;
    if (nvr_camera_manager_init(&manager, &config)) {
        nvr_log(NVR_LOG_ERROR, "Não foi possível criar as câmeras"); nvr_config_free(&config); return 1;
    }
    NvrRenderer renderer;
    if (nvr_renderer_init(&renderer, "NVR Monitor", 1280, 720, manager.count)) {
        nvr_log(NVR_LOG_ERROR, "Não foi possível iniciar o SDL: %s", SDL_GetError());
        nvr_camera_manager_destroy(&manager); nvr_config_free(&config); return 1;
    }
    if (nvr_camera_manager_start(&manager)) {
        nvr_log(NVR_LOG_ERROR, "Não foi possível iniciar as threads das câmeras");
        nvr_renderer_destroy(&renderer); nvr_camera_manager_destroy(&manager); nvr_config_free(&config); return 1;
    }
    nvr_log(NVR_LOG_INFO, "%zu câmera(s) iniciada(s). Duplo clique maximiza; Esc volta; Q encerra.", manager.count);
    NvrUi ui;
    nvr_ui_init(&ui, config_path);
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) running = nvr_ui_handle_event(&ui, &event, &renderer, &manager);
        nvr_renderer_draw(&renderer, &manager);
        nvr_ui_draw(&ui, &renderer, &manager);
        SDL_RenderPresent(renderer.renderer);
        SDL_Delay(1);
    }
    nvr_camera_manager_stop(&manager);
    nvr_renderer_destroy(&renderer);
    nvr_camera_manager_destroy(&manager);
    nvr_config_free(&config);
    return 0;
}
