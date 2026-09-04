#ifndef NVR_LAYOUT_H
#define NVR_LAYOUT_H

#include <stddef.h>

typedef struct { int x, y, width, height; } NvrRect;

void nvr_layout_grid(size_t camera_count, int width, int height,
                     NvrRect *rects, size_t rect_count);
void nvr_layout_grid_slots(size_t camera_count, size_t slot_count,
                           int width, int height, NvrRect *rects, size_t rect_count);
NvrRect nvr_layout_fit(NvrRect area, int video_width, int video_height);

#endif
