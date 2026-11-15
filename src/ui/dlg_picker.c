#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "dialogs.h"

typedef struct {
    char name[128];
    char path[SMM_PATH];
    char fallback[96];
    int result;
    char community[96];
    int has_source;
} Detect;

enum { DET_DONE, DET_PICK };

static Scroll pscroll;
static Detect det;
static Job det_job;
static int detecting;
static int picker_first;
static int purpose;
static char pending_name[128];
static char pending_path[SMM_PATH];

static SmmCommunities comms;
static Job comm_job;
static int comm_loaded;
static int comm_sel = -1;
static char comm_search[100];
static Scroll comm_scroll;
static int comm_failed;

static SmmSteamGames steam;
static Job steam_job;
static int steam_loaded;
static char steam_status[200];
static Scroll steam_scroll;
static int steam_busy;
static Job steam_find_job;
static int steam_pick_app;
static char steam_pick_name[128];
static char steam_pick_exe[SMM_PATH];
static int steam_find_done;

static uint32_t accent_for(const char *name)
{
    unsigned h = 5381;
    for (const char *s = name; *s; s++) h = h * 33 + (unsigned char)*s;
    static const uint32_t pal[] = {0xFF7C3AED, 0xFF2563EB, 0xFF059669, 0xFFD97706, 0xFFDC2626, 0xFFDB2777, 0xFF0891B2, 0xFF65A30D};
    return pal[h % 8];
}

static int file_ok(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void commit(const char *path, const char *name)
{
    snprintf(smm_settings.game_path, sizeof(smm_settings.game_path), "%s", path);
    snprintf(smm_settings.game_name, sizeof(smm_settings.game_name), "%s", name);
    const SmmPreset *p = smm_preset_find(name);
    if (p) smm_map_set(&smm_settings.known_game_paths, name, path);
    if (p && p->default_community && !smm_map_get(&smm_settings.thunderstore_communities, name))
        smm_map_set(&smm_settings.thunderstore_communities, name, p->default_community);
    smm_state_save();
    smm_ensure_sb_folder();
    smm_dc_load_settings();
    dlg_close_all();
    app_game_changed();
    app_status("Switched to %s.", name);
    if (!smm_settings.has_seen_tutorial) tutorial_start();
    picker_first = 0;
}

static void detect_worker(void *p)
{
    Detect *d = p;
    d->result = DET_DONE;
    if (smm_map_get(&smm_settings.thunderstore_communities, d->name) || smm_strs_find(&smm_settings.manual_only_games, d->name, 0) >= 0) return;
    char slug[64];
    smm_derive_slug(d->name, slug, sizeof(slug));
    int id = smm_sb_register_game(slug);
    if (id > 0) {
        SmmMods m;
        memset(&m, 0, sizeof(m));
        smm_sb_list_mods("all", 1, id, &m);
        int n = m.n;
        smm_mods_free(&m);
        if (n > 0) return;
    }
    char guess[96] = "";
    if (smm_ts_guess_community(d->name, guess, sizeof(guess)) >= 0 && guess[0]) {
        snprintf(d->community, sizeof(d->community), "%s", guess);
        return;
    }
    if (d->fallback[0]) {
        snprintf(d->community, sizeof(d->community), "%s", d->fallback);
        return;
    }
    d->result = DET_PICK;
}

static void begin_detect(const char *path, const char *name, const char *fallback)
{
    memset(&det, 0, sizeof(det));
    snprintf(det.name, sizeof(det.name), "%s", name);
    snprintf(det.path, sizeof(det.path), "%s", path);
    snprintf(det.fallback, sizeof(det.fallback), "%s", fallback ? fallback : "");
    snprintf(pending_name, sizeof(pending_name), "%s", name);
    snprintf(pending_path, sizeof(pending_path), "%s", path);
    detecting = 1;
    job_start(&det_job, detect_worker, &det);
}

static void poll_detect(void)
{
    if (!detecting || !job_finished(&det_job)) return;
    detecting = 0;
    if (det.community[0]) smm_map_set(&smm_settings.thunderstore_communities, det.name, det.community);
    if (det.result == DET_PICK) {
        purpose = 1;
        dlg_community_open();
        return;
    }
    commit(det.path, det.name);
}

static void pick_preset(const SmmPreset *p)
{
    const char *found = NULL;
    for (int k = 0; k < p->n_paths; k++)
        if (file_ok(p->default_paths[k])) {
            found = p->default_paths[k];
            break;
        }
    char buf[SMM_PATH];
    if (!found && p->steam_app_id > 0 && p->n_paths > 0) {
        const char *base = strrchr(p->default_paths[0], '\\');
        if (!base) base = strrchr(p->default_paths[0], '/');
        base = base ? base + 1 : p->default_paths[0];
        if (smm_steam_find_game_path(p->steam_app_id, base, buf, sizeof(buf)) > 0) found = buf;
    }
    const char *known = smm_map_get(&smm_settings.known_game_paths, p->name);
    if (!found && known && file_ok(known)) found = known;
    char picked[SMM_PATH];
    if (!found) {
        char title[160];
        snprintf(title, sizeof(title), "Locate %s executable", p->name);
        if (file_pick_open(title, "Executable", "exe", picked, sizeof(picked)) <= 0) return;
        smm_map_set(&smm_settings.known_game_paths, p->name, picked);
        found = picked;
    }
    if (strcmp(p->name, "Gorilla Tag") == 0) {
        commit(found, p->name);
        return;
    }
    if (strcmp(p->name, "ULTRAKILL") == 0 && p->default_community) {
        if (!smm_map_get(&smm_settings.thunderstore_communities, p->name)) smm_map_set(&smm_settings.thunderstore_communities, p->name, p->default_community);
        commit(found, p->name);
        return;
    }
    begin_detect(found, p->name, p->default_community);
}

static void commit_custom_exe(const char *exe)
{
    char name[128];
    const char *b = strrchr(exe, '/');
    if (!b) b = strrchr(exe, '\\');
    snprintf(name, sizeof(name), "%s", b ? b + 1 : exe);
    char *dot = strrchr(name, '.');
    if (dot) *dot = 0;
    smm_map_set(&smm_settings.custom_games, name, exe);
    begin_detect(exe, name, NULL);
}

void dlg_game_picker_open(void)
{
    Dlg *d = dlg_push(DK_PICKER);
    d->tag = 0;
    pscroll.off = 0;
    picker_first = !app_has_game();
}

static void tile(R r, const char *name, int mode_icons)
{
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    uint32_t base = col_lighten(P.alt, 20), hb = col_lighten(P.alt, 34);
    uint32_t u = accent_for(name);
    (void)mode_icons;
    fill(r, 14, col_lerp(hov ? hb : base, u, 0.35));
    draw_game_dot(rect(r.x + (r.w - 46) / 2, r.y + 14, 46, 46), name);
    tx_fit(rect(r.x + 4, r.y + r.h - 28, r.w - 8, 22), name, 11.3, P.text, 0, AL_CENTER);
}

void draw_picker(Dlg *d)
{
    poll_detect();
    R c;
    int closed = dlg_frame("Select Game", 440, 540, &c, 1);
    if (closed && !detecting) {
        if (picker_first && !app_has_game()) K.quit = 1;
        dlg_pop();
        return;
    }
    if (K.key == GOOME_K_ESC && !detecting && app_has_game()) {
        dlg_pop();
        return;
    }
    int saved = K.blocked;
    if (detecting) K.blocked = 1;
    fill_rd(rect(c.x - 15, c.y - 11, c.w + 30, c.h + 26), (goome_radii){0, 0, 11, 11}, P.alt);
    tx_in(rect(c.x + 4, c.y - 6, c.w, 26), "Choose a game to manage", 14.7, P.text, 1, AL_LEFT);
    hline(c.x - 16 + 1, c.x + c.w + 16 - 1, c.y + 22, P.border);
    R view = rect(c.x, c.y + 36, c.w, 54 * 7);
    int mode = smm_settings.game_picker_display;
    typedef struct {
        char name[128];
        const SmmPreset *p;
        const char *custom_path;
    } Item;
    static Item items[96];
    int n = 0;
    for (int i = 0; i < smm_preset_count() && n < 80; i++) {
        const SmmPreset *p = smm_preset_at(i);
        if (strcmp(p->name, "Custom") == 0 || smm_strs_find(&smm_settings.hidden_presets, p->name, 0) >= 0) continue;
        snprintf(items[n].name, 128, "%s", p->name);
        items[n].p = p;
        items[n].custom_path = NULL;
        n++;
    }
    for (int i = 0; i < smm_settings.custom_games.n && n < 90; i++) {
        snprintf(items[n].name, 128, "%s", smm_settings.custom_games.k[i]);
        items[n].p = NULL;
        items[n].custom_path = smm_settings.custom_games.v[i];
        n++;
    }
    double total;
    int cols = 3;
    double tile_w = 92, tile_h = 112, gx = 12;
    if (mode == SMM_PICKER_ICONS) {
        int rows = (n + cols - 1) / cols;
        total = rows * (tile_h + 8) + 2 * 54;
    } else total = (n + 2) * 54;
    sc_begin(&pscroll, view, total);
    double y = view.y - pscroll.off;
    if (mode == SMM_PICKER_ICONS) {
        double gw = cols * (tile_w + gx), x0 = view.x + (view.w - gw) / 2 + gx / 2;
        for (int i = 0; i < n; i++) {
            R tr = rect(x0 + (i % cols) * (tile_w + gx), y + (i / cols) * (tile_h + 8), tile_w, tile_h);
            tile(tr, items[i].name, 1);
            if (ui_click(tr)) {
                if (items[i].p) pick_preset(items[i].p);
                else commit_custom_exe(items[i].custom_path);
            }
        }
        y += ((n + cols - 1) / cols) * (tile_h + 8);
    } else {
        for (int i = 0; i < n; i++) {
            R br = rect(view.x, y + i * 54, 380, 46);
            int hov = ui_hover(br);
            if (hov) ui_cursor(GOOME_CURSOR_HAND);
            uint32_t base = col_lighten(P.accent, 0), hb = col_lighten(P.accent, 18);
            fill(br, 10, col_lerp(hov ? hb : base, accent_for(items[i].name), 0.35));
            R tr = rect(br.x + 16, br.y, br.w - 70, br.h);
            if (mode == SMM_PICKER_BOTH) {
                tx_fit(tr, items[i].name, 13.3, P.accent_text, 0, AL_LEFT);
                draw_game_dot(rect(br.x + br.w - 40, br.y + 10, 26, 26), items[i].name);
            } else tx_fit(rect(br.x + 16, br.y, br.w - 32, br.h), items[i].name, 13.3, P.accent_text, 0, AL_CENTER);
            if (ui_click(br)) {
                if (items[i].p) pick_preset(items[i].p);
                else commit_custom_exe(items[i].custom_path);
            }
        }
        y += n * 54;
    }
    R sb = rect(view.x, y, 380, 46);
    if (w_button_col(sb, "Select Steam Game...", col_lighten(P.alt, 20), col_lighten(P.alt, 34), P.text, goome_radius(10))) dlg_steam_open();
    R cb = rect(view.x, y + 54, 380, 46);
    if (w_button_col(cb, "Custom Game...", col_lighten(P.alt, 20), col_lighten(P.alt, 34), P.text, goome_radius(10))) {
        char path[SMM_PATH];
        if (file_pick_open("Select Game Executable", "Executable", "exe", path, sizeof(path)) > 0) commit_custom_exe(path);
    }
    sc_end(&pscroll, view, total);
    K.blocked = saved;
    if (detecting) {
        goome_draw_rect(K.c, c.x - 15, c.y + 24, c.w + 30, c.h - 8, col_alpha(P.alt, 190));
        tx_in(rect(c.x, c.y + c.h / 2 - 20, c.w, 40), "Looking up mod source...", 14, P.text, 0, AL_CENTER);
    }
    (void)d;
}

void dlg_steam_open(void)
{
    Dlg *d = dlg_push(DK_STEAM);
    d->tag = 0;
    steam_loaded = 0;
    steam_busy = 0;
    steam_scroll.off = 0;
    snprintf(steam_status, sizeof(steam_status), "Scanning Steam library...");
}

static void steam_scan(void *p)
{
    (void)p;
    smm_steam_games_free(&steam);
    memset(&steam, 0, sizeof(steam));
    smm_steam_list_installed(&steam);
}

static void steam_find(void *p)
{
    (void)p;
    steam_pick_exe[0] = 0;
    char cached_key[24];
    snprintf(cached_key, sizeof(cached_key), "%d", steam_pick_app);
    const char *cached = smm_map_get(&smm_settings.steam_exe_paths, cached_key);
    if (cached && file_ok(cached)) {
        snprintf(steam_pick_exe, sizeof(steam_pick_exe), "%s", cached);
        steam_find_done = 1;
        return;
    }
    SmmStrs c;
    memset(&c, 0, sizeof(c));
    smm_steam_exe_candidates(steam_pick_app, &c);
    const char *best = c.n > 0 ? c.v[0] : NULL;
    if (c.n > 1) {
        for (int i = 0; i < c.n; i++) {
            const char *f = strrchr(c.v[i], '/');
            if (!f) f = strrchr(c.v[i], '\\');
            f = f ? f + 1 : c.v[i];
            char stem[200];
            snprintf(stem, sizeof(stem), "%s", f);
            char *dot = strrchr(stem, '.');
            if (dot) *dot = 0;
            char dirpath[SMM_PATH];
            snprintf(dirpath, sizeof(dirpath), "%.*s", (int)(f - c.v[i]) - 1 > 0 ? (int)(f - c.v[i]) - 1 : 0, c.v[i]);
            const char *dn = strrchr(dirpath, '/');
            if (!dn) dn = strrchr(dirpath, '\\');
            dn = dn ? dn + 1 : dirpath;
            if (strcasecmp(stem, dn) == 0) {
                best = c.v[i];
                break;
            }
        }
    }
    if (best) snprintf(steam_pick_exe, sizeof(steam_pick_exe), "%s", best);
    smm_strs_free(&c);
    steam_find_done = 1;
}

void draw_steam(Dlg *d)
{
    if (job_finished(&steam_job)) {
        steam_loaded = 1;
        if (steam.n == 0) snprintf(steam_status, sizeof(steam_status), "No Steam games found.");
        else snprintf(steam_status, sizeof(steam_status), "%d games found. Click one to select it.", steam.n);
    }
    if (steam_busy == 0 && !steam_loaded && !job_running(&steam_job)) job_start(&steam_job, steam_scan, NULL);
    if (steam_busy && steam_find_done && job_finished(&steam_find_job)) {
        steam_busy = 0;
        steam_find_done = 0;
        if (!steam_pick_exe[0]) {
            char t[200];
            snprintf(t, sizeof(t), "Couldn't find an executable for \"%s\".", steam_pick_name);
            dlg_message(TAG_GENERIC, "Not Found", t, DB_OK, DI_WARN);
            snprintf(steam_status, sizeof(steam_status), "Click a game to select it.");
        } else {
            char key[24];
            snprintf(key, sizeof(key), "%d", steam_pick_app);
            smm_map_set(&smm_settings.steam_exe_paths, key, steam_pick_exe);
            smm_state_save();
            dlg_pop();
            commit_custom_exe(steam_pick_exe);
            return;
        }
    }
    R c;
    int closed = dlg_frame("Select Steam Game", 580, 510, &c, 1);
    if (closed || K.key == GOOME_K_ESC) {
        dlg_pop();
        return;
    }
    tx_in(rect(c.x, c.y - 6, c.w, 26), steam_status, 12.7, P.sub, 0, AL_LEFT);
    R view = rect(c.x, c.y + 24, c.w, c.h - 24 - 44);
    double cw = 120, ch = 190, gap = 12;
    int cols = (int)((view.w - 14 + gap) / (cw + gap));
    if (cols < 1) cols = 1;
    int rows = (steam.n + cols - 1) / cols;
    double total = rows * (ch + gap);
    int saved = K.blocked;
    if (steam_busy) K.blocked = 1;
    sc_begin(&steam_scroll, view, total);
    for (int i = 0; i < steam.n; i++) {
        R card = rect(view.x + (i % cols) * (cw + gap), view.y + (i / cols) * (ch + gap) - steam_scroll.off, cw, ch);
        if (card.y + ch < view.y || card.y > view.y + view.h) continue;
        int hov = ui_hover(card);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        fill(card, 8, hov ? col_lighten(P.alt, 14) : P.alt);
        R pic = rect(card.x + 4, card.y + 4, cw - 8, 160);
        fill(pic, 6, P.surface);
        char url[200];
        snprintf(url, sizeof(url), "https://cdn.akamai.steamstatic.com/steam/apps/%d/library_600x900.jpg", steam.v[i].appid);
        if (!icon_cache_draw(url, pic, 6)) draw_game_dot(rect(pic.x + (pic.w - 54) / 2, pic.y + 50, 54, 54), steam.v[i].name);
        tx_fit(rect(card.x + 4, card.y + 168, cw - 8, 18), steam.v[i].name, 11.3, P.text, 0, AL_CENTER);
        if (ui_click(card)) {
            steam_pick_app = steam.v[i].appid;
            snprintf(steam_pick_name, sizeof(steam_pick_name), "%s", steam.v[i].name);
            snprintf(steam_status, sizeof(steam_status), "Locating %s...", steam_pick_name);
            steam_busy = 1;
            steam_find_done = 0;
            job_start(&steam_find_job, steam_find, NULL);
        }
    }
    sc_end(&steam_scroll, view, total);
    K.blocked = saved;
    R row = rect(c.x, c.y + c.h - 34, c.w, 32);
    if (w_button(rect(row.x + row.w - 100, row.y, 100, 32), "Cancel", BTN_SOLID, !steam_busy)) dlg_pop();
    (void)d;
}

void dlg_community_open(void)
{
    Dlg *d = dlg_push(DK_COMMUNITY);
    d->tag = 0;
    comm_loaded = 0;
    comm_failed = 0;
    comm_sel = -1;
    comm_search[0] = 0;
    comm_scroll.off = 0;
}

static void comm_load(void *p)
{
    (void)p;
    smm_communities_free(&comms);
    memset(&comms, 0, sizeof(comms));
    smm_ts_list_communities(&comms);
}

static void comm_finish(int picked)
{
    const char *game = purpose == 1 ? pending_name : smm_settings.game_name;
    if (picked >= 0 && picked < comms.n) {
        smm_map_set(&smm_settings.thunderstore_communities, game, comms.v[picked].identifier);
        app_status("Using Thunderstore community \"%s\".", comms.v[picked].name);
    } else if (purpose == 1) {
        smm_strs_add(&smm_settings.manual_only_games, game);
    }
    dlg_pop();
    if (purpose == 1) {
        purpose = 0;
        commit(pending_path, pending_name);
        return;
    }
    smm_state_save();
    if (picked >= 0) mods_activate();
}

void draw_community(Dlg *d)
{
    if (job_finished(&comm_job)) {
        comm_loaded = 1;
        comm_failed = comms.n == 0;
        if (comm_failed) {
            dlg_pop();
            dlg_message(TAG_TS_FAIL, "Thunderstore", "Couldn't reach Thunderstore right now. Check your connection and try again.", DB_OK, DI_WARN);
            if (purpose == 1) {
                smm_strs_add(&smm_settings.manual_only_games, pending_name);
                purpose = 0;
                commit(pending_path, pending_name);
            }
            return;
        }
    }
    if (!comm_loaded && !job_running(&comm_job)) job_start(&comm_job, comm_load, NULL);
    R c;
    int closed = dlg_frame("Choose Thunderstore Community", 440, 520, &c, 1);
    if (closed || K.key == GOOME_K_ESC) {
        comm_finish(-1);
        return;
    }
    tx(c.x, c.y - 2, "Search for your game's Thunderstore community:", 13, P.text, 0);
    w_input("commsearch", rect(c.x, c.y + 24, c.w, 32), comm_search, sizeof(comm_search), "Search communities...", 0, goome_radius(6));
    R view = rect(c.x, c.y + 66, c.w, c.h - 66 - 44);
    fill(view, 0, P.surface);
    stroke(rect(view.x + 0.5, view.y + 0.5, view.w - 1, view.h - 1), 0, 1, P.border);
    int idx[2048], n = 0;
    char q[100];
    snprintf(q, sizeof(q), "%s", comm_search);
    for (char *p = q; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    for (int i = 0; i < comms.n && n < 2048; i++) {
        if (q[0]) {
            char a[260];
            snprintf(a, sizeof(a), "%s %s", comms.v[i].name, comms.v[i].identifier);
            for (char *p = a; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p += 32;
            if (!strstr(a, q)) continue;
        }
        idx[n++] = i;
    }
    double rh = 24;
    R inner = inset(view, 1);
    sc_begin(&comm_scroll, inner, n * rh);
    int dbl = 0;
    for (int i = 0; i < n; i++) {
        R row = rect(inner.x, inner.y + i * rh - comm_scroll.off, inner.w, rh);
        if (row.y + rh < inner.y || row.y > inner.y + inner.h) continue;
        int hov = ui_hover(row);
        uint32_t hi = col_dark(P.bg) ? col_lighten(P.alt, 22) : col_darken(P.alt, 22);
        if (idx[i] == comm_sel) fill(row, 0, hi);
        else if (hov) fill(row, 0, col_alpha(hi, 90));
        tx_fit(rect(row.x + 8, row.y, row.w - 16, row.h), comms.v[idx[i]].name, 12.7, P.text, 0, AL_LEFT);
        if (ui_click(row)) {
            long now = (long)goome_now_ms();
            if (comm_sel == idx[i] && now - (long)d->t < 400) dbl = 1;
            d->t = (double)now;
            comm_sel = idx[i];
        }
    }
    sc_end(&comm_scroll, inner, n * rh);
    R row = rect(c.x, c.y + c.h - 34, c.w, 32);
    if (w_button(rect(row.x + row.w - 208, row.y, 100, 32), "Select", BTN_ACCENT, comm_sel >= 0) || dbl) {
        if (comm_sel >= 0) comm_finish(comm_sel);
    } else if (w_button(rect(row.x + row.w - 100, row.y, 100, 32), "Cancel", BTN_SOLID, 1)) comm_finish(-1);
}
