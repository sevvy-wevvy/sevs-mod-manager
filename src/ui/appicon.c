#include <stdlib.h>

#include "app.h"
#include "appicon_data.h"

int app_icon_draw(R r, double radius)
{
    static goome_bitmap bm;
    static int state;
    if (!state) {
        state = goome_bitmap_load_mem(appicon_png, sizeof(appicon_png), &bm) == GOOME_OK ? 1 : -1;
    }
    if (state < 0) return 0;
    goome_draw_bitmap(K.c, &bm, r.x, r.y, r.w, r.h, goome_radius(radius), 0xFFFFFFFF);
    return 1;
}
