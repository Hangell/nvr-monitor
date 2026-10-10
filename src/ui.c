#include "nvr/ui.h"
#include "nvr/config.h"
#include "nvr/layout.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { FIELD_NAME, FIELD_HOST, FIELD_MAC, FIELD_PORT, FIELD_USER, FIELD_PASSWORD,
       FIELD_GRID_PATH, FIELD_MAIN_PATH, FIELD_TRANSPORT, FIELD_COUNT };

static const char *field_labels[FIELD_COUNT] = {
    "NOME", "IP / HOST", "MAC OPCIONAL", "PORTA", "USUARIO", "SENHA",
    "CAMINHO DA GRADE", "CAMINHO PRINCIPAL", "TRANSPORTE"
};

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

static void form_notes(SDL_Renderer *renderer, CameraForm form) {
    const char *notes = "PARA IP DINAMICO / DHCP, RECOMENDAMOS MAC. COM MAC, IP / HOST E OPCIONAL. "
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
    memset(ui, 0, sizeof(*ui)); ui->active_field = FIELD_NAME;ui->editing_index=-1;ui->database=database;
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
    for(size_t i=0;i<view->texture_count;i++){SDL_DestroyTexture(view->textures[i]);view->textures[i]=NULL;}
    nvr_renderer_resize_cameras(view,manager->count);close_panel(ui);snprintf(ui->status,sizeof(ui->status),"CAMERA DELETADA");return 0;
}

static SDL_Rect quality_button(NvrRect area) {
    int width = area.width < 110 ? area.width : 110;
    return (SDL_Rect){area.x + (area.width - width) / 2,
                      area.y + NVR_MENU_HEIGHT + area.height - NVR_CAMERA_FOOTER_HEIGHT + 4,
                      width, 32};
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
    snprintf(ui->status, sizeof(ui->status), slots ? "LAYOUT %zu SELECIONADO" : "LAYOUT AUTOMATICO", slots);
}

int nvr_ui_handle_event(NvrUi *ui, const SDL_Event *event, NvrRenderer *view,
                        NvrCameraManager *manager) {
    if (event->type == SDL_QUIT) return 0;
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
    if (view->fullscreen_camera < 0 && inside(x,y,(SDL_Rect){8,6,170,32})) { open_add(ui); return 1; }
    if (view->fullscreen_camera < 0 && inside(x,y,(SDL_Rect){188,6,130,32})) { ui->panel=NVR_UI_LAYOUT; return 1; }
    if(view->fullscreen_camera<0&&manager->count){
        int width,height;SDL_GetWindowSize(view->window,&width,&height);NvrRect areas[9];
        size_t slots=view->layout_slots?view->layout_slots:manager->count;if(slots<manager->count)slots=manager->count;
        nvr_layout_grid_slots(manager->count,slots,width,height-NVR_MENU_HEIGHT,areas,manager->count);
        for(size_t i=0;i<manager->count;i++){
            if (inside(x, y, quality_button(areas[i]))) {
                if (event->button.button == SDL_BUTTON_LEFT && event->button.clicks == 1) {
                    NvrCamera *camera = &manager->cameras[i];
                    camera->grid_main_stream = !camera->grid_main_stream;
                    nvr_camera_request_main_stream(camera, camera->grid_main_stream);
                }
                return 1;
            }
            SDL_Rect label={areas[i].x+5,areas[i].y+NVR_MENU_HEIGHT+5,220,22};
            if(inside(x,y,label)){open_edit(ui,&manager->cameras[i].config,i);return 1;}
        }
    }
    if (event->button.clicks != 2 || !manager->count) return 1;
    int selected = view->fullscreen_camera;
    if (selected < 0) {
        int width, height; SDL_GetWindowSize(view->window, &width, &height);
        NvrRect areas[9]; size_t slots = view->layout_slots ? view->layout_slots : manager->count;
        if (slots < manager->count) slots = manager->count;
        nvr_layout_grid_slots(manager->count, slots, width, height-NVR_MENU_HEIGHT, areas, manager->count);
        for (size_t i=0;i<manager->count;i++) {
            areas[i].y += NVR_MENU_HEIGHT;
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
    button(view->renderer,(SDL_Rect){8,6,170,32},"+ ADD CAMERA",ui->panel==NVR_UI_ADD_CAMERA);
    button(view->renderer,(SDL_Rect){188,6,130,32},"LAYOUT",ui->panel==NVR_UI_LAYOUT);
    if (ui->status[0]) text(view->renderer,338,15,1,(SDL_Color){161,176,194,255},ui->status);
}

static void draw_camera_labels(NvrRenderer *view, NvrCameraManager *manager) {
    if (!manager->count || view->fullscreen_camera >= 0) return;
    int width,height; SDL_GetWindowSize(view->window,&width,&height); NvrRect areas[9];
    size_t slots=view->layout_slots?view->layout_slots:manager->count; if(slots<manager->count)slots=manager->count;
    nvr_layout_grid_slots(manager->count,slots,width,height-NVR_MENU_HEIGHT,areas,manager->count);
    for(size_t i=0;i<manager->count;i++) {
        button(view->renderer, quality_button(areas[i]),
               manager->cameras[i].grid_main_stream ? "FHD" : "HD",
               manager->cameras[i].grid_main_stream);
        SDL_Rect bg={areas[i].x+5,areas[i].y+NVR_MENU_HEIGHT+5,220,22};
        SDL_SetRenderDrawColor(view->renderer,0,0,0,185); SDL_RenderFillRect(view->renderer,&bg);
        text(view->renderer,bg.x+6,bg.y+7,1,(SDL_Color){240,244,248,255},manager->cameras[i].config.name);
    }
}

void nvr_ui_draw(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    if (view->fullscreen_camera < 0) { draw_menu(ui,view); draw_camera_labels(view,manager); }
    if (ui->panel == NVR_UI_LAYOUT) {
        SDL_SetRenderDrawColor(view->renderer,24,31,40,255); SDL_Rect panel={188,44,192,276}; SDL_RenderFillRect(view->renderer,&panel);
        const char *labels[]={"AUTOMATICO","1 TELA","2 TELAS","4 TELAS","6 TELAS","8 TELAS","9 TELAS"};
        const size_t values[]={0,1,2,4,6,8,9};
        for(int i=0;i<7;i++) button(view->renderer,(SDL_Rect){190,48+i*38,190,34},labels[i],view->layout_slots==values[i]);
    } else if (ui->panel == NVR_UI_ADD_CAMERA) {
        CameraForm form = camera_form(ui, view);
        SDL_SetRenderDrawBlendMode(view->renderer,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(view->renderer,5,8,12,225); SDL_Rect shade={0,NVR_MENU_HEIGHT,10000,10000}; SDL_RenderFillRect(view->renderer,&shade);
        SDL_SetRenderDrawColor(view->renderer,24,31,40,255); SDL_RenderFillRect(view->renderer,&form.panel);
        clipped_text(view->renderer,(SDL_Rect){form.panel.x+14,form.panel.y+14,form.panel.w-28,16},
                     form.panel.w >= 300 ? 2 : 1,(SDL_Color){238,242,247,255},ui->editing_index>=0?"EDITAR CAMERA":"ADICIONAR CAMERA");
        for(int row=0;row<form.rows;row++) {
            int i=ui->first_visible_field+row;
            SDL_Rect field=camera_field(form,row);
            clipped_text(view->renderer,(SDL_Rect){form.panel.x+14,field.y+10,form.field_x-form.panel.x-22,8},
                         1,(SDL_Color){171,185,201,255},field_labels[i]);
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
        form_button(view->renderer,form.save,"SALVAR",0); form_button(view->renderer,form.cancel,"CANCELAR",-1);
        if(ui->editing_index>=0)form_button(view->renderer,form.remove,ui->confirm_delete?"CONFIRMAR":"DELETAR",1);
        form_notes(view->renderer,form);
        if(ui->status[0]) clipped_text(view->renderer,(SDL_Rect){form.panel.x+14,form.status_y,form.panel.w-28,8},
                                      1,(SDL_Color){240,90,90,255},ui->status);
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
