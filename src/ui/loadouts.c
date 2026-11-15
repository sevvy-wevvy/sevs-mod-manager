#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static SmmLoadouts loads;
static int sel = -1;
static char name_buf[128];
static char status[160] = "";
static Scroll lscroll, sc_en, sc_dis, sc_miss;
static int saved_name_for = -2;

void loadouts_activate(void)
{
    smm_loadouts_free(&loads);
    memset(&loads, 0, sizeof(loads));
    if (!app_has_game()) {
        snprintf(status, sizeof(status), "No game selected.");
        return;
    }
    smm_loadouts_load(&loads);
    if (sel >= loads.n) sel = loads.n - 1;
    saved_name_for = -2;
    snprintf(status, sizeof(status), "%d loadouts", loads.n);
}

void loadouts_select(int i)
{
    if (i >= 0 && i < loads.n) sel = i;
}

static void display(const SmmLoadout *l, char *out, size_t n)
{
    smm_loadout_display_name(l, out, n);
}

static void save_name(void)
{
    if (sel < 0 || sel >= loads.n) return;
    if (strcmp(loads.v[sel].name, name_buf) == 0) return;
    smm_loadout_rename(&loads, loads.v[sel].number, name_buf);
}

static void do_save(void)
{
    if (!app_has_game()) {
        dlg_message(TAG_LOADOUT_ERR, "Error", "No game path set.", DB_OK, DI_ERROR);
        return;
    }
    if (loads.n >= SMM_MAX_LOADOUTS) {
        dlg_message(TAG_LOADOUT_ERR, "Full", "Loadout cap is 20. Delete one first.", DB_OK, DI_WARN);
        return;
    }
    if (smm_loadout_capture(&loads) >= 0) {
        snprintf(status, sizeof(status), "Loadout %d saved.", loads.n);
        sel = loads.n - 1;
        saved_name_for = -2;
    }
}

static void list_col(R r, const char *title, int n, SmmStrs *s, Scroll *sc)
{
    char t[80];
    snprintf(t, sizeof(t), "%s (%d)", title, n);
    tx_in(rect(r.x, r.y, r.w, 22), t, 12, P.sub, 0, AL_LEFT);
    SmmStrsView v = {s->v, s->n};
    str_list(rect(r.x, r.y + 24, r.w, r.h - 24), &v, sc, P.text);
}

void panel_loadouts(R r)
{
    int res = dlg_result(TAG_LOADOUT_DELETE);
    if (res == DR_YES && sel >= 0 && sel < loads.n) {
        char d[200];
        display(&loads.v[sel], d, sizeof(d));
        smm_loadout_delete(&loads, loads.v[sel].number);
        sel = -1;
        saved_name_for = -2;
        snprintf(status, sizeof(status), "%d loadouts", loads.n);
    }
    dlg_result(TAG_LOADOUT_ERR);

    R bar = cut_top(&r, 44);
    fill(bar, 0, P.surface);
    double x = bar.x + 8, y = bar.y;
    int has = sel >= 0 && sel < loads.n;
    if (tb_button(&x, y, "+ Save Current", 1, 0)) do_save();
    if (tb_button(&x, y, "Refresh", 1, 1)) loadouts_activate();
    tb_style = BTN_ACCENT;
    int do_apply = tb_button(&x, y, "Apply", has, 0);
    tb_style = BTN_GHOST;
    if (do_apply && has) {
        if (smm_loadout_apply(&loads, loads.v[sel].number) >= 0) {
            char d[200];
            display(&loads.v[sel], d, sizeof(d));
            snprintf(status, sizeof(status), "%s applied.", d);
            app_refresh_installed();
        }
    }
    if (tb_button(&x, y, "Delete", has, 0) && has) {
        char d[200], t[260];
        display(&loads.v[sel], d, sizeof(d));
        snprintf(t, sizeof(t), "Delete %s?", d);
        dlg_message(TAG_LOADOUT_DELETE, "Confirm", t, DB_YESNO, DI_QUESTION);
    }
    double rx = bar.x + bar.w - 8;
    if (tb_button_right(&rx, y, "Open Folder", 1, 2)) {
        char d[SMM_PATH], p[SMM_PATH + 16];
        if (smm_sb_dir(d, sizeof(d))) {
            snprintf(p, sizeof(p), "%s/loadouts", d);
            smm_open_path(p);
        }
    }

    R st = cut_bottom(&r, 22);
    tx_in(rect(st.x + 8, st.y, st.w - 8, st.h), status, 12, P.sub, 0, AL_LEFT);
    R left = cut_left(&r, 220);
    fill(left, 0, P.alt);
    vline(left.x + left.w - 1, left.y, left.y + left.h, col_rgb(55, 55, 55));
    R lv = rect(left.x, left.y, left.w - 1, left.h);
    fill(lv, 0, P.bg);
    double rh = 52;
    sc_begin(&lscroll, lv, loads.n * rh);
    for (int i = 0; i < loads.n; i++) {
        R row = rect(lv.x, lv.y + i * rh - lscroll.off, lv.w, rh);
        R card = rect(row.x + 6, row.y + 3, row.w - 12, row.h - 6);
        int hov = ui_hover(card);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        uint32_t hi = col_dark(P.bg) ? col_lighten(P.alt, 22) : col_darken(P.alt, 22);
        fill(card, 8, i == sel ? hi : P.surface);
        char d[200];
        display(&loads.v[i], d, sizeof(d));
        tx_fit(rect(card.x + 10, card.y + 4, card.w - 20, 20), d, 13.3, P.text, 1, AL_LEFT);
        char sub[100];
        snprintf(sub, sizeof(sub), "%d on \xc2\xb7 %d off \xc2\xb7 %d missing", loads.v[i].enabled.n, loads.v[i].disabled.n, loads.v[i].missing.n);
        tx(card.x + 10, card.y + 26, sub, 10.7, P.sub, 0);
        if (ui_click(card)) {
            save_name();
            sel = i;
        }
    }
    sc_end(&lscroll, lv, loads.n * rh);
    if (sel != saved_name_for) {
        saved_name_for = sel;
        snprintf(name_buf, sizeof(name_buf), "%s", sel >= 0 && sel < loads.n ? loads.v[sel].name : "");
    }

    fill(r, 0, P.alt);
    R d = inset(r, 12);
    tx_in(rect(d.x, d.y + 4, 60, 26), "Name", 11.3, P.sub, 0, AL_LEFT);
    R nb = rect(d.x + 64, d.y + 4, 240, 28);
    if (w_input("lname", nb, name_buf, sizeof(name_buf), "Custom name (optional)", GOOME_INPUT_ENTER_RETURNS, goome_radius(6))) save_name();
    if (!K.blocked && !goome_ui_focused(K.u) && 0) save_name();
    d.y += 38;
    d.h -= 38;
    double cw = d.w / 3;
    if (sel >= 0 && sel < loads.n) {
        SmmLoadout *l = &loads.v[sel];
        list_col(rect(d.x, d.y, cw - 6, d.h), "Enabled", l->enabled.n, &l->enabled, &sc_en);
        list_col(rect(d.x + cw, d.y, cw - 6, d.h), "Disabled", l->disabled.n, &l->disabled, &sc_dis);
        list_col(rect(d.x + cw * 2, d.y, cw - 6, d.h), "Missing", l->missing.n, &l->missing, &sc_miss);
    } else {
        const char *t[3] = {"Enabled", "Disabled", "Missing"};
        SmmStrsView e = {NULL, 0};
        Scroll *s[3] = {&sc_en, &sc_dis, &sc_miss};
        for (int i = 0; i < 3; i++) {
            tx_in(rect(d.x + cw * i, d.y, cw, 22), t[i], 12, P.sub, 0, AL_LEFT);
            str_list(rect(d.x + cw * i, d.y + 24, cw - 6, d.h - 24), &e, s[i], P.text);
        }
    }
}
