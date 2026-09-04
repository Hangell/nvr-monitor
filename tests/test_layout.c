#include "nvr/layout.h"
#include <assert.h>

int main(void) {
    NvrRect r[9];
    nvr_layout_grid(4, 1280, 720, r, 4);
    assert(r[0].x == 0 && r[0].y == 0 && r[0].width == 640 && r[0].height == 360);
    assert(r[3].x == 640 && r[3].y == 360);
    nvr_layout_grid(2, 1280, 720, r, 2);
    assert(r[0].width == 640 && r[0].height == 720 && r[1].x == 640);
    nvr_layout_grid(6, 1200, 800, r, 6);
    assert(r[5].x == 800 && r[5].y == 400 && r[5].height == 400);
    nvr_layout_grid(8, 1200, 800, r, 8);
    assert(r[7].x == 900 && r[7].y == 400 && r[7].width == 300);
    nvr_layout_grid(9, 900, 600, r, 9);
    assert(r[8].x == 600 && r[8].y == 400 && r[8].width == 300 && r[8].height == 200);
    NvrRect fit = nvr_layout_fit((NvrRect){0, 0, 400, 400}, 1920, 1080);
    assert(fit.x == 0 && fit.width == 400 && fit.height == 225 && fit.y == 87);
    return 0;
}
