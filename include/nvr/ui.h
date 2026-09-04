#ifndef NVR_UI_H
#define NVR_UI_H

#include <SDL.h>
#include "nvr/database.h"
#include "nvr/renderer.h"

typedef enum { NVR_UI_NONE, NVR_UI_ADD_CAMERA, NVR_UI_LAYOUT } NvrUiPanel;

typedef struct {
    NvrUiPanel panel;
    int active_field;
    int editing_index;
    int confirm_delete;
    char fields[8][256];
    char status[256];
    NvrDatabase *database;
} NvrUi;

void nvr_ui_init(NvrUi *ui, NvrDatabase *database);
int nvr_ui_handle_event(NvrUi *ui, const SDL_Event *event, NvrRenderer *renderer,
                        NvrCameraManager *manager);
void nvr_ui_draw(NvrUi *ui, NvrRenderer *renderer, NvrCameraManager *manager);
int nvr_ui_configure_file(const char *path);
#endif
