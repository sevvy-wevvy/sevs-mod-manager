#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"

Ui K;
Palette P;

R rect(double x, double y, double w, double h)
{
    R r = {x, y, w, h};
    return r;
}

R inset(R r, double d)
{
    return rect(r.x + d, r.y + d, r.w - d * 2, r.h - d * 2);
}

R cut_top(R *r, double h)
{
    R t = rect(r->x, r->y, r->w, h);
    r->y += h;
    r->h -= h;
    return t;
}

R cut_bottom(R *r, double h)
{
    R t = rect(r->x, r->y + r->h - h, r->w, h);
    r->h -= h;
    return t;
}

R cut_left(R *r, double w)
{
    R t = rect(r->x, r->y, w, r->h);
    r->x += w;
    r->w -= w;
    return t;
}

R cut_right(R *r, double w)
{
    R t = rect(r->x + r->w - w, r->y, w, r->h);
    r->w -= w;
    return t;
}

int inside(R r, double x, double y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static int clamp8(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

uint32_t col_rgb(int r, int g, int b)
{
    return 0xFF000000u | (uint32_t)clamp8(r) << 16 | (uint32_t)clamp8(g) << 8 | (uint32_t)clamp8(b);
}

uint32_t col_lighten(uint32_t c, int amt)
{
    return col_rgb((int)((c >> 16) & 255) + amt, (int)((c >> 8) & 255) + amt, (int)(c & 255) + amt);
}

uint32_t col_darken(uint32_t c, int amt)
{
    return col_lighten(c, -amt);
}

uint32_t col_lerp(uint32_t a, uint32_t b, double t)
{
    int ch[4];
    for (int i = 0; i < 4; i++) {
        int s = i * 8;
        double va = (double)((a >> s) & 255), vb = (double)((b >> s) & 255);
        ch[i] = clamp8((int)(va + (vb - va) * t + 0.5));
    }
    return (uint32_t)ch[3] << 24 | (uint32_t)ch[2] << 16 | (uint32_t)ch[1] << 8 | (uint32_t)ch[0];
}

uint32_t col_alpha(uint32_t c, int a)
{
    return (c & 0x00FFFFFFu) | (uint32_t)clamp8(a) << 24;
}

int col_dark(uint32_t c)
{
    return (int)((c >> 16) & 255) + (int)((c >> 8) & 255) + (int)(c & 255) < 384;
}

int col_parse(const char *hex, uint32_t *out)
{
    if (!hex) return 0;
    if (*hex == '#') hex++;
    if (strlen(hex) != 6) return 0;
    char *end = NULL;
    long v = strtol(hex, &end, 16);
    if (!end || *end) return 0;
    *out = 0xFF000000u | (uint32_t)v;
    return 1;
}

void col_hex(uint32_t c, char *out)
{
    snprintf(out, 8, "#%02X%02X%02X", (c >> 16) & 255, (c >> 8) & 255, c & 255);
}

static goome_font *load_font(int bold)
{
    goome_font *f = NULL;
#if defined(_WIN32)
    f = goome_font_system(bold ? "segoeuib" : "segoeui");
#elif defined(__APPLE__)
    f = goome_font_system(bold ? "helveticaneue" : NULL);
#else
    f = goome_font_system(bold ? "sans-serif:bold" : "sans-serif");
#endif
    return f ? f : goome_font_system(NULL);
}

void ui_init(goome *g, goome_ui *u)
{
    memset(&K, 0, sizeof(K));
    K.g = g;
    K.u = u;
    K.regular = load_font(0);
    K.bold = load_font(1);
#if defined(_WIN32)
    K.mono = goome_font_system("consola");
#elif defined(__APPLE__)
    K.mono = goome_font_system("menlo");
#else
    K.mono = goome_font_system("monospace");
#endif
}

void ui_begin_frame(goome_canvas *c, double w, double h)
{
    K.c = c;
    K.w = w;
    K.h = h;
    K.nclip = 0;
    K.cursor = GOOME_CURSOR_ARROW;
    K.now = goome_now_ms();
    K.animating = 0;
    K.frame++;
}

void ui_end_frame(void)
{
    goome_cursor(K.g, K.cursor);
    K.pressed = K.released = K.rpressed = K.rreleased = K.dbl = 0;
    K.wheel = 0;
    K.key = 0;
    K.mods = 0;
    K.typed[0] = 0;
}

void ui_event(const goome_event *ev)
{
    switch (ev->type) {
    case GOOME_MOTION:
        K.mx = ev->x, K.my = ev->y;
        break;
    case GOOME_DOWN:
        K.mx = ev->x, K.my = ev->y;
        if (ev->button == 1) K.down = 1, K.pressed = 1, K.px = ev->x, K.py = ev->y;
        if (ev->button == 3) K.rpressed = 1, K.px = ev->x, K.py = ev->y;
        break;
    case GOOME_UP:
        K.mx = ev->x, K.my = ev->y;
        if (ev->button == 1) K.down = 0, K.released = 1;
        if (ev->button == 3) K.rreleased = 1;
        break;
    case GOOME_SCROLL:
        K.mx = ev->x, K.my = ev->y;
        K.wheel += ev->dy;
        break;
    case GOOME_KEY:
        K.key = ev->key;
        K.mods = ev->mods;
        if (ev->text[0]) {
            size_t n = strlen(K.typed);
            snprintf(K.typed + n, sizeof(K.typed) - n, "%s", ev->text);
        }
        break;
    default:
        break;
    }
}

static int clipped_out(double x, double y)
{
    if (!K.nclip) return 0;
    double *c = K.clip[K.nclip - 1];
    return x < c[0] || y < c[1] || x >= c[0] + c[2] || y >= c[1] + c[3];
}

static int tut_blocks(double x, double y)
{
    return K.tut && !inside(K.hole, x, y);
}

int ui_hover(R r)
{
    if (K.blocked || tut_blocks(K.mx, K.my)) return 0;
    return inside(r, K.mx, K.my) && !clipped_out(K.mx, K.my);
}

int ui_click(R r)
{
    if (K.blocked || !K.released || tut_blocks(K.mx, K.my) || tut_blocks(K.px, K.py)) return 0;
    return inside(r, K.mx, K.my) && inside(r, K.px, K.py) && !clipped_out(K.mx, K.my);
}

int ui_rclick(R r)
{
    if (K.blocked || !K.rreleased || tut_blocks(K.mx, K.my)) return 0;
    return inside(r, K.mx, K.my) && !clipped_out(K.mx, K.my);
}

int ui_dbl(R r)
{
    return K.dbl && ui_hover(r);
}

int ui_held(R r)
{
    return K.down && !K.blocked && inside(r, K.px, K.py) && ui_hover(r);
}

void ui_cursor(int shape)
{
    K.cursor = shape;
}

void ui_clip(R r)
{
    double x = r.x, y = r.y, w = r.w, h = r.h;
    if (K.nclip) {
        double *p = K.clip[K.nclip - 1];
        double x1 = x + w, y1 = y + h;
        if (x < p[0]) x = p[0];
        if (y < p[1]) y = p[1];
        if (x1 > p[0] + p[2]) x1 = p[0] + p[2];
        if (y1 > p[1] + p[3]) y1 = p[1] + p[3];
        w = x1 - x < 0 ? 0 : x1 - x;
        h = y1 - y < 0 ? 0 : y1 - y;
    }
    if (K.nclip < 16) {
        double *d = K.clip[K.nclip++];
        d[0] = x, d[1] = y, d[2] = w, d[3] = h;
    }
    goome_clip_push(K.c, x, y, w, h);
}

void ui_unclip(void)
{
    if (K.nclip > 0) K.nclip--;
    goome_clip_pop(K.c);
}

int ui_wheel(R r)
{
    if (K.blocked || !K.wheel) return 0;
    return inside(r, K.mx, K.my) && !clipped_out(K.mx, K.my) ? K.wheel : 0;
}

static goome_font *fnt(int bold)
{
    if (bold == 2 && K.mono) return K.mono;
    return bold == 1 && K.bold ? K.bold : K.regular;
}

double tx_w(const char *s, double size, int bold)
{
    if (!s || !*s) return 0;
    return goome_text_measure(fnt(bold), size, s, NULL);
}

double tx_h(double size)
{
    return goome_text_line_height(K.regular, size);
}

void tx(double x, double y, const char *s, double size, uint32_t c, int bold)
{
    if (!s || !*s) return;
    goome_text(K.c, fnt(bold), size, x, y, s, c);
}

static double text_x(R r, double w, int align)
{
    if (align == AL_CENTER) return r.x + (r.w - w) / 2;
    if (align == AL_RIGHT) return r.x + r.w - w;
    return r.x;
}

void tx_in(R r, const char *s, double size, uint32_t c, int bold, int align)
{
    if (!s || !*s) return;
    double w = tx_w(s, size, bold), h = tx_h(size);
    tx(text_x(r, w, align), r.y + (r.h - h) / 2, s, size, c, bold);
}

void fmt_ellipsis(const char *s, double size, int bold, double maxw, char *out, size_t cap)
{
    snprintf(out, cap, "%s", s);
    if (tx_w(out, size, bold) <= maxw) return;
    size_t n = strlen(out);
    while (n > 0) {
        n--;
        while (n > 0 && (out[n] & 0xC0) == 0x80) n--;
        char t[512];
        snprintf(t, sizeof(t), "%.*s...", (int)n, s);
        if (tx_w(t, size, bold) <= maxw || n == 0) {
            snprintf(out, cap, "%s", t);
            return;
        }
    }
}

void tx_fit(R r, const char *s, double size, uint32_t c, int bold, int align)
{
    char t[512];
    fmt_ellipsis(s, size, bold, r.w, t, sizeof(t));
    tx_in(r, t, size, c, bold, align);
}

static int wrap_step(const char *s, double size, int bold, double w, int *pos, char *line, size_t cap)
{
    if (!s[*pos]) return 0;
    int start = *pos, last_space = -1, i = start;
    while (s[i] && s[i] != '\n') {
        char t[1024];
        int len = i - start + 1;
        if (len >= (int)sizeof(t) - 4) break;
        memcpy(t, s + start, (size_t)len);
        t[len] = 0;
        if (s[i] == ' ') last_space = i;
        if (tx_w(t, size, bold) > w && i > start) {
            int cut = last_space > start ? last_space : i;
            snprintf(line, cap, "%.*s", cut - start, s + start);
            *pos = last_space > start ? cut + 1 : cut;
            return 1;
        }
        i++;
        while (s[i] && (s[i] & 0xC0) == 0x80) i++;
    }
    snprintf(line, cap, "%.*s", i - start, s + start);
    *pos = s[i] == '\n' ? i + 1 : i;
    return 1;
}

void tx_wrap(R r, const char *s, double size, uint32_t c, int bold, double *used)
{
    int pos = 0;
    double y = r.y, lh = tx_h(size) + 3;
    char line[1024];
    while (wrap_step(s, size, bold, r.w, &pos, line, sizeof(line))) {
        if (y + lh >= r.y - 1 && y <= K.h) tx(r.x, y, line, size, c, bold);
        y += lh;
    }
    if (used) *used = y - r.y;
}

int tx_wrap_lines(const char *s, double size, int bold, double w)
{
    int pos = 0, n = 0;
    char line[1024];
    while (wrap_step(s, size, bold, w, &pos, line, sizeof(line))) n++;
    return n;
}

void fill(R r, double rad, uint32_t c)
{
    if (r.w <= 0 || r.h <= 0) return;
    if (K.square) rad = 0;
    if (rad <= 0) goome_draw_rect(K.c, r.x, r.y, r.w, r.h, c);
    else goome_draw_round_rect(K.c, r.x, r.y, r.w, r.h, rad, c);
}

void fill_rd(R r, goome_radii rd, uint32_t c)
{
    if (r.w <= 0 || r.h <= 0) return;
    if (K.square) rd = goome_radius(0);
    goome_draw_rrect(K.c, r.x, r.y, r.w, r.h, rd, c);
}

void stroke(R r, double rad, double w, uint32_t c)
{
    if (K.square) rad = 0;
    goome_draw_rrect_line(K.c, r.x, r.y, r.w, r.h, goome_radius(rad), w, c);
}

void stroke_rd(R r, goome_radii rd, double w, uint32_t c)
{
    if (K.square) rd = goome_radius(0);
    goome_draw_rrect_line(K.c, r.x, r.y, r.w, r.h, rd, w, c);
}

void hline(double x0, double x1, double y, uint32_t c)
{
    goome_draw_rect(K.c, x0, y, x1 - x0, 1, c);
}

void vline(double x, double y0, double y1, uint32_t c)
{
    goome_draw_rect(K.c, x, y0, 1, y1 - y0, c);
}

void shadow(R r, double rad, double blur, uint32_t c)
{
    goome_draw_shadow(K.c, r.x, r.y, r.w, r.h, rad, blur, c);
}

void ic_play(R r, uint32_t c)
{
    goome_draw_triangle(K.c, r.x, r.y, r.x, r.y + r.h, r.x + r.w, r.y + r.h / 2, c);
}

void ic_stop(R r, uint32_t c)
{
    fill(r, 1.5, c);
}

void ic_chev_down(R r, uint32_t c)
{
    double cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    goome_draw_line(K.c, cx - r.w / 2, cy - r.h / 4, cx, cy + r.h / 4, 1.6, c);
    goome_draw_line(K.c, cx, cy + r.h / 4, cx + r.w / 2, cy - r.h / 4, 1.6, c);
}

void ic_up_tri(R r, uint32_t c)
{
    goome_draw_triangle(K.c, r.x, r.y + r.h, r.x + r.w, r.y + r.h, r.x + r.w / 2, r.y, c);
}

void ic_refresh(R r, uint32_t c)
{
    double cx = r.x + r.w / 2, cy = r.y + r.h / 2, rad = r.w / 2 - 1.2;
    goome_draw_arc(K.c, cx, cy, rad, 1.6, -0.4, 4.6, c);
    goome_draw_triangle(K.c, cx + rad - 3.5, cy - 3.5, cx + rad + 3.5, cy - 3.5, cx + rad, cy + 1.5, c);
}

void ic_trash(R r, uint32_t c)
{
    double x = r.x, y = r.y, w = r.w, h = r.h;
    goome_draw_rect(K.c, x, y + h * 0.18, w, 1.6, c);
    goome_draw_rect(K.c, x + w * 0.35, y, w * 0.3, 1.6, c);
    goome_draw_rrect_line(K.c, x + w * 0.12, y + h * 0.28, w * 0.76, h * 0.72, goome_radius(1.5), 1.5, c);
    goome_draw_rect(K.c, x + w * 0.38, y + h * 0.42, 1.3, h * 0.4, c);
    goome_draw_rect(K.c, x + w * 0.58, y + h * 0.42, 1.3, h * 0.4, c);
}

void ic_ext(R r, uint32_t c)
{
    goome_draw_line(K.c, r.x + 1, r.y + r.h - 1, r.x + r.w - 1, r.y + 1, 1.5, c);
    goome_draw_line(K.c, r.x + r.w * 0.35, r.y + 1, r.x + r.w - 1, r.y + 1, 1.5, c);
    goome_draw_line(K.c, r.x + r.w - 1, r.y + 1, r.x + r.w - 1, r.y + r.h * 0.65, 1.5, c);
}

void ic_warn(R r, uint32_t c)
{
    goome_draw_triangle(K.c, r.x + r.w / 2, r.y, r.x, r.y + r.h, r.x + r.w, r.y + r.h, c);
    goome_draw_rect(K.c, r.x + r.w / 2 - 0.8, r.y + r.h * 0.38, 1.6, r.h * 0.32, P.bg);
    goome_draw_rect(K.c, r.x + r.w / 2 - 0.8, r.y + r.h * 0.78, 1.6, 1.6, P.bg);
}

void ic_check(R r, uint32_t c)
{
    goome_draw_line(K.c, r.x + 1, r.y + r.h * 0.55, r.x + r.w * 0.4, r.y + r.h - 1.5, 1.8, c);
    goome_draw_line(K.c, r.x + r.w * 0.4, r.y + r.h - 1.5, r.x + r.w - 1, r.y + 1.5, 1.8, c);
}

void ic_cross(R r, uint32_t c)
{
    goome_draw_line(K.c, r.x, r.y, r.x + r.w, r.y + r.h, 1.6, c);
    goome_draw_line(K.c, r.x + r.w, r.y, r.x, r.y + r.h, 1.6, c);
}

int w_button_col(R r, const char *label, uint32_t fillc, uint32_t hoverc, uint32_t fg, goome_radii rd)
{
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    uint32_t f = hov ? hoverc : fillc;
    if ((f >> 24) != 0) fill_rd(r, rd, f);
    if (label && *label) tx_fit(inset(r, 4), label, 13, fg, 0, AL_CENTER);
    return ui_click(r);
}

int w_button_rd(R r, const char *label, int style, int enabled, goome_radii rd)
{
    int dark = col_dark(P.bg);
    uint32_t fillc, hoverc, fg = P.text;
    if (style == BTN_ACCENT) {
        fillc = P.accent, hoverc = col_lighten(P.accent, 18), fg = P.accent_text;
    } else if (style == BTN_OUTLINE) {
        fillc = 0, hoverc = P.alt;
    } else if (style == BTN_GHOST) {
        fillc = 0, hoverc = P.alt;
    } else {
        fillc = dark ? col_lighten(P.alt, 20) : col_darken(P.alt, 10);
        hoverc = dark ? col_lighten(P.alt, 34) : col_darken(P.alt, 20);
    }
    if (!enabled) {
        fg = col_alpha(fg, 110);
        if (fillc) fillc = col_lerp(fillc, P.surface, 0.5);
        hoverc = fillc;
    }
    int hov = enabled && ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    fill_rd(r, rd, hov ? hoverc : fillc);
    if (style == BTN_OUTLINE) stroke_rd(inset(r, 0.6), rd, 1.2, hov ? col_lighten(P.border, 20) : P.border);
    if (label && *label) tx_fit(inset(r, 4), label, 13, fg, 0, AL_CENTER);
    return enabled && ui_click(r);
}

int w_button(R r, const char *label, int style, int enabled)
{
    return w_button_rd(r, label, style, enabled, goome_radius(8));
}

int w_icon_button(R r, int icon, uint32_t fg)
{
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND), fill(r, 6, P.alt);
    R ir = inset(r, (r.h - 14) / 2);
    if (icon == 1) ic_trash(ir, fg);
    else if (icon == 2) ic_refresh(ir, fg);
    else ic_cross(ir, fg);
    return ui_click(r);
}

int w_check(double x, double y, const char *label, int *v)
{
    double w = 18 + 8 + tx_w(label, 13, 0);
    R r = rect(x, y, w, 22);
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    R b = rect(x, y + 2, 18, 18);
    fill(b, 4, *v ? P.accent : (hov ? col_lighten(P.alt, 10) : P.alt));
    if (!*v) stroke(inset(b, 0.5), 4, 1, P.border);
    if (*v) ic_check(inset(b, 4), P.accent_text);
    tx_in(rect(x + 26, y, w, 22), label, 13, P.text, 0, AL_LEFT);
    if (ui_click(r)) {
        *v = !*v;
        return 1;
    }
    return 0;
}

int w_radio(double x, double y, const char *label, int *v, int value)
{
    double w = 18 + 8 + tx_w(label, 13, 0);
    R r = rect(x, y, w, 22);
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    goome_draw_circle(K.c, x + 9, y + 11, 9, *v == value ? P.accent : P.alt);
    if (*v != value) goome_draw_ring(K.c, x + 9, y + 11, 8.5, 1, P.border);
    else goome_draw_circle(K.c, x + 9, y + 11, 3.2, P.accent_text);
    tx_in(rect(x + 26, y, w, 22), label, 13, P.text, 0, AL_LEFT);
    if (ui_click(r) && *v != value) {
        *v = value;
        return 1;
    }
    return 0;
}

int w_toggle(R r, int on)
{
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    double tw = 38, th = 20;
    R t = rect(r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2, tw, th);
    fill(t, th / 2, on ? P.accent : col_lighten(P.alt, 12));
    double kx = on ? t.x + tw - th + 2 : t.x + 2;
    goome_draw_circle(K.c, kx + (th - 4) / 2, t.y + th / 2, (th - 4) / 2, 0xFFFFFFFF);
    return ui_click(r);
}

void w_progress(R r, int pct)
{
    fill(r, r.h / 2, P.alt);
    if (pct > 0) fill(rect(r.x, r.y, r.w * (pct > 100 ? 100 : pct) / 100.0, r.h), r.h / 2, P.accent);
}

static char tip_text[400];
static R tip_rect;
static long tip_since;
static int tip_on;

void w_tooltip(R r, const char *s)
{
    if (!s || !*s || !ui_hover(r)) return;
    if (!tip_on || tip_rect.x != r.x || tip_rect.y != r.y) tip_since = K.now;
    tip_rect = r;
    tip_on = 2;
    snprintf(tip_text, sizeof(tip_text), "%s", s);
}

void w_tooltip_overlay(void)
{
    if (tip_on == 2 && K.now - tip_since > 450) {
        double w = 0, y;
        char line[400];
        int pos = 0, n = 0;
        const char *s = tip_text;
        while (s[pos]) {
            int e = pos;
            while (s[e] && s[e] != '\n') e++;
            snprintf(line, sizeof(line), "%.*s", e - pos, s + pos);
            double lw = tx_w(line, 12, 0);
            if (lw > w) w = lw;
            n++;
            pos = s[e] ? e + 1 : e;
        }
        double h = n * 16 + 12;
        double x = K.mx + 12;
        y = K.my + 18;
        if (x + w + 16 > K.w) x = K.w - w - 20;
        if (y + h > K.h) y = K.my - h - 8;
        R b = rect(x, y, w + 16, h);
        shadow(b, 6, 8, 0x66000000);
        fill(b, 6, col_lighten(P.surface, 14));
        stroke(inset(b, 0.5), 6, 1, P.border);
        pos = 0;
        int i = 0;
        while (s[pos]) {
            int e = pos;
            while (s[e] && s[e] != '\n') e++;
            snprintf(line, sizeof(line), "%.*s", e - pos, s + pos);
            tx(x + 8, y + 6 + i * 16, line, 12, P.text, 0);
            i++;
            pos = s[e] ? e + 1 : e;
        }
    }
    if (tip_on) tip_on--;
}

void refresh_canvas(void)
{
    K.c = goome_ui_canvas(K.u);
    int n = K.nclip;
    for (int i = 0; i < n; i++) goome_clip_push(K.c, K.clip[i][0], K.clip[i][1], K.clip[i][2], K.clip[i][3]);
}

uint32_t input_bg;

int w_input(const char *id, R r, char *buf, int cap, const char *hint, int flags, goome_radii rd)
{
    uint32_t bg = input_bg;
    int clipped = 0;
    if (K.nclip) {
        double *c = K.clip[K.nclip - 1];
        clipped = r.y < c[1] || r.y + r.h > c[1] + c[3] || r.x < c[0] || r.x + r.w > c[0] + c[2];
    }
    if (K.blocked || clipped) {
        fill_rd(r, rd, bg ? bg : P.alt);
        const char *shown = buf[0] ? buf : hint;
        if (shown) tx_fit(rect(r.x + 10, r.y, r.w - 20, r.h), shown, 13, buf[0] ? P.text : P.sub, 0, AL_LEFT);
        return 0;
    }
    goome_ui *u = K.u;
    goome_ui_push_color(u, GOOME_S_INPUT_BG, bg ? bg : P.alt);
    goome_ui_push_color(u, GOOME_S_BORDER, bg ? bg : P.alt);
    goome_ui_push_color(u, GOOME_S_TEXT, P.text);
    goome_ui_push_color(u, GOOME_S_TEXT_DIM, P.sub);
    goome_ui_push_color(u, GOOME_S_FOCUS, P.accent);
    goome_ui_push_color(u, GOOME_S_SELECT, col_alpha(P.accent, 120));
    goome_ui_push_color(u, GOOME_S_CARET, P.text);
    goome_ui_push_num(u, GOOME_S_FONT_SIZE, 13);
    goome_ui_push_radii(u, rd);
    goome_ui_push_id(u, id);
    goome_ui_pos(u, r.x, r.y);
    goome_ui_size(u, r.w, r.h);
    int ch = goome_ui_input_ex(u, "##in", buf, cap, flags, hint);
    goome_ui_pop_id(u);
    goome_ui_pop(u, 9);
    refresh_canvas();
    return ch;
}

typedef struct {
    int open;
    char id[48];
    R r;
    const char *const *items;
    int n;
    int *sel;
    goome_radii rd;
    int changed;
    int frame;
} Drop;

int dd_fill;
static Drop dd;
static int dd_changed;

int w_dropdown_open(void)
{
    return dd.open;
}

void w_close_popups(void)
{
    dd.open = 0;
}

int w_dropdown(const char *id, R r, const char *const *items, int n, int *sel, goome_radii rd)
{
    int mine = dd.open && strcmp(dd.id, id) == 0;
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    if (dd_fill) fill_rd(r, rd, hov || mine ? P.border : P.alt);
    else if (hov || mine) fill_rd(r, rd, P.alt);
    int s = *sel;
    if (s >= 0 && s < n) tx_fit(rect(r.x + 12, r.y, r.w - 40, r.h), items[s], 13, P.text, 0, AL_LEFT);
    ic_chev_down(rect(r.x + r.w - 26, r.y + r.h / 2 - 3, 10, 6), P.text);
    if (ui_click(r) && !dd.open) {
        dd.open = 1;
        snprintf(dd.id, sizeof(dd.id), "%s", id);
        dd.r = r;
        dd.items = items;
        dd.n = n;
        dd.sel = sel;
        dd.rd = rd;
        dd.frame = K.frame;
    }
    if (dd_changed && strcmp(dd.id, id) == 0) {
        dd_changed = 0;
        return 1;
    }
    return 0;
}

void w_dropdown_overlay(void)
{
    if (!dd.open) return;
    double ih = 32, w = dd.r.w;
    double h = ih * dd.n + 8;
    double y = dd.r.y + dd.r.h + 2;
    if (y + h > K.h - 4) y = K.h - h - 4;
    R b = rect(dd.r.x, y, w < 150 ? 150 : w, h);
    int saved = K.blocked;
    K.blocked = 0;
    shadow(b, 8, 12, 0x77000000);
    fill(b, 8, col_lighten(P.surface, 6));
    stroke(inset(b, 0.5), 8, 1, P.border);
    for (int i = 0; i < dd.n; i++) {
        R ir = rect(b.x + 4, b.y + 4 + i * ih, b.w - 8, ih);
        int hov = ui_hover(ir);
        if (hov) fill(ir, 6, P.alt);
        if (i == *dd.sel) fill(rect(ir.x + 2, ir.y + 8, 3, ir.h - 16), 1.5, P.accent);
        tx_fit(rect(ir.x + 14, ir.y, ir.w - 20, ir.h), dd.items[i], 13, P.text, 0, AL_LEFT);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        if (ui_click(ir)) {
            *dd.sel = i;
            dd_changed = 1;
            dd.open = 0;
        }
    }
    if (K.pressed && !inside(b, K.mx, K.my) && K.frame != dd.frame) dd.open = 0;
    K.blocked = saved;
}

void sc_begin(Scroll *s, R view, double content_h)
{
    s->max = content_h > view.h ? content_h - view.h : 0;
    int wh = ui_wheel(view);
    if (wh) s->off += wh * 56;
    if (s->off > s->max) s->off = s->max;
    if (s->off < 0) s->off = 0;
    ui_clip(view);
}

double sc_top(const Scroll *s, R view)
{
    return view.y - s->off;
}

void sc_end(Scroll *s, R view, double content_h)
{
    ui_unclip();
    if (s->max <= 0) return;
    double tw = 6, trk = view.h - 4;
    double th = trk * view.h / content_h;
    if (th < 28) th = 28;
    double ty = view.y + 2 + (trk - th) * (s->off / s->max);
    R thumb = rect(view.x + view.w - tw - 3, ty, tw, th);
    R hit = rect(view.x + view.w - 14, view.y, 14, view.h);
    int hov = ui_hover(hit);
    if (K.pressed && hov && !K.blocked) {
        s->dragging = 1;
        s->drag = K.my - ty;
    }
    if (!K.down) s->dragging = 0;
    if (s->dragging && trk > th) {
        double t = (K.my - s->drag - view.y - 2) / (trk - th);
        s->off = (t < 0 ? 0 : t > 1 ? 1 : t) * s->max;
    }
    fill(thumb, tw / 2, s->dragging || hov ? col_lighten(P.border, 30) : P.border);
}

void sc_to(Scroll *s, R view, double y, double h)
{
    if (y < s->off) s->off = y;
    if (y + h > s->off + view.h) s->off = y + h - view.h;
}

int tb_style = BTN_GHOST;

int tb_button(double *x, double y, const char *label, int enabled, int icon)
{
    double w = tx_w(label, 13, 0) + 22 + (icon ? 18 : 0);
    R r = rect(*x, y, w, 38);
    int style = enabled ? tb_style : BTN_GHOST;
    int hit = w_button_rd(r, "", style, enabled, (goome_radii){0, 0, 8, 8});
    uint32_t fg = !enabled ? col_alpha(P.text, 110) : style == BTN_ACCENT ? P.accent_text : P.text;
    R tr = icon == 1 ? rect(r.x + 18, r.y, r.w - 18, r.h) : icon == 2 ? rect(r.x, r.y, r.w - 18, r.h) : r;
    tx_fit(inset(tr, 4), label, 13, fg, 0, AL_CENTER);
    if (icon == 1) ic_refresh(rect(r.x + 9, r.y + 13, 12, 12), fg);
    else if (icon == 2) ic_ext(rect(r.x + r.w - 20, r.y + 14, 10, 10), fg);
    *x += w + 6;
    return hit;
}

int tb_button_right(double *rx, double y, const char *label, int enabled, int icon)
{
    double w = tx_w(label, 13, 0) + 22 + (icon ? 18 : 0);
    *rx -= w;
    double x = *rx;
    int hit = tb_button(&x, y, label, enabled, icon);
    *rx -= 6;
    return hit;
}

void str_list(R view, const SmmStrsView *s, Scroll *sc, uint32_t color)
{
    double rh = 24;
    fill(view, 0, P.surface);
    sc_begin(sc, view, s->n * rh);
    for (int i = 0; i < s->n; i++) {
        double y = view.y + i * rh - sc->off;
        if (y + rh < view.y || y > view.y + view.h) continue;
        const char *line = s->v[i];
        double lx = view.x + 8;
        if (line[0] == 1) {
            line++;
            int sp = 0;
            while (line[sp] == ' ') sp++;
            ic_chev_down(rect(lx + sp * 3.3, y + rh / 2 - 2.5, 8, 5), P.sub);
            lx += 12;
        }
        tx_fit(rect(lx, y, view.x + view.w - lx - 8, rh), line, 12, color, 0, AL_LEFT);
    }
    sc_end(sc, view, s->n * rh);
}
