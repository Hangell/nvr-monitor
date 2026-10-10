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

    nvr_camera_manager_destroy(&manager);
    nvr_renderer_destroy(&view);
    nvr_database_close(&database);
    remove(path);
    puts("UI MAC, edit, validation and resize checks passed");
    return 0;
}
