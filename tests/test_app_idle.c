#include "nvr/app.h"
#include "nvr/database.h"
#include <SDL.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static atomic_int hidden;
static atomic_int presents;
Uint32 __real_SDL_GetWindowFlags(SDL_Window *window);
Uint32 __wrap_SDL_GetWindowFlags(SDL_Window *window) {
    Uint32 flags = __real_SDL_GetWindowFlags(window);
    return atomic_load(&hidden) ? flags | SDL_WINDOW_MINIMIZED : flags;
}
void __wrap_SDL_RenderPresent(SDL_Renderer *renderer) {
    (void)renderer;
    atomic_fetch_add(&presents, 1);
}
static void expose(void) {
    SDL_Event event = {.type = SDL_WINDOWEVENT};
    event.window.event = SDL_WINDOWEVENT_EXPOSED;
    assert(SDL_PushEvent(&event) == 1);
}
static int events(void *unused) {
    (void)unused;
    SDL_Delay(300);
    assert(atomic_load(&presents) >= 1);
    int idle = atomic_load(&presents);
    SDL_Delay(200);
    assert(atomic_load(&presents) == idle);
    atomic_store(&hidden, 1);
    expose();
    SDL_Delay(300);
    assert(atomic_load(&presents) == idle);
    atomic_store(&hidden, 0);
    expose();
    SDL_Delay(300);
    assert(atomic_load(&presents) == idle + 1);
    SDL_Event quit = {.type = SDL_QUIT};
    assert(SDL_PushEvent(&quit) == 1);
    expose(); /* A later event must not cancel the quit request. */
    return 0;
}
int main(void) {
    char path[] = "/tmp/nvr-idle-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    NvrDatabase db;
    char error[256];
    assert(nvr_database_open(&db, path, error, sizeof(error)) == 0);
    nvr_database_close(&db);
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    assert(SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) == 0);
    SDL_Thread *thread = SDL_CreateThread(events, "test-events", NULL);
    assert(thread);
    assert(nvr_app_run(path) == 0);
    SDL_WaitThread(thread, NULL);
    unlink(path);
    puts("Idle, minimized, restored and quit checks passed");
    return 0;
}
