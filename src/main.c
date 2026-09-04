#include "nvr/app.h"
#include "nvr/paths.h"
#include <stdio.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr, "Uso: %s [--database cameras.db] [--config cameras.json]\n", program);
}

int main(int argc, char **argv) {
    char default_database[512];
    const char *config = NULL;
    for (int i = 1; i < argc; ++i) {
        if ((!strcmp(argv[i], "--config") || !strcmp(argv[i], "--database")) && i + 1 < argc) config = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }
    if (!config) {
        if (nvr_default_database_path(default_database, sizeof(default_database))) {
            fprintf(stderr, "Não foi possível determinar a pasta de dados do usuário.\n");
            return 1;
        }
        config = default_database;
    }
    return nvr_app_run(config);
}
