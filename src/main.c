#include "nvr/app.h"
#include "nvr/ui.h"
#include <stdio.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr, "Uso: %s [--config arquivo.json] [--configure]\n", program);
}

int main(int argc, char **argv) {
    const char *config = "config/cameras.json";
    int configure = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) config = argv[++i];
        else if (!strcmp(argv[i], "--configure")) configure = 1;
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }
    return configure ? nvr_ui_configure_file(config) : nvr_app_run(config);
}
