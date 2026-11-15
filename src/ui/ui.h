#ifndef SMM_UI_H
#define SMM_UI_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../../goome.h"

#if defined(_WIN32)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

typedef struct {
    double x, y, w, h;
} R;

typedef struct {
    uint32_t bg, surface, alt, border, text, sub, accent, accent_text;
} Palette;

enum { BTN_SOLID, BTN_ACCENT, BTN_OUTLINE, BTN_GHOST };
enum { AL_LEFT, AL_CENTER, AL_RIGHT };

typedef struct {
    goome *g;
    goome_ui *u;
    goome_canvas *c;
    goome_font *regular, *bold, *mono;
    double w, h;
    double mx, my, px, py;
    int down, pressed, released, rpressed, rreleased, dbl;
    int wheel;
    int key, mods;
    char typed[16];
    int blocked;
    int tut;
    R hole;
    int cursor;
    long now;
    double clip[16][4];
    int nclip;
    int frame;
    int animating;
    int quit;
    int maximized;
    int square;
    int windowed;
} Ui;

extern Ui K;
extern Palette P;

R rect(double x, double y, double w, double h);
R inset(R r, double d);
R cut_top(R *r, double h);
R cut_bottom(R *r, double h);
R cut_left(R *r, double w);
R cut_right(R *r, double w);
int inside(R r, double x, double y);

uint32_t col_rgb(int r, int g, int b);
uint32_t col_lighten(uint32_t c, int amt);
uint32_t col_darken(uint32_t c, int amt);
uint32_t col_lerp(uint32_t a, uint32_t b, double t);
uint32_t col_alpha(uint32_t c, int a);
int col_dark(uint32_t c);
int col_parse(const char *hex, uint32_t *out);
void col_hex(uint32_t c, char *out);

void ui_init(goome *g, goome_ui *u);
void ui_begin_frame(goome_canvas *c, double w, double h);
void ui_end_frame(void);
void ui_event(const goome_event *ev);
int ui_hover(R r);
int ui_click(R r);
int ui_rclick(R r);
int ui_dbl(R r);
int ui_held(R r);
void ui_cursor(int shape);
void ui_clip(R r);
void ui_unclip(void);
int ui_wheel(R r);

double tx_w(const char *s, double size, int bold);
double tx_h(double size);
void tx(double x, double y, const char *s, double size, uint32_t c, int bold);
void tx_in(R r, const char *s, double size, uint32_t c, int bold, int align);
void tx_fit(R r, const char *s, double size, uint32_t c, int bold, int align);
void tx_wrap(R r, const char *s, double size, uint32_t c, int bold, double *used);
int tx_wrap_lines(const char *s, double size, int bold, double w);
void fmt_ellipsis(const char *s, double size, int bold, double maxw, char *out, size_t cap);

void fill(R r, double rad, uint32_t c);
void fill_rd(R r, goome_radii rd, uint32_t c);
void stroke(R r, double rad, double w, uint32_t c);
void stroke_rd(R r, goome_radii rd, double w, uint32_t c);
void hline(double x0, double x1, double y, uint32_t c);
void vline(double x, double y0, double y1, uint32_t c);
void shadow(R r, double rad, double blur, uint32_t c);

void ic_play(R r, uint32_t c);
void ic_stop(R r, uint32_t c);
void ic_chev_down(R r, uint32_t c);
void ic_up_tri(R r, uint32_t c);
void ic_refresh(R r, uint32_t c);
void ic_trash(R r, uint32_t c);
void ic_ext(R r, uint32_t c);
void ic_warn(R r, uint32_t c);
void ic_check(R r, uint32_t c);
void ic_cross(R r, uint32_t c);

int w_button(R r, const char *label, int style, int enabled);
int w_button_rd(R r, const char *label, int style, int enabled, goome_radii rd);
int w_button_col(R r, const char *label, uint32_t fill, uint32_t hover, uint32_t fg, goome_radii rd);
int w_icon_button(R r, int icon, uint32_t fg);
int w_check(double x, double y, const char *label, int *v);
int w_radio(double x, double y, const char *label, int *v, int value);
int w_toggle(R r, int on);
void w_progress(R r, int pct);
extern uint32_t input_bg;
int w_input(const char *id, R r, char *buf, int cap, const char *hint, int flags, goome_radii rd);
extern int dd_fill;
int w_dropdown(const char *id, R r, const char *const *items, int n, int *sel, goome_radii rd);
void w_dropdown_overlay(void);
int w_dropdown_open(void);
void w_close_popups(void);
void refresh_canvas(void);
void w_tooltip(R r, const char *s);
void w_tooltip_overlay(void);

typedef struct {
    double off;
    double max;
    double drag;
    int dragging;
} Scroll;

typedef struct {
    char **v;
    int n;
} SmmStrsView;

void sc_begin(Scroll *s, R view, double content_h);
void sc_end(Scroll *s, R view, double content_h);
double sc_top(const Scroll *s, R view);
void sc_to(Scroll *s, R view, double y, double h);

extern int tb_style;
int tb_button(double *x, double y, const char *label, int enabled, int icon);
int tb_button_right(double *rx, double y, const char *label, int enabled, int icon);
void str_list(R view, const SmmStrsView *s, Scroll *sc, uint32_t color);

#endif
