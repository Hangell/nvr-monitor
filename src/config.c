#include "nvr/config.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *text; size_t pos, length; } Json;

static void set_error(char *error, size_t size, const char *message) {
    if (error && size) snprintf(error, size, "%s", message);
}
static void skip_ws(Json *j) { while (j->pos < j->length && isspace((unsigned char)j->text[j->pos])) j->pos++; }
static int take(Json *j, char c) { skip_ws(j); if (j->pos < j->length && j->text[j->pos] == c) { j->pos++; return 1; } return 0; }

static int string(Json *j, char *out, size_t size) {
    skip_ws(j);
    if (j->pos >= j->length || j->text[j->pos++] != '"') return -1;
    size_t n = 0;
    while (j->pos < j->length && j->text[j->pos] != '"') {
        unsigned char c = (unsigned char)j->text[j->pos++];
        if (c == '\\') {
            if (j->pos >= j->length) return -1;
            c = (unsigned char)j->text[j->pos++];
            if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
            else if (c != '"' && c != '\\' && c != '/') return -1;
        }
        if (n + 1 >= size) return -1;
        out[n++] = (char)c;
    }
    if (j->pos >= j->length) return -1;
    j->pos++; out[n] = '\0';
    return 0;
}

static int number(Json *j, unsigned short *value) {
    skip_ws(j); char *end = NULL;
    errno = 0; long n = strtol(j->text + j->pos, &end, 10);
    if (end == j->text + j->pos || errno || n < 1 || n > 65535) return -1;
    j->pos = (size_t)(end - j->text); *value = (unsigned short)n; return 0;
}

static int copy_value(char *destination, size_t size, const char *value) {
    size_t length = strlen(value);
    if (length >= size) return -1;
    memcpy(destination, value, length + 1);
    return 0;
}

static int camera(Json *j, NvrCameraConfig *c) {
    *c = (NvrCameraConfig){.port = 554, .transport = NVR_TRANSPORT_TCP};
    snprintf(c->grid_path, sizeof(c->grid_path), "/onvif2");
    snprintf(c->main_path, sizeof(c->main_path), "/onvif1");
    if (!take(j, '{')) return -1;
    while (!take(j, '}')) {
        char key[64], value[NVR_PATH_MAX];
        if (string(j, key, sizeof(key)) || !take(j, ':')) return -1;
        if (!strcmp(key, "port")) { if (number(j, &c->port)) return -1; }
        else {
            if (string(j, value, sizeof(value))) return -1;
            if (!strcmp(key, "name")) { if (copy_value(c->name, sizeof(c->name), value)) return -1; }
            else if (!strcmp(key, "host")) { if (copy_value(c->host, sizeof(c->host), value)) return -1; }
            else if (!strcmp(key, "username")) { if (copy_value(c->username, sizeof(c->username), value)) return -1; }
            else if (!strcmp(key, "password_env")) { if (copy_value(c->password_env, sizeof(c->password_env), value)) return -1; }
            else if (!strcmp(key, "grid_path")) { if (copy_value(c->grid_path, sizeof(c->grid_path), value)) return -1; }
            else if (!strcmp(key, "main_path")) { if (copy_value(c->main_path, sizeof(c->main_path), value)) return -1; }
            else if (!strcmp(key, "transport")) {
                if (!strcmp(value, "udp")) c->transport = NVR_TRANSPORT_UDP;
                else if (!strcmp(value, "tcp")) c->transport = NVR_TRANSPORT_TCP;
                else return -1;
            } else return -1;
        }
        if (take(j, '}')) break;
        if (!take(j, ',')) return -1;
    }
    return c->name[0] && c->host[0] ? 0 : -1;
}

int nvr_config_load(const char *path, NvrConfig *config, char *error, size_t error_size) {
    if (!path || !config) return -1;
    *config = (NvrConfig){0};
    FILE *file = fopen(path, "rb");
    if (!file) { set_error(error, error_size, "não foi possível abrir o arquivo"); return -1; }
    if (fseek(file, 0, SEEK_END) || ftell(file) < 0) { fclose(file); return -1; }
    long length = ftell(file); rewind(file);
    char *text = malloc((size_t)length + 1);
    if (!text) { fclose(file); return -1; }
    size_t got = fread(text, 1, (size_t)length, file); fclose(file); text[got] = '\0';
    Json j = {text, 0, got};
    int ok = take(&j, '{'); char key[64];
    if (ok) ok = string(&j, key, sizeof(key)) == 0 && !strcmp(key, "cameras") && take(&j, ':') && take(&j, '[');
    while (ok && !take(&j, ']')) {
        NvrCameraConfig item;
        if (camera(&j, &item)) { ok = 0; break; }
        NvrCameraConfig *items = realloc(config->cameras, (config->count + 1) * sizeof(*items));
        if (!items) { ok = 0; break; }
        config->cameras = items; config->cameras[config->count++] = item;
        if (config->count > 9) { ok = 0; break; }
        if (take(&j, ']')) break;
        if (!take(&j, ',')) { ok = 0; break; }
    }
    ok = ok && take(&j, '}'); skip_ws(&j); ok = ok && j.pos == j.length;
    free(text);
    if (!ok) {
        nvr_config_free(config);
        set_error(error, error_size, "JSON inválido ou campo desconhecido");
        return -1;
    }
    return 0;
}

static void write_string(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; ++s) {
        if (*s == '\n') fputs("\\n", f);
        else if (*s == '\r') fputs("\\r", f);
        else if (*s == '\t') fputs("\\t", f);
        else if (*s == '"' || *s == '\\') { fputc('\\', f); fputc(*s, f); }
        else if ((unsigned char)*s >= 0x20) fputc(*s, f);
    }
    fputc('"', f);
}

int nvr_config_save(const char *path, const NvrConfig *config, char *error, size_t error_size) {
    if (!path || !config) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) { set_error(error, error_size, "não foi possível salvar o arquivo"); return -1; }
    fputs("{\n  \"cameras\": [", f);
    for (size_t i = 0; i < config->count; ++i) {
        const NvrCameraConfig *c = &config->cameras[i];
        fputs(i ? ",\n    {\n" : "\n    {\n", f);
#define FIELD(name, value, comma) fputs("      \"" name "\": ", f); write_string(f, value); fputs(comma "\n", f)
        FIELD("name", c->name, ","); FIELD("host", c->host, ",");
        fprintf(f, "      \"port\": %u,\n", c->port);
        FIELD("username", c->username, ","); FIELD("password_env", c->password_env, ",");
        FIELD("grid_path", c->grid_path, ","); FIELD("main_path", c->main_path, ",");
        FIELD("transport", nvr_transport_name(c->transport), "");
#undef FIELD
        fputs("    }", f);
    }
    fputs("\n  ]\n}\n", f);
    if (fclose(f) != 0) { set_error(error, error_size, "erro ao concluir a gravação"); return -1; }
    return 0;
}

void nvr_config_free(NvrConfig *config) {
    if (!config) return;
    free(config->cameras); *config = (NvrConfig){0};
}

const char *nvr_transport_name(NvrTransport transport) { return transport == NVR_TRANSPORT_UDP ? "udp" : "tcp"; }
