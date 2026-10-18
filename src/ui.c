#include "nvr/ui.h"
#include "nvr/config.h"
#include "nvr/layout.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

enum { FIELD_NAME, FIELD_HOST, FIELD_MAC, FIELD_PORT, FIELD_USER, FIELD_PASSWORD,
       FIELD_GRID_PATH, FIELD_MAIN_PATH, FIELD_TRANSPORT, FIELD_COUNT };

static const char *field_labels[FIELD_COUNT] = {
    "NOME", "IP / HOST", "MAC OPCIONAL", "PORTA", "USUARIO", "SENHA",
    "CAMINHO DA GRADE", "CAMINHO PRINCIPAL", "TRANSPORTE"
};

static const char *translate(const NvrUi *ui, const char *value) {
    static const char *labels[][2] = {
        {"CONFIGURACOES", "SETTINGS"},
        {"NOME", "NAME"}, {"MAC OPCIONAL", "OPTIONAL MAC"}, {"PORTA", "PORT"},
        {"USUARIO", "USERNAME"}, {"SENHA", "PASSWORD"},
        {"CAMINHO DA GRADE", "GRID PATH"}, {"CAMINHO PRINCIPAL", "MAIN PATH"},
        {"TRANSPORTE", "TRANSPORT"}, {"SALVAR", "SAVE"}, {"CANCELAR", "CANCEL"},
        {"DELETAR", "DELETE"}, {"CONFIRMAR", "CONFIRM"}, {"DESTACAR", "DETACH"},
        {"VOLTAR", "RETURN"}, {"EDITAR CAMERA", "EDIT CAMERA"},
        {"ADICIONAR CAMERA", "ADD CAMERA"}, {"AUTOMATICO", "AUTOMATIC"},
        {"1 TELA", "1 VIEW"}, {"2 TELAS", "2 VIEWS"}, {"4 TELAS", "4 VIEWS"},
        {"6 TELAS", "6 VIEWS"}, {"8 TELAS", "8 VIEWS"}, {"9 TELAS", "9 VIEWS"},
        {"IDIOMA", "LANGUAGE"}, {"OCULTAR MENU SUPERIOR", "HIDE TOP MENU"},
        {"SIM", "YES"}, {"NAO", "NO"}, {"PASTA DE CAPTURAS", "SCREENSHOT FOLDER"},
        {"PASTA DE GRAVACOES", "RECORDING FOLDER"},
        {"VAZIO: AINDA NAO DEFINIDO", "EMPTY: NOT SET YET"},
        {"PRINT: PNG / REC: VIDEO MKV SEM AUDIO", "PRINT: PNG / REC: MKV VIDEO WITHOUT AUDIO"},
        {"USE PASTAS EXISTENTES. CTRL+V PARA COLAR.", "USE EXISTING FOLDERS. CTRL+V TO PASTE."},
        {"PASTA INVALIDA OU SEM PERMISSAO DE ESCRITA", "INVALID FOLDER OR NO WRITE PERMISSION"},
        {"NAO FOI POSSIVEL SALVAR CONFIGURACOES", "COULD NOT SAVE SETTINGS"},
        {"NAO FOI POSSIVEL CARREGAR CONFIGURACOES", "COULD NOT LOAD SETTINGS"},
        {"PREENCHA NOME, IP OU MAC, PORTA E TRANSPORTE", "ENTER NAME, IP OR MAC, PORT AND TRANSPORT"},
        {"CAMPO EXCEDE O TAMANHO PERMITIDO", "FIELD EXCEEDS MAXIMUM LENGTH"},
        {"MAC INVALIDO. USE AA:BB:CC:DD:EE:FF", "INVALID MAC. USE AA:BB:CC:DD:EE:FF"},
        {"NAO FOI POSSIVEL ATUALIZAR A CAMERA", "COULD NOT UPDATE CAMERA"},
        {"PERFIL DA CAMERA ATUALIZADO", "CAMERA PROFILE UPDATED"},
        {"NAO FOI POSSIVEL ADICIONAR A CAMERA", "COULD NOT ADD CAMERA"},
        {"CAMERA ADICIONADA AO BANCO DE PERFIS", "CAMERA PROFILE ADDED"},
        {"CLIQUE EM CONFIRMAR PARA DELETAR", "CLICK CONFIRM TO DELETE"},
        {"NAO FOI POSSIVEL DELETAR A CAMERA", "COULD NOT DELETE CAMERA"},
        {"CAMERA DELETADA", "CAMERA DELETED"},
        {"LAYOUT AUTOMATICO", "AUTOMATIC LAYOUT"},
        {"NAO FOI POSSIVEL MAXIMIZAR A JANELA", "COULD NOT MAXIMIZE WINDOW"},
        {"NAO FOI POSSIVEL DESTACAR A CAMERA", "COULD NOT DETACH CAMERA"},
        {"DEFINA A PASTA EM CONFIGURACOES (F10)", "SET THE FOLDER IN SETTINGS (F10)"},
        {"CAMERA SEM VIDEO AO VIVO", "CAMERA HAS NO LIVE VIDEO"},
        {"OPERACAO EM ANDAMENTO", "OPERATION IN PROGRESS"},
        {"REC: AGUARDANDO QUADRO CHAVE", "REC: WAITING FOR KEYFRAME"},
        {"REC: GRAVANDO", "REC: RECORDING"},
        {"REC: FINALIZANDO", "REC: FINALIZING"},
        {"REC: SALVO", "REC: SAVED"},
        {"REC: ERRO DE GRAVACAO", "REC: RECORDING ERROR"},
        {"REC: CANCELADO SEM VIDEO", "REC: CANCELLED WITHOUT VIDEO"},
        {"PRINT: AGUARDANDO QUADRO", "PRINT: WAITING FOR FRAME"},
        {"PRINT: SALVO", "PRINT: SAVED"},
        {"PRINT: ERRO AO SALVAR", "PRINT: SAVE ERROR"},
        {"CONFIGURACOES SALVAS", "SETTINGS SAVED"}
    };
    if (!strcmp(value, "+ ADD CAMERA")) return ui->settings.english ? value : "+ CAMERA";
    if (!ui->settings.english) return value;
    for (size_t i = 0; i < sizeof(labels)/sizeof(labels[0]); ++i)
        if (!strcmp(value, labels[i][0])) return labels[i][1];
    return value;
}

/* Fonte bitmap 5x7 para manter a interface sem dependência de SDL_ttf. */
static const unsigned char *glyph(char input) {
    static const unsigned char blank[5] = {0,0,0,0,0};
    static unsigned char fallback[5] = {0x7f,0x41,0x5d,0x41,0x7f};
    static const unsigned char letters[26][5] = {
        {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
        {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
        {0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
        {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},
        {0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
        {0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
        {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},
        {0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},
        {0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
        {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
        {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };
    static const unsigned char digits[10][5] = {
        {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
        {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
        {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e}
    };
    static const unsigned char dot[5]={0,0x60,0x60,0,0}, colon[5]={0,0x36,0x36,0,0};
    static const unsigned char slash[5]={0x20,0x10,0x08,0x04,0x02}, dash[5]={0x08,0x08,0x08,0x08,0x08};
    static const unsigned char under[5]={0x40,0x40,0x40,0x40,0x40}, at[5]={0x3e,0x41,0x5d,0x55,0x1e};
    static const unsigned char star[5]={0x14,0x08,0x3e,0x08,0x14}, percent[5]={0x63,0x13,0x08,0x64,0x63};
    unsigned char c = (unsigned char)toupper((unsigned char)input);
    if (c >= 'A' && c <= 'Z') return letters[c-'A'];
    if (c >= '0' && c <= '9') return digits[c-'0'];
    static const unsigned char plus[5] = {0x08,0x08,0x3e,0x08,0x08};
    if (c == '+') return plus;
    if (c == ' ') return blank;
    if (c == '.') return dot;
    if (c == ':') return colon;
    if (c == '/') return slash;
    if (c == '-') return dash;
    if (c == '_') return under;
    if (c == '@') return at;
    if (c == '*') return star;
    if (c == '%') return percent;
    return fallback;
}

static void text(SDL_Renderer *r, int x, int y, int scale, SDL_Color color, const char *value) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (; *value; ++value, x += 6 * scale) {
        const unsigned char *columns = glyph(*value);
        for (int col = 0; col < 5; ++col) for (int row = 0; row < 7; ++row)
            if (columns[col] & (1u << row)) {
                SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(r, &pixel);
            }
    }
}

static void button(SDL_Renderer *r, SDL_Rect rect, const char *label, int active) {
    SDL_SetRenderDrawColor(r, active ? 36 : 28, active ? 112 : 43, active ? 168 : 55, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 75, 91, 110, 255); SDL_RenderDrawRect(r, &rect);
    text(r, rect.x + 12, rect.y + 10, 2, (SDL_Color){235,240,246,255}, label);
}

static int inside(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

typedef struct {
    SDL_Rect panel;
    SDL_Rect save, cancel, remove;
    int field_x, field_width, rows, notes_y, notes_height, status_y;
} CameraForm;

/* Drawing and input use the same coordinates, including in short windows. */
static CameraForm camera_form(NvrUi *ui, NvrRenderer *view) {
    int width, height;
    SDL_GetWindowSize(view->window, &width, &height);
    int panel_width = width - 24, panel_height = height - NVR_MENU_HEIGHT - 24;
    if (panel_width > 850) panel_width = 850;
    if (panel_width < 1) panel_width = 1;
    if (panel_height > 552) panel_height = 552;
    if (panel_height < 1) panel_height = 1;
    CameraForm form = {.panel = {(width-panel_width)/2, NVR_MENU_HEIGHT+12, panel_width, panel_height}};
    form.notes_height = panel_height >= 480 ? 80 : panel_height >= 320 ? 48 : 0;
    form.rows = (panel_height - 116 - form.notes_height) / 36;
    if (form.rows < 1) form.rows = 1;
    if (form.rows > FIELD_COUNT) form.rows = FIELD_COUNT;
    if (ui->first_visible_field > FIELD_COUNT - form.rows) ui->first_visible_field = FIELD_COUNT - form.rows;
    if (ui->first_visible_field < 0) ui->first_visible_field = 0;
    int label_width = panel_width >= 400 ? 146 : 116;
    form.field_x = form.panel.x + label_width;
    form.field_width = panel_width - label_width - 14;
    if (form.field_width < 1) form.field_width = 1;
    int button_width = (panel_width - 48) / 3;
    if (button_width > 150) button_width = 150;
    if (button_width < 1) button_width = 1;
    int button_y = form.panel.y + 42 + form.rows*36 + 10;
    form.save = (SDL_Rect){form.panel.x+14, button_y, button_width, 36};
    form.cancel = (SDL_Rect){form.save.x+button_width+10, button_y, button_width, 36};
    form.remove = (SDL_Rect){form.cancel.x+button_width+10, button_y, button_width, 36};
    form.notes_y = button_y + 48;
    form.status_y = form.notes_y + form.notes_height + 4;
    return form;
}

static SDL_Rect camera_field(CameraForm form, int row) {
    return (SDL_Rect){form.field_x, form.panel.y+42+row*36, form.field_width, 28};
}

static void show_active_field(NvrUi *ui, NvrRenderer *view) {
    CameraForm form = camera_form(ui, view);
    if (ui->active_field < ui->first_visible_field) ui->first_visible_field = ui->active_field;
    else if (ui->active_field >= ui->first_visible_field + form.rows)
        ui->first_visible_field = ui->active_field - form.rows + 1;
}

static void clipped_text(SDL_Renderer *renderer, SDL_Rect bounds, int scale,
                         SDL_Color color, const char *value) {
    if (bounds.w <= 0 || bounds.h <= 0) return;
    SDL_Rect previous;
    SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous);
    SDL_RenderSetClipRect(renderer, &bounds);
    text(renderer, bounds.x, bounds.y, scale, color, value);
    SDL_RenderSetClipRect(renderer, clipped ? &previous : NULL);
}

static void form_button(SDL_Renderer *renderer, SDL_Rect rect, const char *label, int style) {
    int danger = style > 0;
    SDL_SetRenderDrawColor(renderer, danger ? 150 : style < 0 ? 28 : 36,
                           danger ? 35 : style < 0 ? 43 : 112,
                           danger ? 42 : style < 0 ? 55 : 168, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, danger ? 230 : 75, danger ? 85 : 91, danger ? 90 : 110, 255);
    SDL_RenderDrawRect(renderer, &rect);
    int scale = (int)strlen(label)*12 + 24 <= rect.w ? 2 : 1;
    clipped_text(renderer, (SDL_Rect){rect.x+8, rect.y+10, rect.w-16, 16}, scale,
                 (SDL_Color){235,240,246,255}, label);
}

static void form_notes(NvrUi *ui, SDL_Renderer *renderer, CameraForm form) {
    const char *notes = ui->settings.english ?
        "FOR DYNAMIC IP / DHCP, WE RECOMMEND MAC. IP / HOST IS OPTIONAL WITH MAC. "
        "CAMERA AND MONITOR MUST BE ON THE SAME LOCAL NETWORK. "
        "RESERVE DHCP BY THE CAMERA WI-FI OR ETHERNET MAC IN YOUR ROUTER. "
        "YOOSEE: ENABLE NVR IN THE APP AND SELECT UDP." : "PARA IP DINAMICO / DHCP, RECOMENDAMOS MAC. COM MAC, IP / HOST E OPCIONAL. "
        "CAMERA E MONITOR DEVEM ESTAR NA MESMA REDE LOCAL. "
        "NO MODEM, RESERVE DHCP PELO MAC DA INTERFACE WI-FI OU CABO DA CAMERA. "
        "YOOSEE: ATIVE NVR NO APP E SELECIONE UDP.";
    int columns = (form.panel.w-28)/6, y = form.notes_y;
    if (columns < 1) return;
    while (*notes && y+7 <= form.notes_y+form.notes_height) {
        size_t length = strlen(notes), take = length > (size_t)columns ? (size_t)columns : length;
        if (take < length) {
            size_t boundary = take;
            while (boundary && notes[boundary] != ' ') boundary--;
            if (boundary) take = boundary;
        }
        char line[256];
        if (take >= sizeof(line)) take = sizeof(line)-1;
        memcpy(line, notes, take); line[take] = '\0';
        text(renderer, form.panel.x+14, y, 1, (SDL_Color){161,176,194,255}, line);
        notes += take;
        while (*notes == ' ') notes++;
        y += 14;
    }
}

void nvr_ui_init(NvrUi *ui, NvrDatabase *database) {
    memset(ui, 0, sizeof(*ui));
    if (nvr_database_load_settings(database, &ui->settings))
        snprintf(ui->status, sizeof(ui->status), "NAO FOI POSSIVEL CARREGAR CONFIGURACOES");
    ui->active_field = FIELD_NAME;ui->editing_index=-1;ui->database=database;
}

static void open_add(NvrUi *ui) {
    memset(ui->fields, 0, sizeof(ui->fields));
    strcpy(ui->fields[FIELD_PORT], "554"); strcpy(ui->fields[FIELD_GRID_PATH], "/onvif2");
    strcpy(ui->fields[FIELD_MAIN_PATH], "/onvif1"); strcpy(ui->fields[FIELD_TRANSPORT], "udp");
    ui->panel = NVR_UI_ADD_CAMERA; ui->active_field = FIELD_NAME;ui->editing_index=-1;ui->confirm_delete=0;ui->first_visible_field=0; ui->status[0] = '\0';
    SDL_StartTextInput();
}

static void open_edit(NvrUi *ui,const NvrCameraConfig *camera,size_t index){
    memset(ui->fields,0,sizeof(ui->fields));
    snprintf(ui->fields[FIELD_NAME],sizeof(ui->fields[0]),"%s",camera->name);
    snprintf(ui->fields[FIELD_HOST],sizeof(ui->fields[0]),"%s",camera->host);
    snprintf(ui->fields[FIELD_MAC],sizeof(ui->fields[0]),"%s",camera->mac_address);
    snprintf(ui->fields[FIELD_PORT],sizeof(ui->fields[0]),"%u",camera->port);
    snprintf(ui->fields[FIELD_USER],sizeof(ui->fields[0]),"%s",camera->username);
    snprintf(ui->fields[FIELD_PASSWORD],sizeof(ui->fields[0]),"%s",camera->password);
    snprintf(ui->fields[FIELD_GRID_PATH],sizeof(ui->fields[0]),"%s",camera->grid_path);
    snprintf(ui->fields[FIELD_MAIN_PATH],sizeof(ui->fields[0]),"%s",camera->main_path);
    snprintf(ui->fields[FIELD_TRANSPORT],sizeof(ui->fields[0]),"%s",nvr_transport_name(camera->transport));
    ui->editing_index=(int)index;ui->confirm_delete=0;ui->first_visible_field=0;ui->active_field=FIELD_NAME;ui->panel=NVR_UI_ADD_CAMERA;ui->status[0]='\0';SDL_StartTextInput();
}

static void close_panel(NvrUi *ui) { ui->panel = NVR_UI_NONE; SDL_StopTextInput(); }

typedef struct {
    SDL_Rect panel, language, menu, screen, record, save, cancel;
} SettingsForm;

static SettingsForm settings_form(NvrRenderer *view) {
    int width, height;
    SDL_GetWindowSize(view->window, &width, &height);
    int panel_width = width - 24;
    if (panel_width > 720) panel_width = 720;
    if (panel_width < 1) panel_width = 1;
    int panel_height = height - 24;
    if (panel_height > 348) panel_height = 348;
    if (panel_height < 1) panel_height = 1;
    SettingsForm form = {.panel = {(width-panel_width)/2, (height-panel_height)/2, panel_width, panel_height}};
    int x = form.panel.x+12, y = form.panel.y+36, w = panel_width-24;
    form.language = (SDL_Rect){x, y, w/2-4, 28};
    form.menu = (SDL_Rect){x+w/2+4, y, w/2-4, 28};
    form.screen = (SDL_Rect){x, y+62, w, 28};
    form.record = (SDL_Rect){x, y+120, w, 28};
    form.save = (SDL_Rect){x, y+212, w/2-4, 32};
    form.cancel = (SDL_Rect){x+w/2+4, y+212, w/2-4, 32};
    return form;
}

static void open_settings(NvrUi *ui) {
    ui->draft_settings = ui->settings;
    ui->settings_field = 0;
    ui->panel = NVR_UI_SETTINGS;
    ui->status[0] = '\0';
    SDL_StartTextInput();
}

static int valid_output_directory(const char *path) {
    if (!path[0]) return 1;
#ifdef _WIN32
    int absolute = (isalpha((unsigned char)path[0]) && path[1] == ':' &&
                    (path[2] == '/' || path[2] == '\\')) ||
                   (path[0] == '\\' && path[1] == '\\');
#else
    int absolute = path[0] == '/';
#endif
    struct stat info;
    return absolute && stat(path, &info) == 0 && S_ISDIR(info.st_mode) && access(path, W_OK) == 0;
}

static void save_settings(NvrUi *ui, NvrRenderer *view) {
    if (!valid_output_directory(ui->draft_settings.screenshot_directory) ||
        !valid_output_directory(ui->draft_settings.recording_directory)) {
        snprintf(ui->status, sizeof(ui->status), "PASTA INVALIDA OU SEM PERMISSAO DE ESCRITA");
        return;
    }
    if (nvr_database_save_settings(ui->database, &ui->draft_settings)) {
        snprintf(ui->status, sizeof(ui->status), "NAO FOI POSSIVEL SALVAR CONFIGURACOES");
        return;
    }
    ui->settings = ui->draft_settings;
    view->hide_menu = ui->settings.hide_menu;
    close_panel(ui);
    snprintf(ui->status, sizeof(ui->status), "CONFIGURACOES SALVAS");
}

static void append_settings_text(NvrUi *ui, const char *value) {
    if (ui->settings_field < 2) return;
    char *field = ui->settings_field == 2 ? ui->draft_settings.screenshot_directory : ui->draft_settings.recording_directory;
    size_t have = strlen(field), add = strlen(value);
    if (have+add < sizeof(ui->draft_settings.screenshot_directory) && !strchr(value, '\n') && !strchr(value, '\r'))
        memcpy(field+have, value, add+1);
}

static void settings_event(NvrUi *ui, NvrRenderer *view, const SDL_Event *event) {
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_ESCAPE) { close_panel(ui); return; }
        if (key == SDLK_TAB) ui->settings_field = (ui->settings_field + ((event->key.keysym.mod & KMOD_SHIFT) ? 3 : 1)) % 4;
        else if (key == SDLK_RETURN) save_settings(ui, view);
        else if (key == SDLK_SPACE && ui->settings_field < 2) {
            if (ui->settings_field == 0) ui->draft_settings.english = !ui->draft_settings.english;
            else ui->draft_settings.hide_menu = !ui->draft_settings.hide_menu;
        } else if (key == SDLK_BACKSPACE && ui->settings_field >= 2) {
            char *field = ui->settings_field == 2 ? ui->draft_settings.screenshot_directory : ui->draft_settings.recording_directory;
            size_t length = strlen(field);
            if (length) {
                do { --length; } while (length && ((unsigned char)field[length] & 0xc0) == 0x80);
                field[length] = '\0';
            }
        } else if (key == SDLK_v && (event->key.keysym.mod & KMOD_CTRL)) {
            char *value = SDL_GetClipboardText();
            if (value) { append_settings_text(ui, value); SDL_free(value); }
        }
    } else if (event->type == SDL_TEXTINPUT) append_settings_text(ui, event->text.text);
    else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        SettingsForm form = settings_form(view);
        int x = event->button.x, y = event->button.y;
        if (inside(x, y, form.language)) { ui->settings_field = 0; ui->draft_settings.english = !ui->draft_settings.english; }
        else if (inside(x, y, form.menu)) { ui->settings_field = 1; ui->draft_settings.hide_menu = !ui->draft_settings.hide_menu; }
        else if (inside(x, y, form.screen)) ui->settings_field = 2;
        else if (inside(x, y, form.record)) ui->settings_field = 3;
        else if (inside(x, y, form.save)) save_settings(ui, view);
        else if (inside(x, y, form.cancel)) close_panel(ui);
    }
}

static void draw_settings(NvrUi *ui, NvrRenderer *view) {
    SettingsForm form = settings_form(view);
    SDL_Renderer *renderer = view->renderer;
    SDL_SetRenderDrawColor(renderer, 24,31,40,250);
    SDL_RenderFillRect(renderer, &form.panel);
    SDL_RenderSetClipRect(renderer, &form.panel);
    text(renderer, form.panel.x+12, form.panel.y+12, 2, (SDL_Color){240,244,248,255}, translate(ui, "CONFIGURACOES"));
    char label[96];
    snprintf(label, sizeof(label), "%s: %s", translate(ui, "IDIOMA"), ui->draft_settings.english ? "ENGLISH" : "PORTUGUES");
    form_button(renderer, form.language, label, ui->settings_field == 0 ? 0 : -1);
    snprintf(label, sizeof(label), "%s: %s", translate(ui, "OCULTAR MENU SUPERIOR"), translate(ui, ui->draft_settings.hide_menu ? "SIM" : "NAO"));
    form_button(renderer, form.menu, label, ui->settings_field == 1 ? 0 : -1);
    SDL_Rect fields[] = {form.screen, form.record};
    const char *values[] = {ui->draft_settings.screenshot_directory, ui->draft_settings.recording_directory};
    for (int i = 0; i < 2; ++i) {
        SDL_Rect field = fields[i];
        text(renderer, field.x, field.y-16, 1, (SDL_Color){190,205,220,255}, translate(ui, i ? "PASTA DE GRAVACOES" : "PASTA DE CAPTURAS"));
        SDL_SetRenderDrawColor(renderer, 12,20,30,255);
        SDL_RenderFillRect(renderer, &field);
        SDL_SetRenderDrawColor(renderer, ui->settings_field == i+2 ? 80 : 45,110,150,255);
        SDL_RenderDrawRect(renderer, &field);
        const char *value = values[i][0] ? values[i] : translate(ui, "VAZIO: AINDA NAO DEFINIDO");
        size_t visible = field.w > 16 ? (size_t)(field.w-16)/6 : 0;
        if (ui->settings_field == i+2 && strlen(value) > visible) value += strlen(value)-visible;
        clipped_text(renderer, (SDL_Rect){field.x+8, field.y+10, field.w-16,8}, 1, (SDL_Color){235,240,246,255}, value);
    }
    text(renderer, form.screen.x, form.record.y+42, 1, (SDL_Color){175,190,205,255}, translate(ui, "USE PASTAS EXISTENTES. CTRL+V PARA COLAR."));
    text(renderer, form.screen.x, form.record.y+58, 1, (SDL_Color){175,190,205,255}, translate(ui, "PRINT: PNG / REC: VIDEO MKV SEM AUDIO"));
    form_button(renderer, form.save, translate(ui, "SALVAR"), 0);
    form_button(renderer, form.cancel, translate(ui, "CANCELAR"), -1);
    clipped_text(renderer, (SDL_Rect){form.save.x, form.save.y+46, form.panel.w-24,16}, 1,
                 (SDL_Color){240,140,100,255}, translate(ui, ui->status));
    SDL_RenderSetClipRect(renderer, NULL);
}

static int save_camera(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    char *end = NULL; long port = strtol(ui->fields[FIELD_PORT], &end, 10);
    if (!ui->fields[FIELD_NAME][0] || (!ui->fields[FIELD_HOST][0] && !ui->fields[FIELD_MAC][0]) ||
        end == ui->fields[FIELD_PORT] || *end ||
        port < 1 || port > 65535 ||
        (strcmp(ui->fields[FIELD_TRANSPORT], "udp") && strcmp(ui->fields[FIELD_TRANSPORT], "tcp"))) {
        snprintf(ui->status, sizeof(ui->status), "PREENCHA NOME, IP OU MAC, PORTA E TRANSPORTE"); return -1;
    }
    NvrCameraConfig camera = {.port=(unsigned short)port};
#define COPY_FIELD(destination, source) do { if (strlen(source) >= sizeof(destination)) { snprintf(ui->status,sizeof(ui->status),"CAMPO EXCEDE O TAMANHO PERMITIDO"); return -1; } strcpy(destination, source); } while (0)
    COPY_FIELD(camera.name, ui->fields[FIELD_NAME]); COPY_FIELD(camera.host, ui->fields[FIELD_HOST]);
    COPY_FIELD(camera.username, ui->fields[FIELD_USER]); COPY_FIELD(camera.grid_path, ui->fields[FIELD_GRID_PATH]);
    COPY_FIELD(camera.main_path, ui->fields[FIELD_MAIN_PATH]); COPY_FIELD(camera.password,ui->fields[FIELD_PASSWORD]);
#undef COPY_FIELD
    if (ui->fields[FIELD_MAC][0] &&
        !nvr_mac_normalize(ui->fields[FIELD_MAC], camera.mac_address, sizeof(camera.mac_address))) {
        snprintf(ui->status, sizeof(ui->status), "MAC INVALIDO. USE AA:BB:CC:DD:EE:FF"); return -1;
    }
    camera.transport = !strcmp(ui->fields[FIELD_TRANSPORT], "udp") ? NVR_TRANSPORT_UDP : NVR_TRANSPORT_TCP;
    char error[160];
    if(ui->editing_index>=0){
        size_t index=(size_t)ui->editing_index;
        if(!camera.password[0])snprintf(camera.password_env,sizeof(camera.password_env),"%s",manager->cameras[index].config.password_env);
        if(nvr_camera_manager_update(manager,index,&camera)||nvr_database_update_camera(ui->database,index,&camera,error,sizeof(error))){
            snprintf(ui->status,sizeof(ui->status),"NAO FOI POSSIVEL ATUALIZAR A CAMERA");return -1;
        }
        close_panel(ui);snprintf(ui->status,sizeof(ui->status),"PERFIL DA CAMERA ATUALIZADO");return 0;
    }
    if (nvr_renderer_resize_cameras(view, manager->count + 1) || nvr_camera_manager_add(manager, &camera)||
        nvr_database_insert_camera(ui->database,&camera,error,sizeof(error))) {
        snprintf(ui->status, sizeof(ui->status), "NAO FOI POSSIVEL ADICIONAR A CAMERA"); return -1;
    }
    close_panel(ui);
    snprintf(ui->status, sizeof(ui->status), "CAMERA ADICIONADA AO BANCO DE PERFIS");
    return 0;
}

static int delete_camera(NvrUi *ui,NvrRenderer *view,NvrCameraManager *manager){
    if(ui->editing_index<0)return -1;
    size_t index=(size_t)ui->editing_index;char error[160];
    if(!ui->confirm_delete){ui->confirm_delete=1;snprintf(ui->status,sizeof(ui->status),"CLIQUE EM CONFIRMAR PARA DELETAR");return 0;}
    if(nvr_camera_manager_remove(manager,index)||nvr_database_delete_camera(ui->database,index,error,sizeof(error))){
        snprintf(ui->status,sizeof(ui->status),"NAO FOI POSSIVEL DELETAR A CAMERA");return -1;
    }
    nvr_renderer_attach(view, index);
    for (size_t i = index; i + 1 < 9; ++i) view->detached[i] = view->detached[i+1];
    view->detached[8] = NULL;
    view->fullscreen_camera = -1;
    for(size_t i=0;i<view->texture_count;i++){SDL_DestroyTexture(view->textures[i]);view->textures[i]=NULL;}
    nvr_renderer_resize_cameras(view,manager->count);close_panel(ui);snprintf(ui->status,sizeof(ui->status),"CAMERA DELETADA");return 0;
}

static SDL_Rect quality_button(NvrRect area) {
    int width = area.width < 58 ? area.width : 58;
    return (SDL_Rect){area.x + area.width - width - 6,
                      area.y + area.height - 30, width, 24};
}

static SDL_Rect detach_button(NvrRect area) {
    SDL_Rect quality = quality_button(area);
    return (SDL_Rect){quality.x - 88, quality.y, 82, 24};
}

static SDL_Rect media_button(NvrRect area, int screenshot) {
    int width = (area.width-18)/2;
    if (width > 60) width = 60;
    if (width < 1) width = 1;
    return (SDL_Rect){area.x+6+(screenshot ? width+6 : 0), area.y+area.height-58, width, 24};
}

static int handle_media_click(NvrUi *ui, NvrCamera *camera, size_t index,
                               NvrRect area, const SDL_Event *event) {
    int screenshot = inside(event->button.x, event->button.y, media_button(area, 1));
    if (!screenshot && !inside(event->button.x, event->button.y, media_button(area, 0))) return 0;
    if (event->button.clicks != 1) return 1;
    int state = atomic_load(&camera->media.recording_state);
    const char *message = NULL;
    if (!screenshot && (state == NVR_MEDIA_WAITING || state == NVR_MEDIA_ACTIVE || state == NVR_MEDIA_STOPPING)) {
        nvr_media_stop_recording(&camera->media);
    } else if (nvr_camera_state(camera) != NVR_CAMERA_ONLINE) {
        message = "CAMERA SEM VIDEO AO VIVO";
    } else {
        const char *directory = screenshot ? ui->settings.screenshot_directory : ui->settings.recording_directory;
        if (!directory[0]) message = "DEFINA A PASTA EM CONFIGURACOES (F10)";
        else if (!valid_output_directory(directory)) message = "PASTA INVALIDA OU SEM PERMISSAO DE ESCRITA";
        else {
            int result = screenshot ? nvr_media_request_screenshot(&camera->media, directory) :
                                      nvr_media_request_recording(&camera->media, directory);
            if (result) message = "OPERACAO EM ANDAMENTO";
            else if (screenshot) ui->media_screenshot_state[index] = NVR_MEDIA_IDLE;
            else ui->media_record_state[index] = NVR_MEDIA_IDLE;
        }
    }
    snprintf(ui->media_message[index], sizeof(ui->media_message[index]), "%s", message ? message : "");
    ui->media_message_until[index] = SDL_GetTicks()+3000;
    return 1;
}

static void toggle_quality(NvrCamera *camera) {
    camera->grid_main_stream = !atomic_load(&camera->main_stream_requested);
    nvr_camera_request_main_stream(camera, camera->grid_main_stream);
}

static void overlay_button(NvrRenderer *view, SDL_Rect rect, const char *label) {
    int x, y;
    SDL_GetMouseState(&x, &y);
    int hover = SDL_GetMouseFocus() == view->window && inside(x, y, rect);
    SDL_SetRenderDrawBlendMode(view->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(view->renderer, 12, 18, 24, hover ? 210 : 70);
    SDL_RenderFillRect(view->renderer, &rect);
    clipped_text(view->renderer, (SDL_Rect){rect.x+6, rect.y+8, rect.w-12, 8}, 1,
                 (SDL_Color){240,244,248,hover ? 255 : 150}, label);
}

static void leave_fullscreen(NvrRenderer *view, NvrCameraManager *manager) {
    int old = view->fullscreen_camera;
    if (old >= 0 && (size_t)old < manager->count) {
        nvr_renderer_toggle_fullscreen_camera(view, old);
        nvr_camera_request_main_stream(&manager->cameras[old], manager->cameras[old].grid_main_stream);
    }
}

static void select_layout(NvrUi *ui, NvrRenderer *view, size_t slots) {
    view->layout_slots = slots; close_panel(ui);
    snprintf(ui->status, sizeof(ui->status), slots ? (ui->settings.english ? "LAYOUT %zu SELECTED" : "LAYOUT %zu SELECIONADO") : translate(ui, "LAYOUT AUTOMATICO"), slots);
}

int nvr_ui_handle_event(NvrUi *ui, const SDL_Event *event, NvrRenderer *view,
                        NvrCameraManager *manager) {
    if (event->type == SDL_QUIT) return 0;
    Uint32 window_id = 0;
    switch (event->type) {
        case SDL_WINDOWEVENT: window_id = event->window.windowID; break;
        case SDL_KEYDOWN: case SDL_KEYUP: window_id = event->key.windowID; break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: window_id = event->button.windowID; break;
        case SDL_MOUSEMOTION: window_id = event->motion.windowID; break;
        case SDL_MOUSEWHEEL: window_id = event->wheel.windowID; break;
        case SDL_TEXTINPUT: window_id = event->text.windowID; break;
        default: break;
    }
    for (size_t i = 0; i < manager->count && i < 9; ++i) {
        NvrRenderer *child = view->detached[i];
        if (!child || window_id != SDL_GetWindowID(child->window)) continue;
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F10) {
            open_settings(ui);
            SDL_RestoreWindow(view->window);
            SDL_RaiseWindow(view->window);
            return 1;
        }
        if ((event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE) ||
            (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE)) {
            nvr_renderer_attach(view, i);
            nvr_camera_request_main_stream(&manager->cameras[i], manager->cameras[i].grid_main_stream);
        } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
            int width, height;
            SDL_GetWindowSize(child->window, &width, &height);
            NvrRect area = {0, 0, width, height};
            int x = event->button.x, y = event->button.y;
            if (handle_media_click(ui, &manager->cameras[i], i, area, event)) return 1;
            if (inside(x, y, detach_button(area))) nvr_renderer_attach(view, i);
            else if (inside(x, y, quality_button(area))) {
                if (event->button.clicks == 1) toggle_quality(&manager->cameras[i]);
            } else if (event->button.clicks == 2) {
                Uint32 flags = SDL_GetWindowFlags(child->window);
                if (SDL_SetWindowFullscreen(child->window,
                    flags & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
                    snprintf(ui->status, sizeof(ui->status), "NAO FOI POSSIVEL MAXIMIZAR A JANELA");
            }
        }
        return 1;
    }
    if (window_id && window_id != SDL_GetWindowID(view->window)) return 1;
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE) return 0;

    if (ui->panel == NVR_UI_SETTINGS) {
        settings_event(ui, view, event);
        return 1;
    }
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F10 && ui->panel == NVR_UI_NONE) {
        open_settings(ui);
        return 1;
    }
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
        ui->panel == NVR_UI_ADD_CAMERA) show_active_field(ui, view);
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_q && ui->panel == NVR_UI_NONE && view->fullscreen_camera < 0) return 0;
        if (key == SDLK_ESCAPE) {
            if (ui->panel != NVR_UI_NONE) close_panel(ui);
            else if (view->fullscreen_camera >= 0) leave_fullscreen(view, manager);
            return 1;
        }
        if (ui->panel == NVR_UI_ADD_CAMERA) {
            if (key == SDLK_TAB && (event->key.keysym.mod & KMOD_SHIFT))
                ui->active_field = (ui->active_field + FIELD_COUNT - 1) % FIELD_COUNT;
            else if (key == SDLK_TAB || key == SDLK_DOWN) ui->active_field = (ui->active_field + 1) % FIELD_COUNT;
            else if (key == SDLK_UP) ui->active_field = (ui->active_field + FIELD_COUNT - 1) % FIELD_COUNT;
            else if (ui->active_field == FIELD_TRANSPORT && (key == SDLK_SPACE || key == SDLK_LEFT || key == SDLK_RIGHT))
                strcpy(ui->fields[FIELD_TRANSPORT], !strcmp(ui->fields[FIELD_TRANSPORT], "udp") ? "tcp" : "udp");
            else if (key == SDLK_BACKSPACE && ui->active_field != FIELD_TRANSPORT) {
                size_t length = strlen(ui->fields[ui->active_field]); if (length) ui->fields[ui->active_field][length-1] = '\0';
            } else if (key == SDLK_RETURN) {
                if (ui->active_field == FIELD_TRANSPORT) save_camera(ui, view, manager);
                else ui->active_field++;
            }
            show_active_field(ui, view);
        }
    }
    if (event->type == SDL_TEXTINPUT && ui->panel == NVR_UI_ADD_CAMERA && ui->active_field != FIELD_TRANSPORT) {
        char *field = ui->fields[ui->active_field]; size_t have = strlen(field), add = strlen(event->text.text);
        if (have + add < 255) memcpy(field + have, event->text.text, add + 1);
    }
    if (event->type == SDL_MOUSEWHEEL && ui->panel == NVR_UI_ADD_CAMERA) {
        int direction = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        ui->first_visible_field -= event->wheel.y * direction;
        (void)camera_form(ui, view);
        return 1;
    }
    if (event->type != SDL_MOUSEBUTTONDOWN) return 1;
    int x = event->button.x, y = event->button.y;
    if (ui->panel == NVR_UI_ADD_CAMERA) {
        CameraForm form = camera_form(ui, view);
        for (int row = 0; row < form.rows; ++row) {
            int i = ui->first_visible_field + row;
            SDL_Rect field = camera_field(form, row);
            if (inside(x, y, field)) {
                ui->active_field = i;
                if (i == FIELD_TRANSPORT) strcpy(ui->fields[i], !strcmp(ui->fields[i], "udp") ? "tcp" : "udp");
                return 1;
            }
        }
        if (inside(x, y, form.save)) save_camera(ui, view, manager);
        else if (inside(x, y, form.cancel)) close_panel(ui);
        else if(ui->editing_index>=0&&inside(x,y,form.remove))delete_camera(ui,view,manager);
        return 1;
    }
    if (ui->panel == NVR_UI_LAYOUT) {
        const size_t choices[] = {0,1,2,4,6,8,9};
        for (size_t i = 0; i < 7; ++i)
            if (inside(x, y, (SDL_Rect){190,48+(int)i*38,190,34})) { select_layout(ui, view, choices[i]); return 1; }
        close_panel(ui); return 1;
    }
    int window_width, window_height;
    SDL_GetWindowSize(view->window, &window_width, &window_height);
    (void)window_height;
    SDL_Rect settings_button = view->hide_menu || view->fullscreen_camera >= 0 || window_width < 510 ?
        (SDL_Rect){window_width-60, 6, 52, 28} : (SDL_Rect){328,6,170,32};
    if (event->button.button == SDL_BUTTON_LEFT && inside(x, y, settings_button)) {
        open_settings(ui); return 1;
    }
    if (!view->hide_menu && view->fullscreen_camera < 0 && inside(x,y,(SDL_Rect){8,6,170,32})) { open_add(ui); return 1; }
    if (!view->hide_menu && view->fullscreen_camera < 0 && inside(x,y,(SDL_Rect){188,6,130,32})) { ui->panel=NVR_UI_LAYOUT; return 1; }
    if (manager->count && event->button.button == SDL_BUTTON_LEFT) {
        NvrRect areas[9];
        nvr_renderer_camera_areas(view, manager->count, areas);
        for (size_t i = 0; i < manager->count; ++i) {
            if (!inside(x, y, (SDL_Rect){areas[i].x, areas[i].y, areas[i].width, areas[i].height})) continue;
            if (handle_media_click(ui, &manager->cameras[i], i, areas[i], event)) return 1;
            if (inside(x, y, quality_button(areas[i]))) {
                if (event->button.clicks == 1) toggle_quality(&manager->cameras[i]);
                return 1;
            }
            if (inside(x, y, detach_button(areas[i]))) {
                if (nvr_renderer_detach(view, i, manager->cameras[i].config.name))
                    snprintf(ui->status, sizeof(ui->status), "NAO FOI POSSIVEL DESTACAR A CAMERA");
                else manager->cameras[i].grid_main_stream = atomic_load(&manager->cameras[i].main_stream_requested);
                return 1;
            }
            SDL_Rect label = {areas[i].x+5, areas[i].y+5,
                              areas[i].width < 230 ? areas[i].width-10 : 220, 22};
            if (view->fullscreen_camera < 0 && inside(x, y, label)) {
                open_edit(ui, &manager->cameras[i].config, i); return 1;
            }
        }
    }
    if (event->button.button != SDL_BUTTON_LEFT || event->button.clicks != 2 || !manager->count) return 1;
    int selected = view->fullscreen_camera;
    if (selected < 0) {
        NvrRect areas[9];
        nvr_renderer_camera_areas(view, manager->count, areas);
        for (size_t i=0;i<manager->count;i++) {
            if (x>=areas[i].x && x<areas[i].x+areas[i].width && y>=areas[i].y && y<areas[i].y+areas[i].height) { selected=(int)i; break; }
        }
    }
    if (selected >= 0) {
        int leaving = view->fullscreen_camera == selected;
        nvr_renderer_toggle_fullscreen_camera(view, selected);
        nvr_camera_request_main_stream(&manager->cameras[selected],
                                       leaving ? manager->cameras[selected].grid_main_stream : 1);
    }
    return 1;
}

static void draw_menu(NvrUi *ui, NvrRenderer *view) {
    SDL_SetRenderDrawColor(view->renderer, 20,25,32,255);
    SDL_Rect bar={0,0,10000,NVR_MENU_HEIGHT}; SDL_RenderFillRect(view->renderer,&bar);
    button(view->renderer,(SDL_Rect){8,6,170,32},translate(ui, "+ ADD CAMERA"),ui->panel==NVR_UI_ADD_CAMERA);
    button(view->renderer,(SDL_Rect){188,6,130,32},"LAYOUT",ui->panel==NVR_UI_LAYOUT);
    int width, height;
    SDL_GetWindowSize(view->window, &width, &height);
    (void)height;
    if (width >= 510) form_button(view->renderer,(SDL_Rect){328,6,170,32},translate(ui, "CONFIGURACOES"),-1);
    else form_button(view->renderer,(SDL_Rect){width-60,6,52,28},"MENU",-1);
    if (ui->status[0]) text(view->renderer,510,15,1,(SDL_Color){161,176,194,255},translate(ui, ui->status));
}

int nvr_ui_update_media(NvrUi *ui, NvrCameraManager *manager, Uint32 now) {
    int dirty = 0;
    for (size_t i = 0; i < manager->count && i < 9; ++i) {
        int record = atomic_load(&manager->cameras[i].media.recording_state);
        int screenshot = atomic_load(&manager->cameras[i].media.screenshot_state);
        if (record != ui->media_record_state[i]) {
            ui->media_record_state[i] = record;
            ui->media_record_notice[i] = record != NVR_MEDIA_IDLE;
            ui->media_record_until[i] = now+3000;
            dirty = 1;
        }
        if (screenshot != ui->media_screenshot_state[i]) {
            ui->media_screenshot_state[i] = screenshot;
            ui->media_screenshot_notice[i] = screenshot != NVR_MEDIA_IDLE;
            ui->media_screenshot_until[i] = now+3000;
            dirty = 1;
        }
        if (ui->media_record_notice[i] && SDL_TICKS_PASSED(now, ui->media_record_until[i])) {
            ui->media_record_notice[i] = 0; dirty = 1;
        }
        if (ui->media_screenshot_notice[i] && SDL_TICKS_PASSED(now, ui->media_screenshot_until[i])) {
            ui->media_screenshot_notice[i] = 0; dirty = 1;
        }
        if (ui->media_message[i][0] && SDL_TICKS_PASSED(now, ui->media_message_until[i])) {
            ui->media_message[i][0] = '\0'; dirty = 1;
        }
    }
    return dirty;
}

static void draw_media_notice(NvrUi *ui, NvrRenderer *target, NvrRect area,
                              const char *message, int y) {
    if (!message[0] || area.width <= 24) return;
    const char *label = translate(ui, message);
    int width = (int)strlen(label)*6+16;
    if (width > area.width-12) width = area.width-12;
    SDL_Rect background = {area.x+(area.width-width)/2, y, width, 20};
    SDL_SetRenderDrawColor(target->renderer, 12,18,24,200);
    SDL_RenderFillRect(target->renderer, &background);
    clipped_text(target->renderer, (SDL_Rect){background.x+8,y+6,width-16,8},
                 1, (SDL_Color){240,220,190,255}, label);
}

static void draw_media_controls(NvrUi *ui, NvrRenderer *target, NvrCamera *camera, size_t index, NvrRect area) {
    int record = atomic_load(&camera->media.recording_state);
    int screenshot = atomic_load(&camera->media.screenshot_state);
    int active = record == NVR_MEDIA_WAITING || record == NVR_MEDIA_ACTIVE || record == NVR_MEDIA_STOPPING;
    SDL_Rect rec_button = media_button(area, 0);
    if (active) {
        SDL_SetRenderDrawColor(target->renderer, 190, 30, 30, 210);
        SDL_RenderFillRect(target->renderer, &rec_button);
    }
    overlay_button(target, rec_button, active ? "STOP" : "REC");
    overlay_button(target, media_button(area, 1), "PRINT");
    const char *record_labels[] = {"", "REC: AGUARDANDO QUADRO CHAVE", "REC: GRAVANDO", "REC: FINALIZANDO",
                                  "REC: SALVO", "REC: ERRO DE GRAVACAO", "REC: CANCELADO SEM VIDEO"};
    const char *screen_label = screenshot == NVR_MEDIA_SAVED ? "PRINT: SALVO" : screenshot == NVR_MEDIA_ERROR ?
                              "PRINT: ERRO AO SALVAR" : screenshot == NVR_MEDIA_WAITING || screenshot == NVR_MEDIA_ACTIVE ?
                              "PRINT: AGUARDANDO QUADRO" : "";
    const char *record_label = ui->media_record_notice[index] ? record_labels[record] : "";
    if (!ui->media_screenshot_notice[index]) screen_label = "";
    const char *message = record_label[0] ? record_label : screen_label;
    if (ui->media_message[index][0] && !SDL_TICKS_PASSED(SDL_GetTicks(), ui->media_message_until[index]))
        message = ui->media_message[index];
    int second_line = area.height >= 120 && record_label[0] && screen_label[0];
    int y = area.y+area.height-(second_line ? 108 : 84);
    draw_media_notice(ui, target, area, message, y);
    if (second_line) draw_media_notice(ui, target, area, screen_label, y+24);
}

static void draw_camera_labels(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    NvrRect areas[9];
    nvr_renderer_camera_areas(view, manager->count, areas);
    for (size_t i = 0; i < manager->count; ++i) {
        NvrRenderer *target = view->detached[i] ? view->detached[i] : view;
        NvrRect area = areas[i];
        if (target != view) {
            SDL_GetWindowSize(target->window, &area.width, &area.height);
            area.x = area.y = 0;
        }
        if (area.width <= 0 || area.height <= 0) continue;
        SDL_Rect bounds = {area.x, area.y, area.width, area.height};
        SDL_RenderSetClipRect(target->renderer, &bounds);
        draw_media_controls(ui, target, &manager->cameras[i], i, area);
        overlay_button(target, quality_button(area), atomic_load(&manager->cameras[i].main_stream_requested) ? "FHD" : "HD");
        overlay_button(target, detach_button(area), translate(ui, target == view ? "DESTACAR" : "VOLTAR"));
        SDL_Rect bg = {area.x+5, area.y+5, area.width < 230 ? area.width-10 : 220, 22};
        SDL_SetRenderDrawColor(target->renderer, 0, 0, 0, 90);
        SDL_RenderFillRect(target->renderer, &bg);
        clipped_text(target->renderer, (SDL_Rect){bg.x+6,bg.y+7,bg.w-12,8}, 1,
                     (SDL_Color){240,244,248,190}, manager->cameras[i].config.name);
        SDL_RenderSetClipRect(target->renderer, NULL);
    }
}

void nvr_ui_draw(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    if (view->fullscreen_camera < 0 && !view->hide_menu) draw_menu(ui,view);
    draw_camera_labels(ui,view,manager);
    if (view->hide_menu || view->fullscreen_camera >= 0) {
        int width, height;
        SDL_GetWindowSize(view->window, &width, &height);
        (void)height;
        overlay_button(view, (SDL_Rect){width-60,6,52,28}, "MENU");
    }
    if (ui->panel == NVR_UI_SETTINGS) { draw_settings(ui, view); return; }
    if (ui->panel == NVR_UI_LAYOUT) {
        SDL_SetRenderDrawColor(view->renderer,24,31,40,255); SDL_Rect panel={188,44,192,276}; SDL_RenderFillRect(view->renderer,&panel);
        const char *labels[]={"AUTOMATICO","1 TELA","2 TELAS","4 TELAS","6 TELAS","8 TELAS","9 TELAS"};
        const size_t values[]={0,1,2,4,6,8,9};
        for(int i=0;i<7;i++) button(view->renderer,(SDL_Rect){190,48+i*38,190,34},translate(ui, labels[i]),view->layout_slots==values[i]);
    } else if (ui->panel == NVR_UI_ADD_CAMERA) {
        CameraForm form = camera_form(ui, view);
        SDL_SetRenderDrawBlendMode(view->renderer,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(view->renderer,5,8,12,225); SDL_Rect shade={0,NVR_MENU_HEIGHT,10000,10000}; SDL_RenderFillRect(view->renderer,&shade);
        SDL_SetRenderDrawColor(view->renderer,24,31,40,255); SDL_RenderFillRect(view->renderer,&form.panel);
        clipped_text(view->renderer,(SDL_Rect){form.panel.x+14,form.panel.y+14,form.panel.w-28,16},
                     form.panel.w >= 300 ? 2 : 1,(SDL_Color){238,242,247,255},translate(ui, ui->editing_index>=0?"EDITAR CAMERA":"ADICIONAR CAMERA"));
        for(int row=0;row<form.rows;row++) {
            int i=ui->first_visible_field+row;
            SDL_Rect field=camera_field(form,row);
            clipped_text(view->renderer,(SDL_Rect){form.panel.x+14,field.y+10,form.field_x-form.panel.x-22,8},
                         1,(SDL_Color){171,185,201,255},translate(ui, field_labels[i]));
            SDL_SetRenderDrawColor(view->renderer,i==ui->active_field?35:14,i==ui->active_field?91:20,i==ui->active_field?135:27,255);
            SDL_RenderFillRect(view->renderer,&field);
            SDL_SetRenderDrawColor(view->renderer,70,87,105,255); SDL_RenderDrawRect(view->renderer,&field);
            char hidden[256]; const char *shown=ui->fields[i];
            if(i==FIELD_PASSWORD){size_t n=strlen(shown);if(n>40)n=40;memset(hidden,'*',n);hidden[n]='\0';shown=hidden;}
            size_t visible=field.w > 16 ? (size_t)(field.w-16)/6 : 0;
            if(i==ui->active_field && strlen(shown)>visible) shown+=strlen(shown)-visible;
            clipped_text(view->renderer,(SDL_Rect){field.x+8,field.y+10,field.w-16,8},
                         1,(SDL_Color){235,240,246,255},shown);
        }
        form_button(view->renderer,form.save,translate(ui, "SALVAR"),0); form_button(view->renderer,form.cancel,translate(ui, "CANCELAR"),-1);
        if(ui->editing_index>=0)form_button(view->renderer,form.remove,translate(ui, ui->confirm_delete?"CONFIRMAR":"DELETAR"),1);
        form_notes(ui,view->renderer,form);
        if(ui->status[0]) clipped_text(view->renderer,(SDL_Rect){form.panel.x+14,form.status_y,form.panel.w-28,8},
                                      1,(SDL_Color){240,90,90,255},translate(ui, ui->status));
    }
}

/* Assistente de terminal mantido para ambientes sem interface gráfica. */
static int prompt(const char *label, char *value, size_t size) {
    char line[512];
    if (value[0]) printf("%s [%s]: ", label, value); else printf("%s: ", label);
    fflush(stdout); if (!fgets(line, sizeof(line), stdin)) return -1;
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0]) { if (strlen(line) >= size) return -1; memcpy(value,line,strlen(line)+1); }
    return 0;
}

static int prompt_optional(const char *label, char *value, size_t size) {
    if (prompt(label, value, size)) return -1;
    if (!strcmp(value, "-")) value[0] = '\0';
    return 0;
}

int nvr_ui_configure_file(const char *path) {
    NvrConfig config={0}; char error[256];
    if(access(path,F_OK)==0&&nvr_config_load(path,&config,error,sizeof(error))){fprintf(stderr,"Configuração inválida: %s\n",error);return 1;}
    printf("Câmeras cadastradas:\n"); for(size_t i=0;i<config.count;i++)printf("  %zu. %s (IP/host: %s; MAC: %s)\n",i+1,config.cameras[i].name,config.cameras[i].host,config.cameras[i].mac_address);
    printf("Digite o número para editar ou 0 para adicionar: "); fflush(stdout); char line[64];
    if(!fgets(line,sizeof(line),stdin)){nvr_config_free(&config);return 1;} char *end; long choice=strtol(line,&end,10);
    if(end==line||choice<0||(size_t)choice>config.count||(choice==0&&config.count>=9)){nvr_config_free(&config);return 1;}
    int adding=choice==0; NvrCameraConfig item=adding?(NvrCameraConfig){.port=554,.transport=NVR_TRANSPORT_TCP}:config.cameras[choice-1];
    if(adding){strcpy(item.grid_path,"/onvif2");strcpy(item.main_path,"/onvif1");}
    char port[16],transport[8];snprintf(port,sizeof(port),"%u",item.port);snprintf(transport,sizeof(transport),"%s",nvr_transport_name(item.transport));
    printf("Informe IP/host ou MAC. Para câmeras com DHCP, recomendamos MAC na mesma rede local.\n"
           "No modem, reserve DHCP pelo MAC da interface Wi-Fi ou cabo da câmera. '-' apaga IP ou MAC.\n");
    char mac[256];snprintf(mac,sizeof(mac),"%s",item.mac_address);
    if(prompt("Nome",item.name,sizeof(item.name))||prompt_optional("IP/host (opcional com MAC)",item.host,sizeof(item.host))||
       prompt_optional("MAC (opcional com IP/host)",mac,sizeof(mac))||prompt("Porta",port,sizeof(port))||
       prompt("Usuário",item.username,sizeof(item.username))||prompt("Variável de ambiente da senha",item.password_env,sizeof(item.password_env))||
       prompt("Caminho da grade",item.grid_path,sizeof(item.grid_path))||prompt("Caminho principal",item.main_path,sizeof(item.main_path))||
       prompt("Transporte (tcp/udp)",transport,sizeof(transport))){nvr_config_free(&config);return 1;}
    if(mac[0]&&!nvr_mac_normalize(mac,item.mac_address,sizeof(item.mac_address))){fprintf(stderr,"MAC inválido. Use AA:BB:CC:DD:EE:FF.\n");nvr_config_free(&config);return 1;}
    if(!mac[0])item.mac_address[0]='\0';
    long parsed=strtol(port,&end,10);if(!item.name[0]||(!item.host[0]&&!item.mac_address[0])||end==port||*end||parsed<1||parsed>65535||(strcmp(transport,"tcp")&&strcmp(transport,"udp"))){nvr_config_free(&config);return 1;}
    item.port=(unsigned short)parsed;item.transport=!strcmp(transport,"udp")?NVR_TRANSPORT_UDP:NVR_TRANSPORT_TCP;
    if(adding){NvrCameraConfig *items=realloc(config.cameras,(config.count+1)*sizeof(*items));if(!items){nvr_config_free(&config);return 1;}config.cameras=items;config.cameras[config.count++]=item;}else config.cameras[choice-1]=item;
    int result=nvr_config_save(path,&config,error,sizeof(error));if(result)fprintf(stderr,"Falha ao salvar: %s\n",error);else printf("Configuração salva em %s.\n",path);
    nvr_config_free(&config);return result!=0;
}
