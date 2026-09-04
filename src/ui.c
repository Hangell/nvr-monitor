#include "nvr/ui.h"
#include "nvr/config.h"
#include "nvr/layout.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { FIELD_NAME, FIELD_HOST, FIELD_PORT, FIELD_USER, FIELD_PASSWORD,
       FIELD_GRID_PATH, FIELD_MAIN_PATH, FIELD_TRANSPORT, FIELD_COUNT };

static const char *field_labels[FIELD_COUNT] = {
    "NOME", "IP / HOST", "PORTA", "USUARIO", "SENHA",
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

void nvr_ui_init(NvrUi *ui, const char *config_path) {
    memset(ui, 0, sizeof(*ui)); ui->active_field = FIELD_NAME;
    snprintf(ui->config_path, sizeof(ui->config_path), "%s", config_path);
}

static void open_add(NvrUi *ui) {
    memset(ui->fields, 0, sizeof(ui->fields));
    strcpy(ui->fields[FIELD_PORT], "554"); strcpy(ui->fields[FIELD_GRID_PATH], "/onvif2");
    strcpy(ui->fields[FIELD_MAIN_PATH], "/onvif1"); strcpy(ui->fields[FIELD_TRANSPORT], "udp");
    ui->panel = NVR_UI_ADD_CAMERA; ui->active_field = FIELD_NAME; ui->status[0] = '\0';
    SDL_StartTextInput();
}

static void close_panel(NvrUi *ui) { ui->panel = NVR_UI_NONE; SDL_StopTextInput(); }

static int save_manager_config(NvrUi *ui, NvrCameraManager *manager) {
    NvrCameraConfig *items = calloc(manager->count, sizeof(*items));
    if (!items) return -1;
    for (size_t i = 0; i < manager->count; ++i) items[i] = manager->cameras[i].config;
    NvrConfig config = {items, manager->count}; char error[128];
    int result = nvr_config_save(ui->config_path, &config, error, sizeof(error)); free(items);
    if (result) snprintf(ui->status, sizeof(ui->status), "ERRO AO SALVAR CONFIGURACAO");
    return result;
}

static int add_camera(NvrUi *ui, NvrRenderer *view, NvrCameraManager *manager) {
    char *end = NULL; long port = strtol(ui->fields[FIELD_PORT], &end, 10);
    if (!ui->fields[FIELD_NAME][0] || !ui->fields[FIELD_HOST][0] || !end || *end ||
        port < 1 || port > 65535 ||
        (strcmp(ui->fields[FIELD_TRANSPORT], "udp") && strcmp(ui->fields[FIELD_TRANSPORT], "tcp"))) {
        snprintf(ui->status, sizeof(ui->status), "PREENCHA NOME HOST PORTA E TRANSPORTE"); return -1;
    }
    NvrCameraConfig camera = {.port=(unsigned short)port};
#define COPY_FIELD(destination, source) do { if (strlen(source) >= sizeof(destination)) return -1; strcpy(destination, source); } while (0)
    COPY_FIELD(camera.name, ui->fields[FIELD_NAME]); COPY_FIELD(camera.host, ui->fields[FIELD_HOST]);
    COPY_FIELD(camera.username, ui->fields[FIELD_USER]); COPY_FIELD(camera.grid_path, ui->fields[FIELD_GRID_PATH]);
    COPY_FIELD(camera.main_path, ui->fields[FIELD_MAIN_PATH]);
#undef COPY_FIELD
    camera.transport = !strcmp(ui->fields[FIELD_TRANSPORT], "udp") ? NVR_TRANSPORT_UDP : NVR_TRANSPORT_TCP;
    if (ui->fields[FIELD_PASSWORD][0]) {
        snprintf(camera.password_env, sizeof(camera.password_env), "NVR_CAMERA_%zu_PASSWORD", manager->count + 1);
        if (setenv(camera.password_env, ui->fields[FIELD_PASSWORD], 1)) return -1;
    }
    if (nvr_renderer_resize_cameras(view, manager->count + 1) || nvr_camera_manager_add(manager, &camera)) {
        snprintf(ui->status, sizeof(ui->status), "NAO FOI POSSIVEL ADICIONAR A CAMERA"); return -1;
    }
    save_manager_config(ui, manager);
    close_panel(ui);
    if (camera.password_env[0])
        snprintf(ui->status, sizeof(ui->status), "ADICIONADA - AO REINICIAR EXPORTE %s", camera.password_env);
    else
        snprintf(ui->status, sizeof(ui->status), "CAMERA ADICIONADA");
    return 0;
}

static void leave_fullscreen(NvrRenderer *view, NvrCameraManager *manager) {
    int old = view->fullscreen_camera;
    if (old >= 0 && (size_t)old < manager->count) {
        nvr_renderer_toggle_fullscreen_camera(view, old);
        nvr_camera_request_main_stream(&manager->cameras[old], 0);
    }
}

static void select_layout(NvrUi *ui, NvrRenderer *view, size_t slots) {
    view->layout_slots = slots; close_panel(ui);
    snprintf(ui->status, sizeof(ui->status), slots ? "LAYOUT %zu SELECIONADO" : "LAYOUT AUTOMATICO", slots);
}

int nvr_ui_handle_event(NvrUi *ui, const SDL_Event *event, NvrRenderer *view,
                        NvrCameraManager *manager) {
    if (event->type == SDL_QUIT) return 0;
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_q && ui->panel == NVR_UI_NONE && view->fullscreen_camera < 0) return 0;
        if (key == SDLK_ESCAPE) {
            if (ui->panel != NVR_UI_NONE) close_panel(ui);
            else if (view->fullscreen_camera >= 0) leave_fullscreen(view, manager);
            return 1;
        }
        if (ui->panel == NVR_UI_ADD_CAMERA) {
            if (key == SDLK_TAB || key == SDLK_DOWN) ui->active_field = (ui->active_field + 1) % FIELD_COUNT;
            else if (key == SDLK_UP) ui->active_field = (ui->active_field + FIELD_COUNT - 1) % FIELD_COUNT;
            else if (key == SDLK_BACKSPACE && ui->active_field != FIELD_TRANSPORT) {
                size_t length = strlen(ui->fields[ui->active_field]); if (length) ui->fields[ui->active_field][length-1] = '\0';
            } else if (key == SDLK_RETURN) {
                if (ui->active_field == FIELD_TRANSPORT) add_camera(ui, view, manager);
                else ui->active_field++;
            }
        }
    }
    if (event->type == SDL_TEXTINPUT && ui->panel == NVR_UI_ADD_CAMERA && ui->active_field != FIELD_TRANSPORT) {
        char *field = ui->fields[ui->active_field]; size_t have = strlen(field), add = strlen(event->text.text);
        if (have + add < 255) memcpy(field + have, event->text.text, add + 1);
    }
    if (event->type != SDL_MOUSEBUTTONDOWN) return 1;
    int x = event->button.x, y = event->button.y;
    if (ui->panel == NVR_UI_ADD_CAMERA) {
        for (int i = 0; i < FIELD_COUNT; ++i) {
            SDL_Rect field = {250, 94 + i * 42, 650, 30};
            if (inside(x, y, field)) {
                ui->active_field = i;
                if (i == FIELD_TRANSPORT) strcpy(ui->fields[i], !strcmp(ui->fields[i], "udp") ? "tcp" : "udp");
                return 1;
            }
        }
        if (inside(x, y, (SDL_Rect){250,440,170,36})) add_camera(ui, view, manager);
        else if (inside(x, y, (SDL_Rect){435,440,150,36})) close_panel(ui);
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
        nvr_camera_request_main_stream(&manager->cameras[selected], !leaving);
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
        SDL_SetRenderDrawBlendMode(view->renderer,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(view->renderer,5,8,12,225); SDL_Rect shade={0,NVR_MENU_HEIGHT,10000,10000}; SDL_RenderFillRect(view->renderer,&shade);
        SDL_SetRenderDrawColor(view->renderer,24,31,40,255); SDL_Rect panel={90,58,850,530}; SDL_RenderFillRect(view->renderer,&panel);
        text(view->renderer,112,72,2,(SDL_Color){238,242,247,255},"ADICIONAR CAMERA");
        for(int i=0;i<FIELD_COUNT;i++) {
            int y=94+i*42; text(view->renderer,112,y+10,1,(SDL_Color){171,185,201,255},field_labels[i]);
            SDL_SetRenderDrawColor(view->renderer,i==ui->active_field?35:14,i==ui->active_field?91:20,i==ui->active_field?135:27,255);
            SDL_Rect field={250,y,650,30}; SDL_RenderFillRect(view->renderer,&field);
            SDL_SetRenderDrawColor(view->renderer,70,87,105,255); SDL_RenderDrawRect(view->renderer,&field);
            char hidden[256]; const char *shown=ui->fields[i];
            if(i==FIELD_PASSWORD){size_t n=strlen(shown);if(n>40)n=40;memset(hidden,'*',n);hidden[n]='\0';shown=hidden;}
            text(view->renderer,258,y+10,1,(SDL_Color){235,240,246,255},shown);
        }
        button(view->renderer,(SDL_Rect){250,440,170,36},"SALVAR",1); button(view->renderer,(SDL_Rect){435,440,150,36},"CANCELAR",0);
        text(view->renderer,112,496,1,(SDL_Color){247,188,76,255},"YOOSEE: ATIVE O NVR NO APP DO CELULAR E SELECIONE UDP.");
        text(view->renderer,112,516,1,(SDL_Color){161,176,194,255},"TESTE: FFPLAY -RTSP_TRANSPORT UDP RTSP://USUARIO:SENHA@IP:554/ONVIF1");
        text(view->renderer,112,536,1,(SDL_Color){161,176,194,255},"CODIFIQUE CARACTERES ESPECIAIS NO TESTE. A SENHA NAO SERA GRAVADA.");
        if(ui->status[0]) text(view->renderer,112,556,1,(SDL_Color){240,90,90,255},ui->status);
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

int nvr_ui_configure_file(const char *path) {
    NvrConfig config={0}; char error[256];
    if(access(path,F_OK)==0&&nvr_config_load(path,&config,error,sizeof(error))){fprintf(stderr,"Configuração inválida: %s\n",error);return 1;}
    printf("Câmeras cadastradas:\n"); for(size_t i=0;i<config.count;i++)printf("  %zu. %s (%s)\n",i+1,config.cameras[i].name,config.cameras[i].host);
    printf("Digite o número para editar ou 0 para adicionar: "); fflush(stdout); char line[64];
    if(!fgets(line,sizeof(line),stdin)){nvr_config_free(&config);return 1;} char *end; long choice=strtol(line,&end,10);
    if(end==line||choice<0||(size_t)choice>config.count||(choice==0&&config.count>=9)){nvr_config_free(&config);return 1;}
    int adding=choice==0; NvrCameraConfig item=adding?(NvrCameraConfig){.port=554,.transport=NVR_TRANSPORT_TCP}:config.cameras[choice-1];
    if(adding){strcpy(item.grid_path,"/onvif2");strcpy(item.main_path,"/onvif1");}
    char port[16],transport[8];snprintf(port,sizeof(port),"%u",item.port);snprintf(transport,sizeof(transport),"%s",nvr_transport_name(item.transport));
    if(prompt("Nome",item.name,sizeof(item.name))||prompt("IP/host",item.host,sizeof(item.host))||prompt("Porta",port,sizeof(port))||
       prompt("Usuário",item.username,sizeof(item.username))||prompt("Variável de ambiente da senha",item.password_env,sizeof(item.password_env))||
       prompt("Caminho da grade",item.grid_path,sizeof(item.grid_path))||prompt("Caminho principal",item.main_path,sizeof(item.main_path))||
       prompt("Transporte (tcp/udp)",transport,sizeof(transport))){nvr_config_free(&config);return 1;}
    long parsed=strtol(port,&end,10);if(!item.name[0]||!item.host[0]||*end||parsed<1||parsed>65535||(strcmp(transport,"tcp")&&strcmp(transport,"udp"))){nvr_config_free(&config);return 1;}
    item.port=(unsigned short)parsed;item.transport=!strcmp(transport,"udp")?NVR_TRANSPORT_UDP:NVR_TRANSPORT_TCP;
    if(adding){NvrCameraConfig *items=realloc(config.cameras,(config.count+1)*sizeof(*items));if(!items){nvr_config_free(&config);return 1;}config.cameras=items;config.cameras[config.count++]=item;}else config.cameras[choice-1]=item;
    int result=nvr_config_save(path,&config,error,sizeof(error));if(result)fprintf(stderr,"Falha ao salvar: %s\n",error);else printf("Configuração salva em %s.\n",path);
    nvr_config_free(&config);return result!=0;
}
