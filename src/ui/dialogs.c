#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "dialogs.h"

static Dlg stack[DLG_DEPTH];
static int depth;
static int results[64];
static char prompt_out[64][256];
static int update_version;

Dlg *dlg_push(int kind)
{
    if (depth >= DLG_DEPTH) return &stack[DLG_DEPTH - 1];
    Dlg *d = &stack[depth++];
    memset(d, 0, sizeof(*d));
    d->kind = kind;
    return d;
}

void dlg_pop(void)
{
    if (depth > 0) depth--;
}

Dlg *dlg_top(void)
{
    return depth ? &stack[depth - 1] : NULL;
}

int dlg_any(void)
{
    return depth > 0;
}

void dlg_close_all(void)
{
    depth = 0;
}

void dlg_scrim(void)
{
    goome_draw_rect(K.c, 0, 0, K.w, K.h, 0x99000000);
}

int dlg_frame(const char *title, double w, double h, R *content, int closable)
{
    R b = rect((int)((K.w - w) / 2), (int)((K.h - h) / 2), w, h);
    shadow(b, 12, 24, 0xAA000000);
    fill(b, 12, P.bg);
    stroke(inset(b, 0.5), 12, 1, P.border);
    R head = rect(b.x, b.y, b.w, 40);
    fill_rd(head, (goome_radii){12, 12, 0, 0}, P.surface);
    hline(b.x, b.x + b.w, b.y + 40, P.border);
    tx_in(rect(b.x + 16, b.y, b.w - 60, 40), title, 13, P.text, 1, AL_LEFT);
    int closed = 0;
    if (closable) {
        R cx = rect(b.x + b.w - 38, b.y + 6, 28, 28);
        int hov = ui_hover(cx);
        if (hov) fill(cx, 6, P.alt), ui_cursor(GOOME_CURSOR_HAND);
        ic_cross(rect(cx.x + 9, cx.y + 9, 10, 10), hov ? P.text : P.sub);
        if (ui_click(cx)) closed = 1;
    }
    if (content) *content = rect(b.x + 16, b.y + 52, b.w - 32, b.h - 52 - 16);
    return closed;
}

static void draw_msg_icon(R r, int icon)
{
    uint32_t c = icon == DI_ERROR ? 0xFFDC4B4B : icon == DI_WARN ? 0xFFF5B83D : icon == DI_QUESTION ? P.accent : 0xFF3B82F6;
    if (icon == DI_WARN) {
        ic_warn(inset(r, 4), c);
        return;
    }
    goome_draw_circle(K.c, r.x + r.w / 2, r.y + r.h / 2, r.w / 2, c);
    const char *g = icon == DI_ERROR ? "x" : icon == DI_QUESTION ? "?" : "i";
    tx_in(r, g, 17, 0xFFFFFFFF, 1, AL_CENTER);
}

void dlg_message(int tag, const char *title, const char *msg, int buttons, int icon)
{
    Dlg *d = dlg_push(DK_MSG);
    d->tag = tag;
    d->buttons = buttons;
    d->icon = icon;
    snprintf(d->title, sizeof(d->title), "%s", title);
    snprintf(d->text, sizeof(d->text), "%s", msg);
}

void dlg_prompt(int tag, const char *title, const char *label, const char *def)
{
    Dlg *d = dlg_push(DK_PROMPT);
    d->tag = tag;
    snprintf(d->title, sizeof(d->title), "%s", title);
    snprintf(d->text, sizeof(d->text), "%s", label);
    snprintf(d->input, sizeof(d->input), "%s", def ? def : "");
}

int dlg_result(int tag)
{
    int r = results[tag & 63];
    results[tag & 63] = 0;
    return r;
}

int dlg_prompt_result(int tag, char *out, size_t n)
{
    int r = results[tag & 63];
    if (!r) return 0;
    results[tag & 63] = 0;
    if (r == DR_OK) snprintf(out, n, "%s", prompt_out[tag & 63]);
    return r == DR_OK ? 1 : -1;
}

static void finish(Dlg *d, int res)
{
    results[d->tag & 63] = res;
    if (d->kind == DK_PROMPT) snprintf(prompt_out[d->tag & 63], 256, "%s", d->input);
    dlg_pop();
}

static int button_row(R r, const char *a, const char *b, const char *c, int accent_idx)
{
    const char *l[3] = {a, b, c};
    int n = (a != NULL) + (b != NULL) + (c != NULL);
    double w = 92, g = 8, x = r.x + r.w - (w * n + g * (n - 1));
    int hit = 0;
    for (int i = 0; i < n; i++) {
        R br = rect(x + i * (w + g), r.y, w, 32);
        if (w_button(br, l[i], i == accent_idx ? BTN_ACCENT : BTN_SOLID, 1)) hit = i + 1;
    }
    return hit;
}

static void draw_msg(Dlg *d)
{
    double w = 440;
    int lines = tx_wrap_lines(d->text, 13, 0, w - 32 - 56);
    double th = lines * (tx_h(13) + 3);
    if (th < 40) th = 40;
    double h = 52 + th + 24 + 32 + 16;
    R c;
    int closed = dlg_frame(d->title, w, h, &c, 0);
    (void)closed;
    draw_msg_icon(rect(c.x, c.y + 2, 36, 36), d->icon);
    tx_wrap(rect(c.x + 52, c.y + 4, c.w - 52, th), d->text, 13, P.text, 0, NULL);
    R row = rect(c.x, c.y + c.h - 32, c.w, 32);
    int hit = 0;
    if (d->buttons == DB_OK) hit = button_row(row, "OK", NULL, NULL, 0) ? DR_OK : 0;
    else if (d->buttons == DB_YESNO) {
        int b = button_row(row, "Yes", "No", NULL, 0);
        hit = b == 1 ? DR_YES : b == 2 ? DR_NO : 0;
    } else {
        int b = button_row(row, "Yes", "No", "Cancel", 0);
        hit = b == 1 ? DR_YES : b == 2 ? DR_NO : b == 3 ? DR_CANCEL : 0;
    }
    if (!hit && K.key == GOOME_K_ENTER) hit = d->buttons == DB_OK ? DR_OK : DR_YES;
    if (!hit && K.key == GOOME_K_ESC) hit = d->buttons == DB_OK ? DR_OK : d->buttons == DB_YESNO ? DR_NO : DR_CANCEL;
    if (hit) finish(d, hit);
}

static void draw_prompt(Dlg *d)
{
    R c;
    int closed = dlg_frame(d->title, 380, 190, &c, 1);
    tx(c.x, c.y, d->text, 13, P.text, 0);
    w_input("prompt", rect(c.x, c.y + 24, c.w, 32), d->input, sizeof(d->input), NULL, 0, goome_radius(8));
    R row = rect(c.x, c.y + c.h - 32, c.w, 32);
    int b = button_row(row, "OK", "Cancel", NULL, 0);
    if (b == 1 || (!K.blocked && K.key == GOOME_K_ENTER)) finish(d, DR_OK);
    else if (b == 2 || closed || K.key == GOOME_K_ESC) finish(d, DR_CANCEL);
}

void dlg_update_open(int version)
{
    update_version = version;
    Dlg *d = dlg_push(DK_UPDATE);
    d->tag = TAG_UPDATE;
}

void dlg_whatsnew_open(void)
{
    Dlg *d = dlg_push(DK_WHATSNEW);
    d->tag = TAG_WHATSNEW;
}

static void draw_update(Dlg *d)
{
    R c;
    dlg_frame("Update Available", 400, 200, &c, 0);
    char msg[200];
    snprintf(msg, sizeof(msg), "A new version of Sev's Mod Manager is available (v%d, you're on v%d).", update_version, SMM_VERSION);
    tx_wrap(rect(c.x, c.y + 4, c.w, 80), msg, 13, P.text, 0, NULL);
    int b = button_row(rect(c.x, c.y + c.h - 32, c.w, 32), "Get It", "Not Now", NULL, 0);
    if (b == 1) {
        smm_open_url("https://github.com/sevvy-wevvy/sevs-mod-manager/releases/latest");
        finish(d, DR_YES);
    } else if (b == 2 || K.key == GOOME_K_ESC) finish(d, DR_NO);
}

static void draw_whatsnew(Dlg *d)
{
    R c;
    dlg_frame("What's New", 420, 250, &c, 0);
    tx(c.x, c.y, "What's New in Sev's Mod Manager", 15, P.text, 1);
    tx_wrap(rect(c.x, c.y + 32, c.w, 100), "Thanks for updating! Check the GitHub releases page for what's changed in this version.", 13, P.sub, 0, NULL);
    if (button_row(rect(c.x, c.y + c.h - 32, c.w, 32), "Nice", NULL, NULL, 0) || K.key == GOOME_K_ESC || K.key == GOOME_K_ENTER) finish(d, DR_OK);
}

void dlg_draw(void)
{
    if (!depth) return;
    int saved = K.blocked;
    int popup = w_dropdown_open();
    (void)popup;
    for (int i = 0; i < depth; i++) {
        Dlg *d = &stack[i];
        K.blocked = i != depth - 1;
        dlg_scrim();
        switch (d->kind) {
        case DK_MSG: draw_msg(d); break;
        case DK_PROMPT: draw_prompt(d); break;
        case DK_PICKER: draw_picker(d); break;
        case DK_STEAM: draw_steam(d); break;
        case DK_COMMUNITY: draw_community(d); break;
        case DK_EXPORT: draw_export(d); break;
        case DK_UPDATE: draw_update(d); break;
        case DK_WHATSNEW: draw_whatsnew(d); break;
        default: break;
        }
        if (i >= depth) break;
    }
    K.blocked = saved;
}
