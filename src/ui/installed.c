#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static char search[160];
static Scroll scroll;
static SmmStrs conflicts;
static int tut_on, tut_enabled, tut_deleted;
static char pending[200];
static int pending_all;
static R toggle_r, delete_r, list_r;

R installed_list_rect(void)
{
    return list_r;
}
static int sel = -1;

#define COL_STATUS 90
#define COL_VERSION 120
#define COL_SIZE 70
#define COL_ICON 32

void installed_tutorial(int on)
{
    tut_on = on;
    tut_enabled = 1;
    tut_deleted = 0;
}

int installed_tutorial_enabled(void)
{
    return tut_enabled;
}

int installed_tutorial_deleted(void)
{
    return tut_deleted;
}

R installed_toggle_rect(void)
{
    return toggle_r;
}

R installed_delete_rect(void)
{
    return delete_r;
}

static void fmt_size(long long b, char *out, size_t n)
{
    if (b >= 1024 * 1024) snprintf(out, n, "%.1f MB", b / (1024.0 * 1024.0));
    else if (b >= 1024) snprintf(out, n, "%.1f KB", b / 1024.0);
    else snprintf(out, n, "%lld B", b);
    char *dot = strstr(out, ".0 ");
    if (dot) memmove(dot, dot + 2, strlen(dot + 2) + 1);
}

void installed_activate(void)
{
    app_refresh_installed();
    SmmInstalledList *l = app_installed();
    const SmmMods *latest = mods_latest();
    if (latest) smm_apply_latest_versions(l, latest);
    smm_strs_free(&conflicts);
    memset(&conflicts, 0, sizeof(conflicts));
    smm_find_conflicts(l, &conflicts);
    scroll.off = 0;
}

static int matches(const SmmInstalled *m)
{
    if (!search[0]) return 1;
    char a[200], b[160];
    snprintf(a, sizeof(a), "%s", m->name);
    snprintf(b, sizeof(b), "%s", search);
    for (char *p = a; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    for (char *p = b; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    return strstr(a, b) != NULL;
}

static void ask_uninstall(const char *name)
{
    snprintf(pending, sizeof(pending), "%s", name);
    char t[600];
    SmmStrs deps;
    memset(&deps, 0, sizeof(deps));
    SmmInstalledList *l = app_installed();
    const SmmMods *latest = mods_latest();
    if (latest) {
        for (int i = 0; i < latest->n; i++) {
            const SmmMod *m = &latest->v[i];
            char key[200];
            smm_mod_key(m, key, sizeof(key));
            if (strcasecmp(key, name) != 0) continue;
            smm_dependents_installed(m, latest, l, &deps);
        }
    }
    char warn[400] = "";
    if (deps.n > 0) {
        char list[300] = "";
        for (int i = 0; i < deps.n; i++) {
            if (i) strncat(list, ", ", sizeof(list) - strlen(list) - 1);
            strncat(list, deps.v[i], sizeof(list) - strlen(list) - 1);
        }
        snprintf(warn, sizeof(warn), "\n\n%s depend%s on this and may break without it.", list, deps.n == 1 ? "s" : "");
    }
    smm_strs_free(&deps);
    snprintf(t, sizeof(t), "Uninstall %s?%s", name, warn);
    dlg_message(TAG_UNINSTALL, "Confirm", t, DB_YESNO, DI_WARN);
}

static void count_stats(const SmmInstalledList *l, char *out, size_t n)
{
    int en = 0, upd = 0;
    long long total = 0;
    for (int i = 0; i < l->n; i++) {
        if (l->v[i].enabled) en++;
        total += l->v[i].size_bytes;
        if (smm_installed_update_available(&l->v[i])) upd++;
    }
    char sz[32];
    fmt_size(total, sz, sizeof(sz));
    int cnt = l->n + (tut_on ? 1 : 0);
    snprintf(out, n, "%d mods  \xc2\xb7  %d enabled, %d disabled  \xc2\xb7  %s total", cnt, en + (tut_on && tut_enabled), l->n - en + (tut_on && !tut_enabled), sz);
    if (upd > 0) {
        size_t k = strlen(out);
        snprintf(out + k, n - k, "  \xc2\xb7  %d update%s available", upd, upd == 1 ? "" : "s");
    }
}

typedef struct {
    char name[160];
    int enabled;
    long long size;
    char status[16];
    char version[120];
    int upd;
    int tut;
    int idx;
} Row;

void panel_installed(R r)
{
    SmmInstalledList *l = app_installed();
    int res = dlg_result(TAG_UNINSTALL);
    if (res == DR_YES && pending[0]) {
        smm_mod_uninstall(pending);
        smm_mod_version_delete(pending);
        app_refresh_installed();
        installed_activate();
    }
    res = dlg_result(TAG_UNINSTALL_ALL);
    if (res == DR_YES) {
        for (int i = l->n - 1; i >= 0; i--) smm_mod_uninstall(l->v[i].name);
        app_refresh_installed();
        installed_activate();
        app_status("All mods uninstalled.");
    }
    dlg_result(TAG_CONFLICTS);
    l = app_installed();

    int upd = 0;
    for (int i = 0; i < l->n; i++)
        if (smm_installed_update_available(&l->v[i])) upd++;

    R bar = cut_top(&r, 44);
    fill(bar, 0, P.surface);
    double y = bar.y;
    double x = bar.x + 8;
    if (tb_button(&x, y, "Refresh", 1, 1)) installed_activate();
    if (upd > 0) {
        tb_style = BTN_ACCENT;
        if (tb_button(&x, y, "Update All", !smm_queue_busy(), 0)) {
            const SmmMods *latest = mods_latest();
            if (latest) smm_queue_update_mods(latest, l);
        }
        tb_style = BTN_GHOST;
    }
    if (conflicts.n > 0) {
        char t[40];
        snprintf(t, sizeof(t), "%d Conflict%s", conflicts.n, conflicts.n == 1 ? "" : "s");
        double wx = x;
        if (tb_button(&x, y, t, 1, 0)) {
            char msg[1200] = "";
            for (int i = 0; i < conflicts.n; i++) {
                strncat(msg, conflicts.v[i], sizeof(msg) - strlen(msg) - 1);
                strncat(msg, "\n", sizeof(msg) - strlen(msg) - 1);
            }
            dlg_message(TAG_CONFLICTS, "Possible Mod Conflicts", msg, DB_OK, DI_WARN);
        }
        (void)wx;
    }
    goome_radii bot = {0, 0, 8, 8};
    double rx = bar.x + bar.w - 8;
    R se = rect(rx - 180, bar.y, 180, 38);
    w_input("isearch", se, search, sizeof(search), "Search installed...", 0, bot);
    rx -= 186;
    int act = -1;
    if (tb_button_right(&rx, y, "Uninstall All", 1, 0)) act = 3;
    if (tb_button_right(&rx, y, "Disable All Mods", 1, 0)) act = 2;
    if (tb_button_right(&rx, y, "Enable All Mods", 1, 0)) act = 1;
    if (tb_button_right(&rx, y, smm_detect_loader() == SMM_LOADER_MELON ? "Open Mods" : "Open Plugins", 1, 2)) act = 0;
    if (act == 0) {
        char d[SMM_PATH];
        if (smm_mods_install_dir(d, sizeof(d)) >= 0) smm_open_path(d);
    } else if (act == 1) {
        for (int i = 0; i < l->n; i++) smm_mod_enable(l->v[i].name);
        installed_activate();
        app_status("All mods enabled.");
    } else if (act == 2) {
        for (int i = 0; i < l->n; i++) smm_mod_disable(l->v[i].name);
        installed_activate();
        app_status("All mods disabled.");
    } else if (act == 3 && l->n > 0) {
        char t[80];
        snprintf(t, sizeof(t), "Uninstall all %d mods?", l->n);
        dlg_message(TAG_UNINSTALL_ALL, "Confirm", t, DB_YESNO, DI_WARN);
    }

    R status = cut_bottom(&r, 22);
    char st[300];
    count_stats(l, st, sizeof(st));
    tx_in(rect(status.x + 8, status.y, status.w - 8, status.h), st, 12, P.sub, 0, AL_LEFT);
    fill(rect(r.x, r.y, r.w, r.h), 0, P.alt);
    R card = inset(r, 2);
    list_r = card;
    fill(card, 0, P.surface);
    R head = cut_top(&card, 26);
    fill(head, 0, P.alt);
    hline(head.x, head.x + head.w, head.y + head.h - 1, P.border);
    double used = COL_STATUS + COL_VERSION + COL_SIZE + COL_ICON * 2;
    double cn = card.w - used;
    double cx[7];
    cx[0] = card.x;
    cx[1] = cx[0] + cn;
    cx[2] = cx[1] + COL_STATUS;
    cx[3] = cx[2] + COL_VERSION;
    cx[4] = cx[3] + COL_SIZE;
    cx[5] = cx[4] + COL_ICON;
    const char *hn[4] = {"Name", "Status", "Version", "Size"};
    for (int i = 0; i < 4; i++) tx_in(rect(cx[i] + 6, head.y, 150, head.h), hn[i], 12, P.sub, 0, AL_LEFT);

    Row rows[512];
    int n = 0;
    if (tut_on) {
        snprintf(rows[n].name, sizeof(rows[n].name), "Example Mod");
        rows[n].enabled = tut_enabled;
        rows[n].size = 0;
        rows[n].tut = 1;
        rows[n].idx = -1;
        n++;
    }
    for (int i = 0; i < l->n && n < 512; i++) {
        if (!matches(&l->v[i])) continue;
        Row *w = &rows[n++];
        snprintf(w->name, sizeof(w->name), "%s", l->v[i].name);
        w->enabled = l->v[i].enabled;
        w->size = l->v[i].size_bytes;
        w->upd = smm_installed_update_available(&l->v[i]);
        w->tut = 0;
        w->idx = i;
    }
    double rh = 30;
    sc_begin(&scroll, card, n * rh);
    for (int i = 0; i < n; i++) {
        R row = rect(card.x, card.y + i * rh - scroll.off, card.w, rh);
        if (row.y + rh < card.y || row.y > card.y + card.h) continue;
        Row *w = &rows[i];
        int hov = ui_hover(row);
        fill(row, 0, i % 2 == 0 ? P.surface : P.bg);
        uint32_t hi = col_dark(P.bg) ? col_lighten(P.alt, 22) : col_darken(P.alt, 22);
        if (i == sel) fill(rect(row.x + 2, row.y + 1, row.w - 4, row.h - 2), 6, hi);
        else if (hov) fill(rect(row.x + 2, row.y + 1, row.w - 4, row.h - 2), 6, col_alpha(hi, 80));
        R tg = rect(cx[4], row.y, COL_ICON, rh);
        R dl = rect(cx[5], row.y, COL_ICON, rh);
        if (ui_click(row) && !inside(tg, K.mx, K.my) && !inside(dl, K.mx, K.my)) sel = i;
        tx_fit(rect(cx[0] + 6, row.y, cn - 10, rh), w->name, 12, P.text, 0, AL_LEFT);
        tx_in(rect(cx[1] + 6, row.y, COL_STATUS, rh), w->enabled ? "Enabled" : "Disabled", 12, w->enabled ? col_rgb(39, 201, 63) : col_rgb(255, 95, 86), 0, AL_LEFT);
        char ver[120] = "-";
        if (w->upd) {
            const char *a = l->v[w->idx].known_version, *b = l->v[w->idx].latest_version;
            double ax = cx[2] + 6, aw = tx_w(a, 12, 0);
            uint32_t uc = col_rgb(255, 189, 46);
            tx(ax, row.y + (rh - tx_h(12)) / 2, a, 12, uc, 0);
            goome_draw_line(K.c, ax + aw + 5, row.y + rh / 2, ax + aw + 15, row.y + rh / 2, 1.3, uc);
            goome_draw_triangle(K.c, ax + aw + 16, row.y + rh / 2, ax + aw + 11, row.y + rh / 2 - 3.5, ax + aw + 11, row.y + rh / 2 + 3.5, uc);
            tx(ax + aw + 21, row.y + (rh - tx_h(12)) / 2, b, 12, uc, 0);
        } else {
            if (!w->tut && l->v[w->idx].known_version[0]) snprintf(ver, sizeof(ver), "%s", l->v[w->idx].known_version);
            tx_fit(rect(cx[2] + 6, row.y, COL_VERSION - 8, rh), ver, 12, P.text, 0, AL_LEFT);
        }
        char sz[32];
        fmt_size(w->size, sz, sizeof(sz));
        tx_in(rect(cx[3] + 6, row.y, COL_SIZE, rh), sz, 12, P.text, 0, AL_LEFT);
        R box = rect(tg.x + (tg.w - 16) / 2, row.y + (rh - 16) / 2, 16, 16);
        int thov = ui_hover(tg);
        if (thov) ui_cursor(GOOME_CURSOR_HAND);
        if (w->enabled) fill(box, 4, P.accent), ic_check(inset(box, 3.5), 0xFFFFFFFF);
        else stroke(inset(box, 0.7), 4, 1.4, P.sub);
        if (w->tut) toggle_r = tg;
        if (ui_click(tg)) {
            if (w->tut) tut_enabled = !tut_enabled;
            else {
                if (w->enabled) smm_mod_disable(w->name);
                else smm_mod_enable(w->name);
                app_refresh_installed();
                installed_activate();
            }
        }
        if (ui_hover(dl)) ui_cursor(GOOME_CURSOR_HAND);
        ic_trash(rect(dl.x + (dl.w - 12) / 2, row.y + (rh - 12) / 2, 12, 12), col_rgb(255, 95, 86));
        if (w->tut) delete_r = dl;
        if (ui_click(dl)) {
            if (w->tut) tut_deleted = 1, tut_on = 0;
            else ask_uninstall(w->name);
        }
        hline(row.x, row.x + row.w, row.y + row.h - 1, P.border);
    }
    sc_end(&scroll, card, n * rh);
    (void)pending_all;
}
