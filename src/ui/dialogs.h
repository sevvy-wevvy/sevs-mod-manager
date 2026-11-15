#ifndef SMM_DIALOGS_H
#define SMM_DIALOGS_H

#include "app.h"

#define DLG_DEPTH 5

enum { DK_NONE, DK_MSG, DK_PROMPT, DK_PICKER, DK_STEAM, DK_COMMUNITY, DK_EXPORT, DK_UPDATE, DK_WHATSNEW };

typedef struct {
    int kind;
    int tag;
    int buttons;
    int icon;
    char title[96];
    char text[1024];
    char input[256];
    int step;
    double t;
} Dlg;

Dlg *dlg_push(int kind);
void dlg_pop(void);
Dlg *dlg_top(void);

void draw_picker(Dlg *d);
void draw_steam(Dlg *d);
void draw_community(Dlg *d);
void draw_export(Dlg *d);

#endif
