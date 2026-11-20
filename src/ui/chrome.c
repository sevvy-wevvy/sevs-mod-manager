#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "../core/internal.h"

#define TITLE_H 40
#define TAB_H 38
#define STATUS_H 24
#define WIN_RAD 14
#define SIDE_W 200
#define R2_TOP 30
#define MONKE_TITLE_H 30
#define MONKE_TAB_H 38

static const char *const NAV[TAB_COUNT] = {"Mods", "Installed", "Logs", "Loadouts", "Modpacks", "Config", "Settings"};

static double ul_x, ul_w, ul_tx, ul_tw;
static int ul_init;
static int starting, running;
static long launched_at, last_poll, starting_since;
static int menu_open;
static int menu_frame;
static char menu_names[24][128];
static char menu_paths[24][1024];
static int menu_n;
static int menu_disabled[24];
static R menu_anchor;
static int menu_up;
static int light_hover[3];

static int layout(void)
{
    return smm_settings.layout;
}

static int file_exists(const char *p)
{
    return smm_os_is_file(p);
}

void play_state(int *s, int *r)
{
    *s = starting;
    *r = running;
}

static void build_menu(void)
{
    menu_n = 0;
    for (int i = 0; i < smm_preset_count() && menu_n < 24; i++) {
        const SmmPreset *p = smm_preset_at(i);
        if (strcmp(p->name, "Custom") == 0) continue;
        const char *path = NULL;
        for (int k = 0; k < p->n_paths; k++)
            if (file_exists(p->default_paths[k])) {
                path = p->default_paths[k];
                break;
            }
        if (!path) {
            const char *known = smm_map_get(&smm_settings.known_game_paths, p->name);
            if (known && file_exists(known)) path = known;
        }
        char steam_path[1024];
        if (!path && p->steam_app_id > 0 && p->n_paths > 0) {
            const char *base = strrchr(p->default_paths[0], '\\');
            if (!base) base = strrchr(p->default_paths[0], '/');
            base = base ? base + 1 : p->default_paths[0];
            if (smm_steam_find_game_path(p->steam_app_id, base, steam_path, sizeof(steam_path)) > 0) path = steam_path;
        }
        if (!path && strcasecmp(p->name, smm_settings.game_name) == 0 && file_exists(smm_settings.game_path)) path = smm_settings.game_path;
        if (!path) continue;
        snprintf(menu_names[menu_n], 128, "%s", p->name);
        snprintf(menu_paths[menu_n], 1024, "%s", path);
        menu_disabled[menu_n] = strcasecmp(p->name, smm_settings.game_name) == 0;
        menu_n++;
    }
    for (int i = 0; i < smm_settings.custom_games.n && menu_n < 24; i++) {
        if (!file_exists(smm_settings.custom_games.v[i])) continue;
        snprintf(menu_names[menu_n], 128, "%s", smm_settings.custom_games.k[i]);
        snprintf(menu_paths[menu_n], 1024, "%s", smm_settings.custom_games.v[i]);
        menu_disabled[menu_n] = strcasecmp(menu_names[menu_n], smm_settings.game_name) == 0;
        menu_n++;
    }
}

static void quick_switch(const char *path, const char *name)
{
    snprintf(smm_settings.game_path, sizeof(smm_settings.game_path), "%s", path);
    snprintf(smm_settings.game_name, sizeof(smm_settings.game_name), "%s", name);
    smm_state_save();
    smm_ensure_sb_folder();
    smm_dc_load_settings();
    app_game_changed();
    app_status("Switched to %s.", name);
}

static void menu_overlay(void)
{
    if (!menu_open) return;
    double ih = 30, w = 220;
    double h = (menu_n ? menu_n : 1) * ih + 8;
    R b = rect(menu_anchor.x + menu_anchor.w - w, menu_anchor.y + menu_anchor.h + 4, w, h);
    if (menu_up) b = rect(menu_anchor.x + 8, menu_anchor.y - h - 4, w, h);
    int saved = K.blocked;
    K.blocked = 0;
    shadow(b, 8, 12, 0x77000000);
    fill(b, 8, col_lighten(P.surface, 6));
    stroke(inset(b, 0.5), 8, 1, P.border);
    if (!menu_n) tx_in(rect(b.x + 12, b.y + 4, b.w - 24, ih), "No other games set up yet", 13, P.sub, 0, AL_LEFT);
    for (int i = 0; i < menu_n; i++) {
        R ir = rect(b.x + 4, b.y + 4 + i * ih, b.w - 8, ih);
        int hov = !menu_disabled[i] && ui_hover(ir);
        if (hov) fill(ir, 6, P.alt), ui_cursor(GOOME_CURSOR_HAND);
        draw_game_dot(rect(ir.x + 8, ir.y + 4, 22, 22), menu_names[i], menu_paths[i]);
        tx_fit(rect(ir.x + 38, ir.y, ir.w - 48, ir.h), menu_names[i], 13, menu_disabled[i] ? P.sub : P.text, 0, AL_LEFT);
        if (hov && ui_click(ir)) {
            quick_switch(menu_paths[i], menu_names[i]);
            menu_open = 0;
        }
    }
    if (K.pressed && !inside(b, K.mx, K.my) && K.frame != menu_frame) menu_open = 0;
    K.blocked = saved;
}

static void poll_game(void)
{
    if (K.now - last_poll < 1000) return;
    last_poll = K.now;
    int r = app_has_game() ? smm_game_running() > 0 : 0;
    if (starting) {
        if (r) starting = 0;
        else if (launched_at && K.now - launched_at > 30000) starting = 0, launched_at = 0;
    }
    if (r == running) return;
    if (!r && running && launched_at && K.now - launched_at < 10000) {
        char m[300];
        snprintf(m, sizeof(m), "%s closed just a few seconds after launching. It may have crashed. Check the log?", smm_settings.game_name);
        dlg_message(TAG_GAME_CLOSED, "Game Closed Quickly", m, DB_YESNO, DI_WARN);
    }
    launched_at = 0;
    running = r;
    logs_game_state(r);
}

static void do_launch(void)
{
    if (smm_game_launch() >= 0) {
        launched_at = K.now;
        if (smm_strs_find(&smm_settings.disable_starting_indicator, smm_settings.game_name, 1) < 0) {
            starting = 1;
            starting_since = K.now;
        }
    }
}

static void on_play(void)
{
    if (starting) return;
    if (running) {
        smm_game_stop();
        return;
    }
    if (!app_has_game()) return;
    if (proton_before_play(do_launch)) return;
    do_launch();
}

static void draw_play(R r)
{
    uint32_t tint = starting ? col_rgb(52, 120, 246) : running ? col_rgb(220, 60, 60) : col_rgb(60, 190, 100);
    uint32_t f = col_lerp(P.alt, tint, 0.3), h = col_lerp(P.alt, tint, 0.45);
    const char *label = starting ? "Starting..." : running ? "Stop" : "Play";
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    fill(r, 8, hov ? h : f);
    if (layout() == SMM_LAYOUT_MONKE) stroke(inset(r, 1), 0, 2, hov ? tint : col_lerp(P.border, tint, 0.5));
    double tw = tx_w(label, 12, 1), iw = starting ? 0 : 10;
    double x = r.x + (r.w - tw - (iw ? iw + 7 : 0)) / 2;
    if (!starting) {
        R ir = rect(x, r.y + (r.h - 10) / 2, 10, 10);
        if (running) ic_stop(inset(ir, 0.5), P.text);
        else ic_play(rect(ir.x + 1, ir.y - 0.5, 9, 11), P.text);
        x += iw + 7;
    }
    tx(x, r.y + (r.h - tx_h(12)) / 2, label, 12, P.text, 1);
    if (ui_click(r)) on_play();
}

static void light(int i, double x, double y, uint32_t c, int *action)
{
    R r = rect(x - 3, y - 3, 18, 18);
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    double target = hov ? 1 : 0;
    double g = (light_hover[i] ? 1.0 : 0.0);
    (void)g;
    if (hov) goome_draw_circle(K.c, x + 6, y + 6, 8, col_alpha(0xFFFFFFFF, 60));
    goome_draw_circle(K.c, x + 6, y + 6, 6, c);
    light_hover[i] = (int)target;
    if (ui_click(r)) *action = i + 1;
}

typedef struct {
    R title, tabs, side, body, status;
} Frame;

static Frame frame(void)
{
    Frame f;
    memset(&f, 0, sizeof(f));
    int lay = layout();
    if (lay == SMM_LAYOUT_R2MODMAN) {
        f.side = rect(0, 0, SIDE_W, K.h);
        f.title = rect(0, 0, SIDE_W, R2_TOP);
        f.status = rect(SIDE_W, K.h - STATUS_H, K.w - SIDE_W, STATUS_H);
        f.body = rect(SIDE_W, 0, K.w - SIDE_W, K.h - STATUS_H);
    } else if (lay == SMM_LAYOUT_MONKE) {
        f.title = rect(0, 0, K.w, MONKE_TITLE_H);
        f.tabs = rect(0, MONKE_TITLE_H, K.w, MONKE_TAB_H);
        f.status = rect(0, K.h - STATUS_H, K.w, STATUS_H);
        f.body = rect(0, MONKE_TITLE_H + MONKE_TAB_H + 1, K.w, K.h - MONKE_TITLE_H - MONKE_TAB_H - 1 - STATUS_H);
    } else {
        f.title = rect(0, 0, K.w, TITLE_H);
        f.tabs = rect(0, TITLE_H, K.w, TAB_H);
        f.status = rect(0, K.h - STATUS_H, K.w, STATUS_H);
        f.body = rect(0, TITLE_H + TAB_H + 1, K.w, K.h - TITLE_H - TAB_H - 1 - STATUS_H);
    }
    return f;
}

static void lights(R r)
{
    int act = 0;
    light(0, r.x + 14, r.y + 13, col_rgb(255, 95, 86), &act);
    light(1, r.x + 36, r.y + 13, col_rgb(255, 189, 46), &act);
    light(2, r.x + 58, r.y + 13, col_rgb(39, 201, 63), &act);
    if (act == 1) K.quit = 1;
    if (act == 2) goome_window_state(K.g, GOOME_STATE_MINIMIZED);
    if (act == 3) goome_window_state(K.g, K.maximized ? GOOME_STATE_NORMAL : GOOME_STATE_MAXIMIZED);
}

static void drag_zone(R drag)
{
    if (K.pressed && ui_hover(drag) && !K.blocked) goome_window_drag(K.g, 0);
    if (K.dbl && ui_hover(drag)) goome_window_state(K.g, K.maximized ? GOOME_STATE_NORMAL : GOOME_STATE_MAXIMIZED);
}

static void draw_title(R r, int rounded)
{
    fill_rd(r, (goome_radii){rounded ? WIN_RAD : 0, rounded ? WIN_RAD : 0, 0, 0}, P.surface);
    lights(r);
    tx_in(r, "Sev's Mod Manager", 13, P.text, 0, AL_CENTER);

    R box = rect(r.x + r.w - 200, r.y, 200, r.h);
    R iconbox = cut_right(&box, 32);
    int has = app_has_game();
    if (has) draw_game_dot(rect(iconbox.x + 6, r.y + (r.h - 20) / 2, 20, 20), smm_settings.game_name, smm_settings.game_path);
    char label[160];
    snprintf(label, sizeof(label), "%s", has ? smm_settings.game_name : "No game selected");
    R lr = rect(box.x, box.y, box.w - 8, box.h);
    tx_fit(lr, label, 11.3, P.sub, 0, AL_RIGHT);
    R hit = rect(box.x, r.y, 200, r.h);
    if (ui_hover(hit)) ui_cursor(GOOME_CURSOR_HAND);
    if (ui_click(hit)) {
        if (menu_open) menu_open = 0;
        else build_menu(), menu_open = 1, menu_up = 0, menu_anchor = hit, menu_frame = K.frame;
    }

    drag_zone(rect(r.x + 78, r.y, r.w - 78 - 200, r.h));
}

static R nav_rect(R strip, int i)
{
    if (layout() == SMM_LAYOUT_MONKE) return rect(strip.x + 6 + i * 90, strip.y, 90, strip.h);
    return rect(strip.x + 8 + i * 90, strip.y + 3, 90, 32);
}

static R side_nav_rect(int i)
{
    return rect(8, R2_TOP + 10 + i * 38, SIDE_W - 16, 36);
}

static R side_play_rect(void)
{
    return rect(8, K.h - 48 - 44 + 4, SIDE_W - 16, 36);
}

static R side_game_rect(void)
{
    return rect(0, K.h - 48, SIDE_W, 48);
}

static void nav_click(int i)
{
    if (!ui_click(tab_rect(i))) return;
    if (!tutorial_active() || i == TAB_MODS || i == TAB_INSTALLED) app_goto_tab(i);
}

static void draw_tabs(R r)
{
    int lay = layout();
    int mk = lay == SMM_LAYOUT_MONKE;
    fill(r, 0, P.surface);
    int cur = app_tab();
    for (int i = 0; i < TAB_COUNT; i++) {
        R b = nav_rect(r, i);
        int hov = ui_hover(b);
        if (hov) fill(b, 8, P.alt), ui_cursor(GOOME_CURSOR_HAND);
        if (mk) stroke(inset(b, 1), 0, 2, P.border);
        tx_in(b, NAV[i], 13, P.text, i == cur, AL_CENTER);
        nav_click(i);
    }
    R cb = nav_rect(r, cur);
    ul_tx = cb.x + 6 - r.x;
    ul_tw = cb.w - 12;
    if (!ul_init) ul_x = ul_tx, ul_w = ul_tw, ul_init = 1;
    ul_x += (ul_tx - ul_x) * 0.28;
    ul_w += (ul_tw - ul_w) * 0.28;
    if (fabs(ul_x - ul_tx) > 0.4 || fabs(ul_w - ul_tw) > 0.4) K.animating = 1;
    else ul_x = ul_tx, ul_w = ul_tw;
    fill(rect(r.x + ul_x, r.y + r.h - 4, ul_w, 3), 2, P.accent);
    draw_play(play_rect());
}

static void draw_sidebar(Frame f)
{
    fill(f.side, 0, P.surface);
    lights(f.title);
    drag_zone(rect(78, 0, SIDE_W - 78, R2_TOP));
    int cur = app_tab();
    for (int i = 0; i < TAB_COUNT; i++) {
        R b = side_nav_rect(i);
        int hov = ui_hover(b);
        if (i == cur) fill(b, 0, P.accent);
        else if (hov) fill(b, 0, P.alt);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        tx_in(rect(b.x + 14, b.y, b.w - 22, b.h), NAV[i], 13, i == cur ? P.accent_text : P.text, 0, AL_LEFT);
        nav_click(i);
    }
    draw_play(play_rect());
    R g = side_game_rect();
    int has = app_has_game();
    if (has) draw_game_dot(rect(12, g.y + 8, 32, 32), smm_settings.game_name, smm_settings.game_path);
    tx_fit(rect(52, g.y, SIDE_W - 60, g.h), has ? smm_settings.game_name : "No game selected", 11.3, P.text, 0, AL_LEFT);
    if (ui_hover(g)) ui_cursor(GOOME_CURSOR_HAND);
    if (ui_click(g)) {
        if (menu_open) menu_open = 0;
        else build_menu(), menu_open = 1, menu_up = 1, menu_anchor = g, menu_frame = K.frame;
    }
}

R tab_rect(int i)
{
    if (layout() == SMM_LAYOUT_R2MODMAN) return side_nav_rect(i);
    return nav_rect(frame().tabs, i);
}

R play_rect(void)
{
    int lay = layout();
    if (lay == SMM_LAYOUT_R2MODMAN) return side_play_rect();
    if (lay == SMM_LAYOUT_MONKE) return rect(K.w - 130 + 8, MONKE_TITLE_H, 114, MONKE_TAB_H);
    return rect(K.w - 130 + 8, TITLE_H + 3, 114, 32);
}

void chrome_layout_changed(int from, int to)
{
    ul_init = 0;
    menu_open = 0;
    if (!K.windowed || K.maximized) return;
    int add = (to == SMM_LAYOUT_R2MODMAN) - (from == SMM_LAYOUT_R2MODMAN);
    if (add) goome_window_size(K.g, (int)K.w + add * SIDE_W, (int)K.h);
}

static void draw_status(R r, int rounded, int grip)
{
    fill_rd(r, (goome_radii){0, 0, rounded ? WIN_RAD : 0, rounded ? WIN_RAD : 0}, P.alt);
    tx_in(rect(r.x + 8, r.y, r.w - 60, r.h), app_status_text(), 11, P.sub, 0, AL_LEFT);
    if (!grip) return;
    double x = r.x + r.w - 14, y = r.y + r.h - 6;
    goome_draw_line(K.c, x, y - 8, x - 8, y, 1.4, P.border);
    goome_draw_line(K.c, x, y - 4, x - 4, y, 1.4, P.border);
}

static void edge_resize(void)
{
    if (K.maximized || K.blocked) return;
    double g = 6;
    int e = 0;
    if (K.mx < g) e |= GOOME_EDGE_LEFT;
    if (K.mx >= K.w - g) e |= GOOME_EDGE_RIGHT;
    if (K.my < g) e |= GOOME_EDGE_TOP;
    if (K.my >= K.h - g) e |= GOOME_EDGE_BOTTOM;
    if (!e) return;
    int cur = GOOME_CURSOR_ARROW;
    if ((e & 3) && (e & 12)) cur = ((e & 1) && (e & 4)) || ((e & 2) && (e & 8)) ? GOOME_CURSOR_RESIZE_NWSE : GOOME_CURSOR_RESIZE_NESW;
    else if (e & 3) cur = GOOME_CURSOR_RESIZE_EW;
    else cur = GOOME_CURSOR_RESIZE_NS;
    ui_cursor(cur);
    if (K.pressed) goome_window_drag(K.g, e);
}

void chrome_reset(void)
{
    menu_open = 0;
}

void chrome_draw(void)
{
    static int last_layout = -1;
    int lay = layout();
    if (last_layout >= 0 && last_layout != lay) chrome_layout_changed(last_layout, lay);
    last_layout = lay;
    K.square = lay != SMM_LAYOUT_SMM;
    int st = goome_window_get_state(K.g);
    K.maximized = st == GOOME_STATE_MAXIMIZED || st == GOOME_STATE_FULLSCREEN;
    int rounded = !K.maximized && lay == SMM_LAYOUT_SMM;
    goome_erase(K.c, 0, 0, K.w, K.h);
    fill(rect(0, 0, K.w, K.h), rounded ? WIN_RAD : 0, P.bg);

    poll_game();
    tutorial_pre();
    int saved = K.blocked;
    K.blocked = saved || dlg_any() || menu_open || w_dropdown_open();
    int blocked_all = K.blocked;
    Frame f = frame();

    if (menu_open && !w_dropdown_open()) K.blocked = 1;
    if (lay == SMM_LAYOUT_R2MODMAN) {
        draw_sidebar(f);
    } else {
        draw_title(f.title, rounded);
        draw_tabs(f.tabs);
        hline(0, K.w, f.tabs.y + f.tabs.h, P.border);
    }
    switch (app_tab()) {
    case TAB_MODS: panel_mods(f.body); break;
    case TAB_INSTALLED: panel_installed(f.body); break;
    case TAB_LOGS: panel_logs(f.body); break;
    case TAB_LOADOUTS: panel_loadouts(f.body); break;
    case TAB_MODPACKS: panel_modpacks(f.body); break;
    case TAB_CONFIG: panel_config(f.body); break;
    case TAB_SETTINGS: panel_settings(f.body); break;
    }
    draw_status(f.status, rounded, lay == SMM_LAYOUT_SMM);
    K.blocked = saved;
    if (!blocked_all) edge_resize();
    menu_overlay();
    w_dropdown_overlay();
    dlg_draw();
    tutorial_draw();
    w_tooltip_overlay();
}
