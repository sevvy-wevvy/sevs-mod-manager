#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static SmmConfigRefs refs;
static int sel = -1;
static SmmConfigFile cur;
static int have_cur;
static char status[200] = "";
static Scroll lscroll, rscroll;
static int dd_sel[256];
static char dd_ids[256][8];

static int is_melon(void)
{
    return smm_detect_loader() == SMM_LOADER_MELON;
}

static void unload(void)
{
    if (have_cur) smm_config_file_free(&cur);
    memset(&cur, 0, sizeof(cur));
    have_cur = 0;
}

static void show_selected(void)
{
    unload();
    rscroll.off = 0;
    if (sel < 0 || sel >= refs.n) return;
    if (smm_config_parse(refs.v[sel].path, &cur) < 0) {
        snprintf(status, sizeof(status), "Failed to parse config: %s", smm_error());
        return;
    }
    have_cur = 1;
}

void config_activate(void)
{
    char keep[SMM_PATH] = "";
    if (sel >= 0 && sel < refs.n) snprintf(keep, sizeof(keep), "%s", refs.v[sel].path);
    smm_config_refs_free(&refs);
    memset(&refs, 0, sizeof(refs));
    if (!app_has_game()) {
        snprintf(status, sizeof(status), "No game selected.");
        unload();
        return;
    }
    smm_config_list(&refs);
    snprintf(status, sizeof(status), "%d config files", refs.n);
    int idx = -1;
    for (int i = 0; i < refs.n; i++)
        if (keep[0] && strcmp(refs.v[i].path, keep) == 0) idx = i;
    sel = idx >= 0 ? idx : (refs.n > 0 ? 0 : -1);
    show_selected();
}

static int changed_from_default(const SmmConfigEntry *e)
{
    return e->has_default && strcasecmp(e->value, e->default_value) != 0;
}

static void do_save(void)
{
    if (!have_cur) return;
    if (smm_config_save(&cur) >= 0) snprintf(status, sizeof(status), "Saved %s.", cur.display_name);
    else snprintf(status, sizeof(status), "Save failed: %s", smm_error());
}

static void do_reset_all(void)
{
    int changed = 0;
    for (int i = 0; i < refs.n; i++) {
        SmmConfigFile f;
        memset(&f, 0, sizeof(f));
        if (smm_config_parse(refs.v[i].path, &f) < 0) continue;
        int dirty = 0;
        for (int s = 0; s < f.n_sections; s++)
            for (int k = 0; k < f.sections[s].n; k++) {
                SmmConfigEntry *e = &f.sections[s].v[k];
                if (changed_from_default(e)) {
                    snprintf(e->value, sizeof(e->value), "%s", e->default_value);
                    dirty = 1;
                    changed++;
                }
            }
        if (dirty) smm_config_save(&f);
        smm_config_file_free(&f);
    }
    config_activate();
    snprintf(status, sizeof(status), "Reset %d setting(s) to default across %d config files.", changed, refs.n);
}

static int is_int_type(const char *t)
{
    return strstr(t, "Int") || strcasecmp(t, "Byte") == 0 || strcasecmp(t, "SByte") == 0;
}

static void draw_slider(R r, SmmConfigEntry *e)
{
    int isint = is_int_type(e->setting_type);
    double mn = e->range_min, mx = e->range_max, v = atof(e->value);
    double t = mx > mn ? (v - mn) / (mx - mn) : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    R track = rect(r.x, r.y + r.h / 2 - 3, r.w - 70, 6);
    fill(track, 3, P.surface);
    fill(rect(track.x, track.y, track.w * t, track.h), 3, P.accent);
    goome_draw_circle(K.c, track.x + track.w * t, track.y + 3, 8, 0xFFFFFFFF);
    R hit = rect(track.x - 8, r.y, track.w + 16, r.h);
    if (ui_hover(hit)) ui_cursor(GOOME_CURSOR_HAND);
    if (K.down && !K.blocked && inside(hit, K.px, K.py) && ui_hover(rect(hit.x - 40, hit.y - 400, hit.w + 80, hit.h + 800))) {
        double nt = (K.mx - track.x) / track.w;
        if (nt < 0) nt = 0;
        if (nt > 1) nt = 1;
        double nv = mn + nt * (mx - mn);
        if (isint) snprintf(e->value, sizeof(e->value), "%.0f", nv);
        else snprintf(e->value, sizeof(e->value), "%.3g", nv);
    }
    tx_in(rect(r.x + r.w - 60, r.y, 60, r.h), e->value, 13, P.text, 0, AL_RIGHT);
}

static void draw_entry(R row, SmmConfigEntry *e, int uid)
{
    char label[200];
    snprintf(label, sizeof(label), "%s%s", e->key, changed_from_default(e) ? "  (changed)" : "");
    R lr = rect(row.x, row.y + 4, row.w, 18);
    tx_in(lr, label, 12.7, P.text, 1, AL_LEFT);
    char tip[1200] = "";
    for (int i = 0; i < e->description.n; i++) {
        if (i) strncat(tip, " ", sizeof(tip) - strlen(tip) - 1);
        strncat(tip, e->description.v[i], sizeof(tip) - strlen(tip) - 1);
    }
    if (e->has_default) {
        if (tip[0]) strncat(tip, "\n\n", sizeof(tip) - strlen(tip) - 1);
        char d[300];
        snprintf(d, sizeof(d), "Default: %s", e->default_value);
        strncat(tip, d, sizeof(tip) - strlen(tip) - 1);
    }
    w_tooltip(rect(row.x, row.y + 4, tx_w(label, 12.7, 1), 18), tip);
    R wr = rect(row.x, row.y + 22, row.w, 30);
    if (strcasecmp(e->setting_type, "Boolean") == 0) {
        int on = strcasecmp(e->value, "true") == 0;
        if (w_check(wr.x, wr.y + 2, on ? "true" : "false", &on)) snprintf(e->value, sizeof(e->value), "%s", on ? "true" : "false");
    } else if (e->has_range) {
        draw_slider(wr, e);
    } else if (e->acceptable.n > 0) {
        int idx = smm_strs_find(&e->acceptable, e->value, 1);
        if (idx < 0) idx = 0;
        int slot = uid & 255;
        dd_sel[slot] = idx;
        snprintf(dd_ids[slot], sizeof(dd_ids[slot]), "c%d", slot);
        char id[24];
        snprintf(id, sizeof(id), "cfgdd%d", slot);
        uint32_t keep = P.alt;
        P.alt = P.surface;
        int ch = w_dropdown(id, wr, (const char *const *)e->acceptable.v, e->acceptable.n, &dd_sel[slot], goome_radius(6));
        P.alt = keep;
        if (ch) snprintf(e->value, sizeof(e->value), "%s", e->acceptable.v[dd_sel[slot]]);
    } else {
        char id[24];
        snprintf(id, sizeof(id), "cfg%d", uid);
        input_bg = P.surface;
        w_input(id, wr, e->value, sizeof(e->value), NULL, 0, goome_radius(6));
        input_bg = 0;
    }
}

void panel_config(R r)
{
    if (dlg_result(TAG_CFG_RESET) == DR_YES) do_reset_all();
    dlg_result(TAG_CFG_RESET_INFO);
    R bar = cut_top(&r, 44);
    fill(bar, 0, P.surface);
    double x = bar.x + 8, y = bar.y;
    int can_save = have_cur && cur.n_sections > 0;
    tb_style = BTN_ACCENT;
    int sv = tb_button(&x, y, "Save", can_save, 0);
    tb_style = BTN_GHOST;
    if (sv && can_save) do_save();
    if (tb_button(&x, y, "Reload", 1, 1)) config_activate();
    if (tb_button(&x, y, "Reset All to Default", refs.n > 0 && !is_melon(), 0)) {
        char t[300];
        snprintf(t, sizeof(t), "This resets every setting across all %d config files back to its default value. This cannot be undone.\n\nContinue?", refs.n);
        dlg_message(TAG_CFG_RESET, "Reset All to Default", t, DB_YESNO, DI_WARN);
    }

    R st = cut_bottom(&r, 22);
    tx_in(rect(st.x + 8, st.y, st.w - 8, st.h), status, 12, P.sub, 0, AL_LEFT);
    R left = cut_left(&r, 260);
    fill(left, 0, P.alt);
    vline(left.x + left.w - 1, left.y, left.y + left.h, col_rgb(55, 55, 55));
    R lv = inset(rect(left.x, left.y, left.w - 1, left.h), 2);
    fill(lv, 0, P.bg);
    double rh = 24;
    sc_begin(&lscroll, lv, refs.n * rh);
    for (int i = 0; i < refs.n; i++) {
        R row = rect(lv.x, lv.y + i * rh - lscroll.off, lv.w, rh);
        int hov = ui_hover(row);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        uint32_t hi = col_dark(P.bg) ? col_lighten(P.alt, 22) : col_darken(P.alt, 22);
        if (i == sel) fill(rect(row.x + 2, row.y + 1, row.w - 4, row.h - 2), 4, hi);
        else if (hov) fill(rect(row.x + 2, row.y + 1, row.w - 4, row.h - 2), 4, col_alpha(hi, 80));
        tx_fit(rect(row.x + 8, row.y, row.w - 16, row.h), refs.v[i].display_name, 12.7, P.text, 0, AL_LEFT);
        if (ui_click(row) && sel != i) {
            sel = i;
            show_selected();
        }
    }
    sc_end(&lscroll, lv, refs.n * rh);

    fill(r, 0, P.alt);
    R title = cut_top(&r, 30);
    if (have_cur) tx_in(rect(title.x + 8, title.y + 4, title.w - 16, 26), cur.display_name, 16, P.text, 1, AL_LEFT);
    R host = rect(r.x + 12, r.y + 4, r.w - 24, r.h - 16);
    if (refs.n == 0) {
        tx_wrap(rect(host.x + 4, host.y + 8, host.w - 8, 60), "No configs found, maybe try installing some mods and starting your game!", 13.3, P.sub, 0, NULL);
        return;
    }
    if (!have_cur) return;
    double total = 0;
    for (int s = 0; s < cur.n_sections; s++) total += 30 + cur.sections[s].n * 54 + 10;
    R view = rect(host.x, host.y, host.w + 12, host.h);
    sc_begin(&rscroll, view, total);
    double y0 = host.y - rscroll.off;
    int uid = 0;
    for (int s = 0; s < cur.n_sections; s++) {
        SmmConfigSection *sec = &cur.sections[s];
        tx_in(rect(host.x, y0 + 6, host.w, 24), sec->name, 13.3, P.text, 1, AL_LEFT);
        y0 += 30;
        for (int k = 0; k < sec->n; k++) {
            R row = rect(host.x, y0, host.w - 8, 54);
            if (y0 + 54 > view.y && y0 < view.y + view.h) draw_entry(row, &sec->v[k], uid);
            uid++;
            y0 += 54;
        }
        y0 += 10;
    }
    sc_end(&rscroll, view, total);
}
