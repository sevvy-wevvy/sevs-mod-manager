#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app.h"
#include "../core/internal.h"

typedef struct {
    int kind;
    char path[SMM_PATH];
    int stage;
} Flow;

enum { FL_NONE, FL_APPLY, FL_BLANK, FL_IMPORT };

static SmmPacks packs;
static int sel = -1;
static char status[300] = "";
static Scroll lscroll, mscroll, fscroll;
static SmmStrs entries;
static char **tree_lines;
static int n_tree;
static Flow flow;
static char pending_delete[SMM_PATH];
static int last_click_idx = -1;
static long last_click_ms;

static void clear_tree(void)
{
    for (int i = 0; i < n_tree; i++) free(tree_lines[i]);
    free(tree_lines);
    tree_lines = NULL;
    n_tree = 0;
}

static void build_tree(void)
{
    clear_tree();
    tree_lines = calloc((size_t)(entries.n * 4 + 1), sizeof(char *));
    char seen[8192][160];
    int nseen = 0;
    for (int i = 0; i < entries.n; i++) {
        const char *p = entries.v[i];
        char acc[512] = "";
        int depth = 0;
        while (*p) {
            const char *e = strchr(p, '/');
            size_t len = e ? (size_t)(e - p) : strlen(p);
            char part[160];
            snprintf(part, sizeof(part), "%.*s", (int)len, p);
            size_t al = strlen(acc);
            snprintf(acc + al, sizeof(acc) - al, "%s%s", al ? "/" : "", part);
            int dup = 0;
            for (int k = 0; k < nseen && !dup; k++)
                if (strcmp(seen[k], acc) == 0) dup = 1;
            if (!dup) {
                if (nseen < 8192) snprintf(seen[nseen++], 160, "%s", acc);
                char line[200];
                snprintf(line, sizeof(line), "%s%*s%s", e ? "\x01" : "", depth * 2, "", part);
                tree_lines[n_tree++] = strdup(line);
            }
            if (!e) break;
            p = e + 1;
            depth++;
        }
        if (n_tree > entries.n * 3) break;
    }
}

static void show_detail(void)
{
    smm_strs_free(&entries);
    memset(&entries, 0, sizeof(entries));
    clear_tree();
    mscroll.off = fscroll.off = 0;
    if (sel < 0 || sel >= packs.n) return;
    smm_pack_entries(packs.v[sel].path, &entries);
    build_tree();
}

void modpacks_activate(void)
{
    char keep[SMM_PATH] = "";
    if (sel >= 0 && sel < packs.n) snprintf(keep, sizeof(keep), "%s", packs.v[sel].path);
    smm_packs_free(&packs);
    memset(&packs, 0, sizeof(packs));
    if (!app_has_game()) {
        snprintf(status, sizeof(status), "No game selected.");
        return;
    }
    smm_packs_list(&packs);
    snprintf(status, sizeof(status), "%d modpacks", packs.n);
    int idx = -1;
    for (int i = 0; i < packs.n; i++)
        if (keep[0] && strcmp(packs.v[i].path, keep) == 0) idx = i;
    sel = idx;
    show_detail();
}

static void fmt_date(const char *iso, char *out, size_t n, int year)
{
    int y = 0, m = 0, d = 0;
    static const char *const mn[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (sscanf(iso, "%d-%d-%d", &y, &m, &d) == 3 && m >= 1 && m <= 12) {
        if (year) snprintf(out, n, "%s %d, %d", mn[m - 1], d, y);
        else snprintf(out, n, "%s %d", mn[m - 1], d);
    } else {
        snprintf(out, n, "%s", iso);
    }
}

void modpacks_select(int i)
{
    if (i >= 0 && i < packs.n) {
        sel = i;
        show_detail();
    }
}

static void display_name(const SmmPack *p, char *out, size_t n)
{
    smm_pack_display_name(p, out, n);
}

static void queue_save_named(const char *name)
{
    smm_queue_save_pack(name, "user", NULL);
    snprintf(status, sizeof(status), "Saving \"%s\"...", name);
}

static void start_apply(const char *path)
{
    flow.kind = FL_APPLY;
    snprintf(flow.path, sizeof(flow.path), "%s", path);
    flow.stage = 0;
}

static void flow_step(void)
{
    if (flow.kind == FL_NONE) return;
    char name[200];
    int r;
    switch (flow.stage) {
    case 0: {
        if (!app_has_game()) {
            dlg_message(TAG_LOADOUT_ERR, "Error", "No game path set.", DB_OK, DI_ERROR);
            flow.kind = FL_NONE;
            return;
        }
        if (flow.kind == FL_BLANK) {
            dlg_message(TAG_PACK_BLANK, "Create Blank Pack", "This deletes your current BepInEx folder (all installed mods) and leaves a blank one. Continue?", DB_YESNO, DI_WARN);
            flow.stage = 1;
        } else if (flow.kind == FL_APPLY) {
            char t[400];
            snprintf(t, sizeof(t), "Apply \"%s\"? This replaces your current BepInEx folder.", sel >= 0 && sel < packs.n ? packs.v[sel].manifest.name : "pack");
            if (sel >= 0 && sel < packs.n) display_name(&packs.v[sel], name, sizeof(name)), snprintf(t, sizeof(t), "Apply \"%s\"? This replaces your current BepInEx folder.", name);
            dlg_message(TAG_PACK_APPLY, "Confirm", t, DB_YESNO, DI_QUESTION);
            flow.stage = 1;
        } else {
            SmmManifest m;
            if (smm_pack_read_manifest(flow.path, &m) < 0) {
                dlg_message(TAG_PACK_INVALID, "Invalid Pack", "This doesn't look like a valid .smmpack file.", DB_OK, DI_ERROR);
                flow.kind = FL_NONE;
                return;
            }
            char t[500];
            snprintf(t, sizeof(t), "Install modpack \"%s\" by %s (%d mods)? This replaces your current BepInEx setup.", m.name, m.author, m.mods.n);
            smm_manifest_free(&m);
            dlg_message(TAG_PACK_IMPORT, "Install Modpack", t, DB_YESNO, DI_QUESTION);
            flow.stage = 1;
        }
        break;
    }
    case 1:
        r = dlg_result(flow.kind == FL_BLANK ? TAG_PACK_BLANK : flow.kind == FL_APPLY ? TAG_PACK_APPLY : TAG_PACK_IMPORT);
        if (!r) return;
        if (r != DR_YES) {
            flow.kind = FL_NONE;
            return;
        }
        if (flow.kind != FL_BLANK && smm_setup_matches_saved_pack()) flow.stage = 4;
        else if (flow.kind == FL_BLANK && smm_setup_matches_saved_pack()) flow.stage = 4;
        else {
            dlg_message(TAG_PACK_SAVE_FIRST, "Save Current Setup First?",
                        flow.kind == FL_BLANK ? "Creating a blank pack replaces everything in your current BepInEx folder. Save your current setup as a modpack first so you don't lose it?"
                                              : "Applying a modpack replaces everything in your current BepInEx folder. Save your current setup as a modpack first so you don't lose it?",
                        DB_YESNOCANCEL, DI_WARN);
            flow.stage = 2;
        }
        break;
    case 2:
        r = dlg_result(TAG_PACK_SAVE_FIRST);
        if (!r) return;
        if (r == DR_CANCEL) flow.kind = FL_NONE;
        else if (r == DR_NO) flow.stage = 4;
        else {
            char def[64];
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            snprintf(def, sizeof(def), "My Setup %04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
            dlg_prompt(TAG_PACK_SAVE_NAME, "Save Current Setup", "Modpack name:", def);
            flow.stage = 3;
        }
        break;
    case 3:
        r = dlg_prompt_result(TAG_PACK_SAVE_NAME, name, sizeof(name));
        if (!r) return;
        if (r < 0 || !name[0]) {
            flow.kind = FL_NONE;
            return;
        }
        queue_save_named(name);
        flow.stage = 4;
        break;
    case 4:
        if (flow.kind == FL_BLANK) smm_queue_blank_setup();
        else smm_queue_apply_pack(flow.path);
        snprintf(status, sizeof(status), flow.kind == FL_BLANK ? "Creating blank pack..." : "Applying modpack...");
        flow.kind = FL_NONE;
        break;
    }
}

void modpacks_import_path(const char *path)
{
    flow.kind = FL_IMPORT;
    snprintf(flow.path, sizeof(flow.path), "%s", path);
    flow.stage = 0;
    if (app_tab() != TAB_MODPACKS) app_goto_tab(TAB_MODPACKS);
}

static void switch_to_pack_game(const SmmPack *p)
{
    const char *slug = p->manifest.game_slug;
    char cur[64];
    smm_current_slug(cur, sizeof(cur));
    if (!slug[0] || strcasecmp(slug, cur) == 0) return;
    for (int i = 0; i < smm_preset_count(); i++) {
        const SmmPreset *pr = smm_preset_at(i);
        if (strcmp(pr->name, "Custom") == 0 || strcasecmp(pr->slug, slug) != 0) continue;
        const char *path = NULL;
        for (int k = 0; k < pr->n_paths; k++) {
            if (smm_os_is_file(pr->default_paths[k])) {
                path = pr->default_paths[k];
                break;
            }
        }
        if (!path) path = smm_map_get(&smm_settings.known_game_paths, pr->name);
        if (path) {
            snprintf(smm_settings.game_path, sizeof(smm_settings.game_path), "%s", path);
            snprintf(smm_settings.game_name, sizeof(smm_settings.game_name), "%s", pr->name);
            smm_state_save();
            smm_ensure_sb_folder();
            smm_dc_load_settings();
            app_game_changed();
            snprintf(status, sizeof(status), "Switched to %s.", pr->name);
            return;
        }
    }
    char t[300];
    snprintf(t, sizeof(t), "Couldn't automatically find the game this modpack is for (\"%s\"). Pick its executable on the next screen.", slug);
    dlg_message(TAG_PACK_LOCATE, "Locate Game", t, DB_OK, DI_INFO);
}

static void do_export(void)
{
    if (sel < 0 || sel >= packs.n) return;
    char dest[SMM_PATH], def[200];
    snprintf(def, sizeof(def), "%s.smmpack", packs.v[sel].manifest.name);
    if (file_pick_save("Export Modpack", def, "smmpack", dest, sizeof(dest)) <= 0) return;
    if (smm_pack_export(packs.v[sel].path, dest) >= 0) snprintf(status, sizeof(status), "Exported to %s", dest);
    else snprintf(status, sizeof(status), "Export failed: %s", smm_error());
}

static void do_import(void)
{
    char src[SMM_PATH];
    if (file_pick_open("Import Modpack", "Sev's Mod Manager Pack", "smmpack", src, sizeof(src)) <= 0) return;
    char dest[SMM_PATH];
    if (smm_pack_import(src, dest, sizeof(dest)) >= 0) {
        snprintf(status, sizeof(status), "Modpack imported.");
        modpacks_activate();
    } else {
        snprintf(status, sizeof(status), "Import failed: %s", smm_error());
    }
}

void panel_modpacks(R r)
{
    flow_step();
    int res = dlg_result(TAG_PACK_DELETE);
    if (res == DR_YES && pending_delete[0]) {
        smm_pack_delete(pending_delete);
        pending_delete[0] = 0;
        sel = -1;
        modpacks_activate();
    }
    dlg_result(TAG_PACK_LOCATE);
    dlg_result(TAG_PACK_INVALID);
    dlg_result(TAG_LOADOUT_ERR);
    if (dlg_result(TAG_PACK_SAVE_NAME) == DR_OK) {}

    R bar = cut_top(&r, 44);
    fill(bar, 0, P.surface);
    double x = bar.x + 8, y = bar.y;
    int has = sel >= 0 && sel < packs.n;
    if (tb_button(&x, y, "+ Save Current", 1, 0)) dlg_export_open();
    if (tb_button(&x, y, "Create Blank Pack", 1, 0)) {
        flow.kind = FL_BLANK;
        flow.stage = 0;
    }
    if (tb_button(&x, y, "Refresh", 1, 1)) modpacks_activate();
    tb_style = BTN_ACCENT;
    int ap = tb_button(&x, y, "Apply", has, 0);
    tb_style = BTN_GHOST;
    if (ap && has) start_apply(packs.v[sel].path);
    if (tb_button(&x, y, "Delete", has, 0) && has) {
        char d[200], t[300];
        display_name(&packs.v[sel], d, sizeof(d));
        snprintf(t, sizeof(t), "Delete \"%s\"? This only removes the saved pack, not your installed mods.", d);
        snprintf(pending_delete, sizeof(pending_delete), "%s", packs.v[sel].path);
        dlg_message(TAG_PACK_DELETE, "Confirm", t, DB_YESNO, DI_QUESTION);
    }
    double rx = bar.x + bar.w - 8;
    if (tb_button_right(&rx, y, "Open Folder", 1, 2)) {
        char d[SMM_PATH];
        if (smm_packs_dir(d, sizeof(d))) smm_open_path(d);
    }
    if (tb_button_right(&rx, y, "Export", has, 2) && has) do_export();
    if (tb_button_right(&rx, y, "Import...", 1, 0)) do_import();

    R st = cut_bottom(&r, 22);
    fill(st, 0, P.bg);
    R prog = cut_bottom(&r, 3);
    int pct = app_progress_pct();
    fill(prog, 0, P.alt);
    if (pct > 0) fill(rect(prog.x, prog.y, prog.w * pct / 100.0, prog.h), 0, P.accent);
    tx_in(rect(st.x + 8, st.y, st.w - 8, st.h), status, 12, P.sub, 0, AL_LEFT);

    R left = cut_left(&r, 260);
    fill(left, 0, P.alt);
    vline(left.x + left.w - 1, left.y, left.y + left.h, col_rgb(55, 55, 55));
    R lv = rect(left.x, left.y, left.w - 1, left.h);
    fill(lv, 0, P.bg);
    double rh = 52;
    sc_begin(&lscroll, lv, packs.n * rh);
    for (int i = 0; i < packs.n; i++) {
        R row = rect(lv.x, lv.y + i * rh - lscroll.off, lv.w, rh);
        R card = rect(row.x + 6, row.y + 3, row.w - 12, row.h - 6);
        int hov = ui_hover(card);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        uint32_t hi = col_dark(P.bg) ? col_lighten(P.alt, 22) : col_darken(P.alt, 22);
        fill(card, 8, i == sel ? hi : P.surface);
        char d[200];
        display_name(&packs.v[i], d, sizeof(d));
        tx_fit(rect(card.x + 10, card.y + 4, card.w - 20, 20), d, 13.3, P.text, 1, AL_LEFT);
        char date[40], sub[160], ld[40] = "";
        fmt_date(packs.v[i].manifest.created_utc, date, sizeof(date), 0);
        if (packs.v[i].manifest.loader_kind >= 0) snprintf(ld, sizeof(ld), " \xc2\xb7 %s", smm_loader_name(packs.v[i].manifest.loader_kind));
        snprintf(sub, sizeof(sub), "%d mods \xc2\xb7 %s%s", packs.v[i].manifest.mods.n, date, ld);
        tx_fit(rect(card.x + 10, card.y + 26, card.w - 20, 16), sub, 10.7, P.sub, 0, AL_LEFT);
        if (ui_click(card)) {
            long now = (long)goome_now_ms();
            if (i == last_click_idx && now - last_click_ms < 400) switch_to_pack_game(&packs.v[i]);
            last_click_idx = i;
            last_click_ms = now;
            if (sel != i) {
                sel = i;
                show_detail();
            }
        }
    }
    sc_end(&lscroll, lv, packs.n * rh);

    if (!has) {
        fill(r, 0, P.bg);
        return;
    }
    fill(r, 0, P.alt);
    R d = inset(r, 12);
    SmmManifest *m = &packs.v[sel].manifest;
    char nm[200];
    display_name(&packs.v[sel], nm, sizeof(nm));
    tx_in(rect(d.x, d.y, d.w, 28), nm, 17.3, P.text, 1, AL_LEFT);
    char meta[400], date[40];
    fmt_date(m->created_utc, date, sizeof(date), 1);
    snprintf(meta, sizeof(meta), "by %s  \xc2\xb7  %s  \xc2\xb7  %s  \xc2\xb7  %s", m->author[0] ? m->author : "unknown", date, m->game_slug,
             m->loader_kind >= 0 ? smm_loader_name(m->loader_kind) : "any loader");
    tx_in(rect(d.x, d.y + 28, d.w, 22), meta, 12, P.sub, 0, AL_LEFT);
    double y2 = d.y + 56;
    char t[60];
    snprintf(t, sizeof(t), "Mods (%d)", m->mods.n);
    tx_in(rect(d.x, y2, d.w, 22), t, 12, P.sub, 0, AL_LEFT);
    R ml = rect(d.x, y2 + 24, d.w, 120);
    SmmStrsView mv = {m->mods.v, m->mods.n};
    str_list(ml, &mv, &mscroll, P.text);
    fill(ml, 0, 0);
    double y3 = ml.y + ml.h + 12;
    snprintf(t, sizeof(t), "Files (%d)", entries.n);
    tx_in(rect(d.x, y3, d.w, 22), t, 12, P.sub, 0, AL_LEFT);
    R fl = rect(d.x, y3 + 24, d.w, d.y + d.h - y3 - 24);
    SmmStrsView fv = {tree_lines, n_tree};
    str_list(fl, &fv, &fscroll, P.text);
}
