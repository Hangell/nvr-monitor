#include "nvr/ui.h"
#include "nvr/layout.h"
#include "test_temp_file.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void click(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager, int x, int y) {
    SDL_Event event = {.type = SDL_MOUSEBUTTONDOWN};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x = x;
    event.button.y = y;
    assert(nvr_ui_handle_event(ui, &event, view, manager) == 1);
}

static void key(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager, SDL_Keycode code) {
    SDL_Event event = {.type = SDL_KEYDOWN};
    event.key.keysym.sym = code;
    assert(nvr_ui_handle_event(ui, &event, view, manager) == 1);
}

static void type(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager, const char *value) {
    while (*value) {
        SDL_Event event = {.type = SDL_TEXTINPUT};
        size_t length = strlen(value);
        if (length >= sizeof(event.text.text)) length = sizeof(event.text.text)-1;
        memcpy(event.text.text, value, length);
        event.text.text[length] = '\0';
        assert(nvr_ui_handle_event(ui, &event, view, manager) == 1);
        value += length;
    }
}

static void clear(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    while (ui->fields[ui->active_field][0]) key(ui, view, manager, SDLK_BACKSPACE);
}

static void submit(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    const int transport = (int)(sizeof(ui->fields)/sizeof(ui->fields[0]))-1;
    while (ui->active_field != transport) key(ui, view, manager, SDLK_TAB);
    key(ui, view, manager, SDLK_RETURN);
}

static void capture(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager, const char *suffix) {
    const char *base = getenv("NVR_UI_CAPTURE");
    if (!base || !base[0]) return;
    nvr_renderer_draw(view, manager);
    nvr_ui_draw(ui, view, manager);
    int width, height;
    assert(SDL_GetRendererOutputSize(view->renderer, &width, &height) == 0);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(surface);
    assert(SDL_RenderReadPixels(view->renderer, NULL, surface->format->format,
                               surface->pixels, surface->pitch) == 0);
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s-%s.bmp", base, suffix);
    assert(length > 0 && (size_t)length < sizeof(path));
    assert(SDL_SaveBMP(surface, path) == 0);
    SDL_FreeSurface(surface);
}

static void check_database(NvrDatabase *database, size_t count, const char *host, const char *mac) {
    NvrConfig config = {0};
    char error[256];
    assert(nvr_database_load(database, &config, error, sizeof(error)) == 0);
    assert(config.count == count);
    if (count) {
        assert(strcmp(config.cameras[0].host, host) == 0);
        assert(strcmp(config.cameras[0].mac_address, mac) == 0);
    }
    nvr_config_free(&config);
}

int main(void) {
    char path[NVR_TEST_PATH_MAX], error[256];
    nvr_test_temp_file(path);
    NvrDatabase database;
    assert(nvr_database_open(&database, path, error, sizeof(error)) == 0);
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    NvrRenderer view;
    assert(nvr_renderer_init(&view, "UI test", 1280, 720, 0) == 0);
    NvrCameraManager manager;
    NvrConfig empty = {0};
    assert(nvr_camera_manager_init(&manager, &empty) == 0);
    NvrUi ui;
    nvr_ui_init(&ui, &database);

    /* MAC alone creates a profile through the keyboard, normalized in both stores. */
    click(&ui, &view, &manager, 30, 20);
    assert(ui.panel == NVR_UI_ADD_CAMERA);
    assert(strcmp(ui.fields[8], "udp") == 0);
    type(&ui, &view, &manager, "Entrada");
    key(&ui, &view, &manager, SDLK_TAB); /* Optional IP remains empty. */
    key(&ui, &view, &manager, SDLK_TAB);
    type(&ui, &view, &manager, "AA-BB-CC-DD-EE-F0");
    submit(&ui, &view, &manager);
    assert(ui.panel == NVR_UI_NONE && manager.count == 1 && !manager.started);
    assert(manager.cameras[0].config.host[0] == '\0');
    assert(strcmp(manager.cameras[0].config.mac_address, "aa:bb:cc:dd:ee:f0") == 0);
    check_database(&database, 1, "", "aa:bb:cc:dd:ee:f0");

    /* Editing can replace a MAC profile with an IP profile and clear the saved MAC. */
    click(&ui, &view, &manager, 10, NVR_MENU_HEIGHT+12);
    assert(ui.editing_index == 0 && strcmp(ui.fields[2], "aa:bb:cc:dd:ee:f0") == 0);
    capture(&ui, &view, &manager, "default");
    key(&ui, &view, &manager, SDLK_TAB);
    type(&ui, &view, &manager, "192.168.1.30");
    key(&ui, &view, &manager, SDLK_TAB);
    clear(&ui, &view, &manager);
    submit(&ui, &view, &manager);
    assert(ui.panel == NVR_UI_NONE && manager.count == 1);
    assert(strcmp(manager.cameras[0].config.host, "192.168.1.30") == 0);
    assert(manager.cameras[0].config.mac_address[0] == '\0');
    check_database(&database, 1, "192.168.1.30", "");

    /* Invalid MACs must not silently fall back to the hint IP or modify stores. */
    click(&ui, &view, &manager, 30, 20);
    type(&ui, &view, &manager, "Invalida");
    key(&ui, &view, &manager, SDLK_TAB);
    type(&ui, &view, &manager, "192.168.1.40");
    key(&ui, &view, &manager, SDLK_TAB);
    type(&ui, &view, &manager, "not-a-mac");
    submit(&ui, &view, &manager);
    assert(ui.panel == NVR_UI_ADD_CAMERA && strstr(ui.status, "MAC INVALIDO"));
    assert(manager.count == 1);
    key(&ui, &view, &manager, SDLK_TAB); /* Wrap transport -> name. */
    key(&ui, &view, &manager, SDLK_TAB);
    clear(&ui, &view, &manager);
    key(&ui, &view, &manager, SDLK_TAB);
    clear(&ui, &view, &manager);
    submit(&ui, &view, &manager);
    assert(ui.panel == NVR_UI_ADD_CAMERA && strstr(ui.status, "IP OU MAC"));
    check_database(&database, 1, "192.168.1.30", "");

    /* Resizing keeps keyboard focus visible and every row can be reached. */
    SDL_SetWindowSize(view.window, 640, 420);
    SDL_Event resize = {.type = SDL_WINDOWEVENT};
    resize.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    assert(nvr_ui_handle_event(&ui, &resize, &view, &manager) == 1);
    assert(ui.active_field == 8 && ui.first_visible_field > 0);
    capture(&ui, &view, &manager, "short");
    SDL_Event reverse_tab = {.type = SDL_KEYDOWN};
    reverse_tab.key.keysym.sym = SDLK_TAB;
    reverse_tab.key.keysym.mod = KMOD_SHIFT;
    assert(nvr_ui_handle_event(&ui, &reverse_tab, &view, &manager) == 1);
    assert(ui.active_field == 7);
    while (ui.active_field) key(&ui, &view, &manager, SDLK_UP);
    assert(ui.first_visible_field == 0);
    key(&ui, &view, &manager, SDLK_UP);
    assert(ui.active_field == 8 && ui.first_visible_field > 0);
    key(&ui, &view, &manager, SDLK_SPACE);
    assert(strcmp(ui.fields[8], "tcp") == 0);
    key(&ui, &view, &manager, SDLK_ESCAPE);

    /* Detached cameras retain their queue and leave the remaining grid usable. */
    SDL_SetWindowSize(view.window, 1280, 720);
    NvrCameraConfig second = manager.cameras[0].config;
    strcpy(second.name, "Second camera");
    assert(nvr_camera_manager_add(&manager, &second) == 0);
    assert(nvr_renderer_resize_cameras(&view, manager.count) == 0);
    NvrRect areas[9];
    nvr_renderer_camera_areas(&view, manager.count, areas);
    assert(areas[0].height == 720 - NVR_MENU_HEIGHT);
    click(&ui, &view, &manager, areas[0].x+areas[0].width-30, 700);
    assert(manager.cameras[0].grid_main_stream);
    click(&ui, &view, &manager, areas[0].x+areas[0].width-110, 700);
    assert(view.detached[0] && !view.detached[1]);
    nvr_renderer_camera_areas(&view, manager.count, areas);
    assert(areas[0].width == 0 && areas[1].width == 1280);
    Uint32 child_id = SDL_GetWindowID(view.detached[0]->window);
    SDL_Event child_key = {.type = SDL_KEYDOWN};
    child_key.key.windowID = child_id;
    child_key.key.keysym.sym = SDLK_q;
    assert(nvr_ui_handle_event(&ui, &child_key, &view, &manager) == 1);
    assert(view.detached[0]);
    SDL_MinimizeWindow(view.window);
    assert(nvr_renderer_camera_visible(&view, 0));
    SDL_RestoreWindow(view.window);
    const uint8_t pixels[16] = {0};
    assert(nvr_frame_queue_push(&manager.cameras[0].frames, pixels, 2, 2, 8) == 0);
    nvr_renderer_draw(&view, &manager);
    nvr_ui_draw(&ui, &view, &manager);
    assert(view.detached[0]->serials[0] > 0);
    assert(!view.textures[0]);
    capture(&ui, &view, &manager, "detached-grid");
    SDL_Event child_close = {.type = SDL_WINDOWEVENT};
    child_close.window.windowID = child_id;
    child_close.window.event = SDL_WINDOWEVENT_CLOSE;
    assert(nvr_ui_handle_event(&ui, &child_close, &view, &manager) == 1);
    assert(!view.detached[0]);
    nvr_renderer_camera_areas(&view, manager.count, areas);
    assert(areas[0].width == 640 && areas[1].x == 640);
    assert(SDL_GetWindowID(view.window));
    capture(&ui, &view, &manager, "overlay-grid");
    assert(nvr_renderer_detach(&view, 1, second.name) == 0);
    assert(nvr_renderer_detach(&view, 0, "First camera") == 0);
    nvr_renderer_camera_areas(&view, manager.count, areas);
    assert(!areas[0].width && !areas[1].width);
    child_key.key.windowID = SDL_GetWindowID(view.detached[1]->window);
    child_key.key.keysym.sym = SDLK_ESCAPE;
    assert(nvr_ui_handle_event(&ui, &child_key, &view, &manager) == 1);
    assert(view.detached[0] && !view.detached[1]);

    /* Settings persist independently of camera profiles and reclaim menu space. */
    nvr_renderer_attach(&view, 0);
    click(&ui, &view, &manager, 350, 20);
    assert(ui.panel == NVR_UI_SETTINGS);
    capture(&ui, &view, &manager, "settings-pt");
    key(&ui, &view, &manager, SDLK_SPACE);
    key(&ui, &view, &manager, SDLK_TAB);
    key(&ui, &view, &manager, SDLK_SPACE);
    key(&ui, &view, &manager, SDLK_TAB);
    type(&ui, &view, &manager, path); /* A file is not an output directory. */
    key(&ui, &view, &manager, SDLK_RETURN);
    assert(ui.panel == NVR_UI_SETTINGS && strstr(ui.status, "PASTA INVALIDA"));
    assert(!ui.settings.english && !view.hide_menu);
    while (ui.draft_settings.screenshot_directory[0]) key(&ui, &view, &manager, SDLK_BACKSPACE);
    char *directory = SDL_GetBasePath();
    assert(directory);
    type(&ui, &view, &manager, directory);
    key(&ui, &view, &manager, SDLK_TAB);
    type(&ui, &view, &manager, directory);
    key(&ui, &view, &manager, SDLK_RETURN);
    assert(ui.panel == NVR_UI_NONE && ui.settings.english && view.hide_menu);
    nvr_renderer_camera_areas(&view, manager.count, areas);
    assert(areas[0].y == 0 && areas[0].height == 720);
    NvrUi restored;
    nvr_ui_init(&restored, &database);
    assert(restored.settings.english && restored.settings.hide_menu);
    assert(!strcmp(restored.settings.screenshot_directory, directory));
    assert(!strcmp(restored.settings.recording_directory, directory));
    SDL_free(directory);
    check_database(&database, 1, "192.168.1.30", "");
    click(&ui, &view, &manager, 1240, 20); /* Floating button remains reachable. */
    assert(ui.panel == NVR_UI_SETTINGS);
    capture(&ui, &view, &manager, "settings-en");
    key(&ui, &view, &manager, SDLK_SPACE);
    key(&ui, &view, &manager, SDLK_ESCAPE);
    assert(ui.settings.english && view.hide_menu); /* Cancel discards the draft. */
    key(&ui, &view, &manager, SDLK_F10);
    SDL_SetWindowSize(view.window, 420, 360);
    capture(&ui, &view, &manager, "settings-small");
    key(&ui, &view, &manager, SDLK_TAB);
    key(&ui, &view, &manager, SDLK_SPACE);
    key(&ui, &view, &manager, SDLK_RETURN);
    assert(!view.hide_menu);
    nvr_renderer_camera_areas(&view, manager.count, areas);
    assert(areas[0].y == NVR_MENU_HEIGHT);
    click(&ui, &view, &manager, 380, 20);
    assert(ui.panel == NVR_UI_SETTINGS);
    key(&ui, &view, &manager, SDLK_ESCAPE);

    /* Each visor owns its media controls, including detached windows. */
    nvr_renderer_camera_areas(&view, manager.count, areas);
    int media_y = areas[0].y+areas[0].height-50;
    int print_x = areas[0].x+80;
    int record_x = areas[0].x+16;
    click(&ui, &view, &manager, print_x, media_y);
    assert(strstr(ui.media_message[0], "SEM VIDEO"));
    atomic_store(&manager.cameras[0].state, NVR_CAMERA_ONLINE);
    char screenshot_directory[512];
    strcpy(screenshot_directory, ui.settings.screenshot_directory);
    ui.settings.screenshot_directory[0] = '\0';
    click(&ui, &view, &manager, print_x, media_y);
    assert(strstr(ui.media_message[0], "DEFINA A PASTA"));
    assert(atomic_load(&manager.cameras[0].media.screenshot_state) == NVR_MEDIA_IDLE);
    strcpy(ui.settings.screenshot_directory, screenshot_directory);
    click(&ui, &view, &manager, print_x, media_y);
    assert(atomic_load(&manager.cameras[0].media.screenshot_state) == NVR_MEDIA_WAITING);
    nvr_media_frame(&manager.cameras[0].media, pixels, 2, 2, 8);
    assert(atomic_load(&manager.cameras[0].media.screenshot_state) == NVR_MEDIA_SAVED);
    remove(manager.cameras[0].media.screenshot_path);
    click(&ui, &view, &manager, record_x, media_y);
    assert(atomic_load(&manager.cameras[0].media.recording_state) == NVR_MEDIA_WAITING);
    click(&ui, &view, &manager, record_x, media_y);
    assert(atomic_load(&manager.cameras[0].media.recording_state) == NVR_MEDIA_STOPPING);
    nvr_media_end_stream(&manager.cameras[0].media);
    assert(atomic_load(&manager.cameras[0].media.recording_state) == NVR_MEDIA_CANCELLED);
    assert(nvr_renderer_detach(&view, 1, second.name) == 0);
    atomic_store(&manager.cameras[1].state, NVR_CAMERA_ONLINE);
    SDL_Event child_click = {.type = SDL_MOUSEBUTTONDOWN};
    child_click.button.windowID = SDL_GetWindowID(view.detached[1]->window);
    child_click.button.button = SDL_BUTTON_LEFT;
    child_click.button.clicks = 1;
    child_click.button.x = 80;
    child_click.button.y = 490;
    assert(nvr_ui_handle_event(&ui, &child_click, &view, &manager) == 1);
    assert(atomic_load(&manager.cameras[1].media.screenshot_state) == NVR_MEDIA_WAITING);
    assert(atomic_load(&manager.cameras[0].media.screenshot_state) == NVR_MEDIA_SAVED);
    nvr_media_frame(&manager.cameras[1].media, pixels, 2, 2, 8);
    assert(atomic_load(&manager.cameras[1].media.screenshot_state) == NVR_MEDIA_SAVED);
    remove(manager.cameras[1].media.screenshot_path);
    child_click.button.x = 16;
    assert(nvr_ui_handle_event(&ui, &child_click, &view, &manager) == 1);
    assert(atomic_load(&manager.cameras[1].media.recording_state) == NVR_MEDIA_WAITING);
    capture(&ui, &view, &manager, "media-controls");
    assert(nvr_ui_handle_event(&ui, &child_click, &view, &manager) == 1);
    assert(atomic_load(&manager.cameras[1].media.recording_state) == NVR_MEDIA_STOPPING);
    nvr_media_end_stream(&manager.cameras[1].media);

    /* Notices expire without stopping the recording, including tick wraparound. */
    memset(ui.media_message, 0, sizeof(ui.media_message));
    atomic_store(&manager.cameras[0].media.recording_state, NVR_MEDIA_ACTIVE);
    assert(nvr_ui_update_media(&ui, &manager, 1000));
    assert(ui.media_record_notice[0] && ui.media_screenshot_notice[0]);
    capture(&ui, &view, &manager, "centered-notices");
    assert(!nvr_ui_update_media(&ui, &manager, 3999));
    assert(nvr_ui_update_media(&ui, &manager, 4000));
    assert(!ui.media_record_notice[0] && !ui.media_screenshot_notice[0]);
    assert(atomic_load(&manager.cameras[0].media.recording_state) == NVR_MEDIA_ACTIVE);
    assert(!nvr_ui_update_media(&ui, &manager, 5000));
    atomic_store(&manager.cameras[0].media.recording_state, NVR_MEDIA_SAVED);
    assert(nvr_ui_update_media(&ui, &manager, UINT32_MAX-1000));
    assert(ui.media_record_notice[0]);
    assert(!nvr_ui_update_media(&ui, &manager, 1998));
    assert(nvr_ui_update_media(&ui, &manager, 1999));
    assert(!ui.media_record_notice[0]);
    /* Repeated prints refresh their own notice even if they finish between ticks. */
    click(&ui, &view, &manager, print_x, media_y);
    nvr_media_frame(&manager.cameras[0].media, pixels, 2, 2, 8);
    assert(nvr_ui_update_media(&ui, &manager, 6000));
    assert(ui.media_screenshot_notice[0]);
    remove(manager.cameras[0].media.screenshot_path);

    nvr_camera_manager_destroy(&manager);
    nvr_renderer_destroy(&view);
    nvr_database_close(&database);
    remove(path);
    puts("UI MAC, edit, validation and resize checks passed");
    return 0;
}
