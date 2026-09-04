#include "nvr/paths.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define NVR_SEPARATOR '\\'
static int make_directory(const char *path) { return _mkdir(path); }
#else
#define NVR_SEPARATOR '/'
static int make_directory(const char *path) { return mkdir(path, 0700); }
#endif

static int make_directories(const char *directory) {
    char copy[512];
    size_t length = strlen(directory);
    if (!length || length >= sizeof(copy)) return -1;
    memcpy(copy, directory, length + 1);

    for (size_t i = 1; i <= length; ++i) {
        int boundary = copy[i] == '/' || copy[i] == '\\' || copy[i] == '\0';
        if (!boundary) continue;
#ifdef _WIN32
        if (i == 2 && copy[1] == ':') continue;
#endif
        char saved = copy[i];
        copy[i] = '\0';
        if (copy[0] && make_directory(copy) != 0 && errno != EEXIST) return -1;
        copy[i] = saved;
    }
    return 0;
}

int nvr_default_database_path(char *path, size_t path_size) {
    if (!path || !path_size) return -1;
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    if (!base || !base[0]) base = getenv("LOCALAPPDATA");
    const char *folder = "NVR Monitor";
#else
    const char *base = getenv("XDG_DATA_HOME");
    char fallback[512];
    if (!base || !base[0]) {
        const char *home = getenv("HOME");
        if (!home || !home[0]) return -1;
        int used = snprintf(fallback, sizeof(fallback), "%s/.local/share", home);
        if (used < 0 || (size_t)used >= sizeof(fallback)) return -1;
        base = fallback;
    }
    const char *folder = "nvr-monitor";
#endif
    if (!base || !base[0]) return -1;
    char directory[512];
    int used = snprintf(directory, sizeof(directory), "%s%c%s", base, NVR_SEPARATOR, folder);
    if (used < 0 || (size_t)used >= sizeof(directory) || make_directories(directory)) return -1;
    used = snprintf(path, path_size, "%s%ccameras.db", directory, NVR_SEPARATOR);
    return used >= 0 && (size_t)used < path_size ? 0 : -1;
}
