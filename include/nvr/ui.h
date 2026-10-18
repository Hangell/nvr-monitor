#ifndef NVR_UI_H
#define NVR_UI_H

#include <SDL.h>
#include "nvr/database.h"
#include "nvr/renderer.h"

typedef enum { NVR_UI_NONE, NVR_UI_ADD_CAMERA, NVR_UI_LAYOUT, NVR_UI_SETTINGS } NvrUiPanel;

typedef struct {
    NvrUiPanel panel;
    int active_field;
    int editing_index;
    int confirm_delete;
    int first_visible_field;
    char fields[9][256];
    char status[256];
    NvrDatabase *database;
    NvrSettings settings;
    NvrSettings draft_settings;
    int settings_field;
    char media_message[9][128];
    Uint32 media_message_until[9];
    int media_record_state[9], media_screenshot_state[9];
    int media_record_notice[9], media_screenshot_notice[9];
    Uint32 media_record_until[9], media_screenshot_until[9];
} NvrUi;

void nvr_ui_init(NvrUi *ui, NvrDatabase *database);
int nvr_ui_handle_event(NvrUi *ui, const SDL_Event *event, NvrRenderer *renderer,
                        NvrCameraManager *manager);
int nvr_ui_update_media(NvrUi *ui, NvrCameraManager *manager, Uint32 now);
void nvr_ui_draw(NvrUi *ui, NvrRenderer *renderer, NvrCameraManager *manager);
int nvr_ui_configure_file(const char *path);
#endif
