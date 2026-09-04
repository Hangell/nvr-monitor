#include "nvr/layout.h"

void nvr_layout_grid(size_t count, int width, int height,
                     NvrRect *rects, size_t rect_count) {
    nvr_layout_grid_slots(count, count, width, height, rects, rect_count);
}

void nvr_layout_grid_slots(size_t count, size_t slots, int width, int height,
                           NvrRect *rects, size_t rect_count) {
    if (!rects || !count || !rect_count || width <= 0 || height <= 0) return;
    if (count > rect_count) count = rect_count;
    if (slots < count) slots = count;
    if (slots > 9) slots = 9;
    int columns;
    int rows;
    if (slots <= 1) { columns = 1; rows = 1; }
    else if (slots <= 2) { columns = 2; rows = 1; }
    else if (slots <= 4) { columns = 2; rows = 2; }
    else if (slots <= 6) { columns = 3; rows = 2; }
    else if (slots <= 8) { columns = 4; rows = 2; }
    else { columns = 3; rows = 3; }
    for (size_t i = 0; i < count; ++i) {
        int col = (int)i % columns;
        int row = (int)i / columns;
        int x0 = col * width / columns;
        int x1 = (col + 1) * width / columns;
        int y0 = row * height / rows;
        int y1 = (row + 1) * height / rows;
        rects[i] = (NvrRect){x0, y0, x1 - x0, y1 - y0};
    }
}

NvrRect nvr_layout_fit(NvrRect area, int video_width, int video_height) {
    if (video_width <= 0 || video_height <= 0) return area;
    long by_width = (long)area.width * video_height;
    long by_height = (long)area.height * video_width;
    NvrRect result = area;
    if (by_width <= by_height) {
        result.height = (int)((long)area.width * video_height / video_width);
        result.y += (area.height - result.height) / 2;
    } else {
        result.width = (int)((long)area.height * video_width / video_height);
        result.x += (area.width - result.width) / 2;
    }
    return result;
}
