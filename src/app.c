#include "nvr/app.h"
#include "nvr/camera_manager.h"
#include "nvr/config.h"
#include "nvr/database.h"
#include "nvr/logger.h"
#include "nvr/renderer.h"
#include "nvr/ui.h"
#include <SDL.h>
#include <libavutil/log.h>
#include <string.h>
#include <unistd.h>

static int has_suffix(const char *value,const char *suffix){size_t a=strlen(value),b=strlen(suffix);return a>=b&&!strcmp(value+a-b,suffix);}

int nvr_app_run(const char *config_path) {
    /* FFmpeg can include credential-bearing URLs in diagnostics. Our own logs are sanitized. */
    av_log_set_level(AV_LOG_QUIET);
    char database_path[512];
    const char *legacy_path="config/cameras.json";
    if(has_suffix(config_path,".json")){
        size_t length=strlen(config_path)-5;
        if(length+4>=sizeof(database_path))return 1;
        memcpy(database_path,config_path,length);strcpy(database_path+length,".db");legacy_path=config_path;
    }else snprintf(database_path,sizeof(database_path),"%s",config_path);
    int new_database=access(database_path,F_OK)!=0;
    NvrDatabase database;
    char error[256];
    if(nvr_database_open(&database,database_path,error,sizeof(error))){
        nvr_log(NVR_LOG_ERROR,"Banco de perfis: %s",error);return 1;
    }
    NvrConfig config={0};
    if(new_database&&access(legacy_path,F_OK)==0&&nvr_config_load(legacy_path,&config,error,sizeof(error))==0){
        if(nvr_database_replace_all(&database,&config,error,sizeof(error)))nvr_log(NVR_LOG_WARN,"Migração JSON: %s",error);
        else nvr_log(NVR_LOG_INFO,"Perfis migrados de %s para %s",legacy_path,database_path);
    }else if(nvr_database_load(&database,&config,error,sizeof(error))){
        nvr_log(NVR_LOG_ERROR,"Leitura dos perfis: %s",error);nvr_database_close(&database);return 1;
    }
    if(!config.count&&nvr_database_load(&database,&config,error,sizeof(error))){
        nvr_log(NVR_LOG_ERROR,"Leitura dos perfis: %s",error);nvr_database_close(&database);
        return 1;
    }
    NvrCameraManager manager;
    if (nvr_camera_manager_init(&manager, &config)) {
        nvr_log(NVR_LOG_ERROR, "Não foi possível criar as câmeras"); nvr_config_free(&config); nvr_database_close(&database); return 1;
    }
    NvrRenderer renderer;
    if (nvr_renderer_init(&renderer, "NVR Monitor", 1280, 720, manager.count)) {
        nvr_log(NVR_LOG_ERROR, "Não foi possível iniciar o SDL: %s", SDL_GetError());
        nvr_camera_manager_destroy(&manager); nvr_config_free(&config); nvr_database_close(&database); return 1;
    }
    if (nvr_camera_manager_start(&manager)) {
        nvr_log(NVR_LOG_ERROR, "Não foi possível iniciar as threads das câmeras");
        nvr_renderer_destroy(&renderer); nvr_camera_manager_destroy(&manager); nvr_config_free(&config); nvr_database_close(&database); return 1;
    }
    nvr_log(NVR_LOG_INFO, "%zu câmera(s) iniciada(s). Duplo clique maximiza; Esc volta; Q encerra.", manager.count);
    NvrUi ui;
    nvr_ui_init(&ui, &database);
    int running = 1, dirty = 1;
    Uint32 next_frame = SDL_GetTicks();
    while (running) {
        Uint32 now = SDL_GetTicks();
        int timeout = SDL_TICKS_PASSED(now, next_frame) ? 0 : (int)(next_frame - now);
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, timeout)) {
            do {
                running = nvr_ui_handle_event(&ui, &event, &renderer, &manager);
                if (!running) break;
                if (event.type != SDL_MOUSEMOTION) dirty = 1;
            } while (SDL_PollEvent(&event));
        }
        if (!running) break;
        Uint32 flags = SDL_GetWindowFlags(renderer.window);
        int visible = !(flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
        for (size_t i = 0; i < manager.count; ++i) {
            NvrCamera *camera = &manager.cameras[i];
            int displayed = visible && (renderer.fullscreen_camera < 0 ||
                                       (size_t)renderer.fullscreen_camera == i);
            atomic_store(&camera->display_requested, displayed);
            if (displayed) {
                pthread_mutex_lock(&camera->frames.mutex);
                if (camera->frames.count) dirty = 1;
                pthread_mutex_unlock(&camera->frames.mutex);
                if (camera->displayed_state != nvr_camera_state(camera)) dirty = 1;
            }
        }
        now = SDL_GetTicks();
        if (!SDL_TICKS_PASSED(now, next_frame)) continue;
        /* Explicit cap also works when VSync is absent or the monitor is 144 Hz.
           Losing keyboard focus does not hide a camera on another monitor. */
        if (visible && dirty) {
            nvr_renderer_draw(&renderer, &manager);
            nvr_ui_draw(&ui, &renderer, &manager);
            SDL_RenderPresent(renderer.renderer);
            dirty = 0;
        }
        next_frame = SDL_GetTicks() + (visible ? 40 : 250);
    }
    nvr_camera_manager_stop(&manager);
    nvr_renderer_destroy(&renderer);
    nvr_camera_manager_destroy(&manager);
    nvr_config_free(&config);
    nvr_database_close(&database);
    return 0;
}
