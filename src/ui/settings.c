#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static Scroll scroll;
static double Y, X, W;
static char status[200] = "";
static int theme_sel;
static int style_sel;
static int loader_sel;
static int display_sel;
static int activity_open;
static int use_ts;
static char pending_bg[16], pending_ac[16];
static int force_kind = -1;

static void sync_state(void)
{
    theme_sel = smm_settings.theme;
    style_sel = smm_settings.layout;
    display_sel = smm_settings.game_picker_display;
    const char *ov = smm_map_get(&smm_settings.loader_overrides, smm_settings.game_name);
    loader_sel = !ov ? 0 : strcasecmp(ov, "BepInEx") == 0 ? 1 : 2;
    use_ts = smm_map_get(&smm_settings.thunderstore_communities, smm_settings.game_name) != NULL;
    snprintf(pending_bg, sizeof(pending_bg), "%s", smm_settings.custom_background);
    snprintf(pending_ac, sizeof(pending_ac), "%s", smm_settings.custom_accent);
}

void settings_activate(void)
{
    sync_state();
}

static void head(const char *title)
{
    Y += 14;
    char up[64];
    size_t i = 0;
    for (; title[i] && i < 63; i++) up[i] = title[i] >= 'a' && title[i] <= 'z' ? title[i] - 32 : title[i];
    up[i] = 0;
    tx(X, Y, up, 10, P.sub, 1);
    Y += 22;
}

static void label(const char *s, uint32_t c)
{
    tx(X + 14, Y, s, 13, c, 0);
    Y += 22;
}

static int check(const char *s, int *v)
{
    int r = w_check(X + 14, Y, s, v);
    Y += 28;
    return r;
}

static int radio(const char *s, int *v, int value)
{
    int r = w_radio(X + 14, Y, s, v, value);
    Y += 28;
    return r;
}

static int button(double *x, const char *s, int enabled, int icon)
{
    double w = tx_w(s, 13, 0) + 24 + (icon ? 16 : 0);
    if (*x > X + 14 && *x + w > X + W - 8) {
        *x = X + 14;
        Y += 40;
    }
    R r = rect(*x, Y, w, 32);
    int hit = w_button(r, "", BTN_SOLID, enabled);
    uint32_t fg = enabled ? P.text : col_alpha(P.text, 110);
    tx_fit(inset(rect(r.x, r.y, r.w - (icon ? 16 : 0), r.h), 4), s, 13, fg, 0, AL_CENTER);
    if (icon) ic_ext(rect(r.x + r.w - 22, r.y + 11, 10, 10), fg);
    *x += w + 8;
    return hit;
}

static int link(double x, const char *s)
{
    double w = tx_w(s, 13, 0);
    R r = rect(x, Y, w, 20);
    int hov = ui_hover(r);
    if (hov) ui_cursor(GOOME_CURSOR_HAND);
    tx(x, Y + 1, s, 13, hov ? col_rgb(100, 180, 255) : col_rgb(64, 148, 255), 0);
    hline(x, x + w, Y + 19, hov ? col_rgb(100, 180, 255) : col_rgb(64, 148, 255));
    return ui_click(r);
}

static void apply_theme_live(void)
{
    theme_refresh();
}

void settings_set_layout(int layout)
{
    style_sel = layout;
    smm_settings.layout = layout;
    smm_settings.theme = layout == SMM_LAYOUT_MONKE ? SMM_THEME_WHITE : layout == SMM_LAYOUT_R2MODMAN ? SMM_THEME_R2MODMAN : SMM_THEME_BLACK;
    theme_sel = smm_settings.theme;
    smm_state_save();
    apply_theme_live();
}

static void change_game(void)
{
    char path[SMM_PATH];
    if (file_pick_open("Select Game Executable", "Executable", "exe", path, sizeof(path)) <= 0) return;
    snprintf(smm_settings.game_path, sizeof(smm_settings.game_path), "%s", path);
    snprintf(smm_settings.game_name, sizeof(smm_settings.game_name), "Custom");
    char lower[SMM_PATH];
    snprintf(lower, sizeof(lower), "%s", path);
    for (char *p = lower; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    for (int i = 0; i < smm_preset_count(); i++) {
        const SmmPreset *p = smm_preset_at(i);
        if (strcmp(p->name, "Custom") == 0) continue;
        int match = 0;
        for (int k = 0; k < p->n_paths; k++)
            if (strcasecmp(p->default_paths[k], path) == 0) match = 1;
        if (!match && strstr(lower, p->slug)) match = 1;
        if (match) {
            snprintf(smm_settings.game_name, sizeof(smm_settings.game_name), "%s", p->name);
            break;
        }
    }
    smm_state_save();
    smm_ensure_sb_folder();
    smm_dc_load_settings();
    app_game_changed();
    sync_state();
    snprintf(status, sizeof(status), "Game set to: %s", smm_settings.game_name);
}

static void set_override(int sel)
{
    const char *game = smm_settings.game_name;
    if (sel == 0) {
        smm_map_del(&smm_settings.loader_overrides, game);
        smm_state_save();
        snprintf(status, sizeof(status), "Loader override cleared, back to auto-detect.");
        return;
    }
    int newk = sel == 1 ? SMM_LOADER_BEPINEX : SMM_LOADER_MELON;
    int oldk = newk == SMM_LOADER_BEPINEX ? SMM_LOADER_MELON : SMM_LOADER_BEPINEX;
    char dir[SMM_PATH];
    int has_dir = smm_game_dir(dir, sizeof(dir)) > 0;
    if (has_dir && smm_loader_installed(oldk, dir)) {
        force_kind = newk;
        char t[700];
        snprintf(t, sizeof(t),
                 "%s currently has %s installed. Switching to force %s will delete %s entirely, including its mods folder, since a game can only use one loader at a time.\n\nThis cannot be undone. Continue?",
                 game, smm_loader_name(oldk), smm_loader_name(newk), smm_loader_name(oldk));
        dlg_message(TAG_LOADER_SWITCH, "Switch Mod Loader", t, DB_YESNO, DI_WARN);
        return;
    }
    smm_map_set(&smm_settings.loader_overrides, game, smm_loader_name(newk));
    smm_state_save();
    snprintf(status, sizeof(status), "%s is now forced to use %s.", game, smm_loader_name(newk));
}

static void check_dialogs(void)
{
    int r = dlg_result(TAG_LOADER_SWITCH);
    if (r == DR_YES && force_kind >= 0) {
        char dir[SMM_PATH];
        int oldk = force_kind == SMM_LOADER_BEPINEX ? SMM_LOADER_MELON : SMM_LOADER_BEPINEX;
        if (smm_game_dir(dir, sizeof(dir)) > 0) smm_loader_uninstall(oldk, dir);
        smm_map_set(&smm_settings.loader_overrides, smm_settings.game_name, smm_loader_name(force_kind));
        smm_state_save();
        snprintf(status, sizeof(status), "%s removed. %s is now forced to %s. Click Install %s to set it up.", smm_loader_name(oldk), smm_settings.game_name,
                 smm_loader_name(force_kind), smm_loader_name(force_kind));
        force_kind = -1;
        app_refresh_installed();
    } else if (r == DR_NO || r == DR_CANCEL) {
        sync_state();
        snprintf(status, sizeof(status), "Loader switch cancelled.");
        force_kind = -1;
    }
    char hex[32];
    if (dlg_prompt_result(TAG_COLOR_BG, hex, sizeof(hex)) > 0) {
        uint32_t c;
        if (col_parse(hex, &c)) col_hex(c, pending_bg);
    }
    if (dlg_prompt_result(TAG_COLOR_ACCENT, hex, sizeof(hex)) > 0) {
        uint32_t c;
        if (col_parse(hex, &c)) col_hex(c, pending_ac);
    }
}

static void sec_style(void)
{
    head("Style");
    int ns = style_sel;
    radio("Sev's Mod Manager (default)", &ns, SMM_LAYOUT_SMM);
    radio("r2modman", &ns, SMM_LAYOUT_R2MODMAN);
    radio("MonkeModManager", &ns, SMM_LAYOUT_MONKE);
    if (ns != style_sel) settings_set_layout(ns);

    
}

static void sec_game(void)
{
    head("Game");
    char gp[SMM_PATH + 16];
    if (app_has_game()) snprintf(gp, sizeof(gp), "Game: %s", smm_settings.game_path);
    else snprintf(gp, sizeof(gp), "No game selected.");
    tx_fit(rect(X + 14, Y, W - 60, 20), gp, 13, P.text, 0, AL_LEFT);
    Y += 22;
    int kind = smm_detect_loader();
    char dir[SMM_PATH];
    int has_dir = app_has_game() && smm_game_dir(dir, sizeof(dir)) > 0;
    int ok = has_dir && smm_loader_installed(kind, dir);
    char ls[120];
    snprintf(ls, sizeof(ls), "%s: %s", smm_loader_name(kind), ok ? "Installed" : "Not found");
    if (ok) ic_check(rect(X + 14 + tx_w(ls, 13, 0) + 8, Y + 4, 11, 11), col_rgb(39, 201, 63));
    label(ls, ok ? col_rgb(39, 201, 63) : col_rgb(255, 95, 86));
    Y += 6;
    tx(X + 14, Y, "Force mod loader (for unusual games):", 13, P.sub, 0);
    Y += 24;
    static const char *const LO[3] = {"Auto-detect (default)", "BepInEx", "MelonLoader"};
    if (w_dropdown("loaderdd", rect(X + 14, Y, 190, 34), LO, 3, &loader_sel, goome_radius(8))) set_override(loader_sel);
    Y += 42;
    double bx = X + 14;
    if (button(&bx, "Change Game", 1, 0)) change_game();
    char t[80];
    snprintf(t, sizeof(t), "Install %s", smm_loader_name(kind));
    if (button(&bx, t, has_dir && !ok, 0)) smm_queue_install_loader(kind);
    snprintf(t, sizeof(t), "Update %s", smm_loader_name(kind));
    if (button(&bx, t, has_dir && ok, 0)) smm_queue_install_loader(kind);
    if (button(&bx, "Open Game Folder", has_dir, 1) && has_dir) smm_open_path(dir);
    Y += 42;
    int si = smm_strs_find(&smm_settings.disable_starting_indicator, smm_settings.game_name, 1) < 0;
    int nsi = si;
    if (w_check(X + 14, Y + 2, "Show \"Starting...\" on the Play button while a game launches", &nsi)) {
        if (nsi) smm_strs_remove(&smm_settings.disable_starting_indicator, smm_settings.game_name);
        else smm_strs_add(&smm_settings.disable_starting_indicator, smm_settings.game_name);
        smm_state_save();
    }
    Y += 30;

    
}

static void sec_manage(void)
{
    head("Manage Games");
    for (int i = 0; i < smm_preset_count(); i++) {
        const SmmPreset *p = smm_preset_at(i);
        if (strcmp(p->name, "Custom") == 0) continue;
        char l[160];
        snprintf(l, sizeof(l), "Show %s in picker", p->name);
        int shown = smm_strs_find(&smm_settings.hidden_presets, p->name, 0) < 0;
        if (w_check(X + 14, Y, l, &shown)) {
            if (shown) smm_strs_remove(&smm_settings.hidden_presets, p->name);
            else smm_strs_add(&smm_settings.hidden_presets, p->name);
            smm_state_save();
        }
        Y += 26;
    }
    Y += 8;
    tx(X + 14, Y, "Custom games:", 13, P.text, 0);
    Y += 24;
    if (smm_settings.custom_games.n == 0) {
        tx(X + 28, Y, "(none)", 13, P.sub, 0);
        Y += 24;
    }
    int rm = -1;
    for (int i = 0; i < smm_settings.custom_games.n; i++) {
        tx_fit(rect(X + 28, Y, 320, 28), smm_settings.custom_games.k[i], 13, P.text, 0, AL_LEFT);
        double rx = X + (W > 460 ? 360 : W - 100);
        if (button(&rx, "Remove", 1, 0)) rm = i;
        Y += 36;
    }
    if (rm >= 0) {
        char nm[128];
        snprintf(nm, sizeof(nm), "%s", smm_settings.custom_games.k[rm]);
        smm_map_del(&smm_settings.custom_games, nm);
        smm_state_save();
        snprintf(status, sizeof(status), "Removed \"%s\" from the picker.", nm);
    }
    Y += 6;
    tx(X + 14, Y, "Game picker display:", 13, P.text, 0);
    Y += 24;
    static const char *const DM[3] = {"Icon + Text", "Text Only", "Icons Only"};
    if (w_dropdown("displaydd", rect(X + 14, Y, 170, 34), DM, 3, &display_sel, goome_radius(8))) {
        smm_settings.game_picker_display = display_sel;
        smm_state_save();
    }
    Y += 46;

    
}

static void sec_source(void)
{
    double bx = X + 14;
    head("Mod Source");
    int nts = use_ts;
    if (w_check(X + 14, Y, "Use Thunderstore for this game", &nts)) {
        use_ts = nts;
        if (!nts) {
            smm_map_del(&smm_settings.thunderstore_communities, smm_settings.game_name);
            smm_state_save();
            snprintf(status, sizeof(status), "%s now uses Sev's Mod Manager's own mod list.", smm_settings.game_name);
        } else {
            char guess[96];
            if (smm_ts_guess_community(smm_settings.game_name, guess, sizeof(guess)) >= 0 && guess[0]) {
                smm_map_set(&smm_settings.thunderstore_communities, smm_settings.game_name, guess);
                smm_state_save();
                snprintf(status, sizeof(status), "Using Thunderstore community \"%s\".", guess);
            } else {
                snprintf(status, sizeof(status), "Couldn't guess a community for this game, pick one.");
                dlg_community_open();
            }
        }
    }
    Y += 30;
    const char *comm = smm_map_get(&smm_settings.thunderstore_communities, smm_settings.game_name);
    char cl[200];
    if (comm) snprintf(cl, sizeof(cl), "Community: %s", comm);
    else snprintf(cl, sizeof(cl), "Uses Sev's Mod Manager's own mod list.");
    label(cl, P.sub);
    if (comm && button(&bx, "Change Community...", 1, 0)) dlg_community_open();
    Y += 40;

    
}

static void sec_theme(void)
{
    head("Theme");
    int nt = theme_sel;
    radio("Black (default)", &nt, SMM_THEME_BLACK);
    radio("White", &nt, SMM_THEME_WHITE);
    radio("Custom", &nt, SMM_THEME_CUSTOM);
    radio("Synced with Several Bees", &nt, SMM_THEME_SYNCED_SB);
    radio("r2modman", &nt, SMM_THEME_R2MODMAN);
    if (nt != theme_sel) {
        theme_sel = nt;
        smm_settings.theme = nt;
        smm_state_save();
        apply_theme_live();
    }
    if (theme_sel == SMM_THEME_CUSTOM) {
        uint32_t bg = 0xFF141414, ac = 0xFF7C3AED;
        col_parse(pending_bg, &bg);
        col_parse(pending_ac, &ac);
        double cx = X + 34;
        fill(rect(cx, Y, 32, 32), 6, bg);
        stroke(rect(cx + 0.5, Y + 0.5, 31, 31), 6, 1, P.border);
        cx += 40;
        if (button(&cx, "Background Color", 1, 0)) dlg_prompt(TAG_COLOR_BG, "Background Color", "Hex color (for example #141414):", pending_bg);
        fill(rect(cx, Y, 32, 32), 6, ac);
        stroke(rect(cx + 0.5, Y + 0.5, 31, 31), 6, 1, P.border);
        cx += 40;
        if (button(&cx, "Accent Color", 1, 0)) dlg_prompt(TAG_COLOR_ACCENT, "Accent Color", "Hex color (for example #7C3AED):", pending_ac);
        if (button(&cx, "Apply Custom Theme", 1, 0)) {
            snprintf(smm_settings.custom_background, sizeof(smm_settings.custom_background), "%s", pending_bg);
            snprintf(smm_settings.custom_accent, sizeof(smm_settings.custom_accent), "%s", pending_ac);
            smm_settings.theme = SMM_THEME_CUSTOM;
            smm_state_save();
            apply_theme_live();
        }
        Y += 44;
    }

    
}

static void sec_about(void)
{
    double bx = X + 14;
    head("About");
    tx(X + 14, Y, "Sev's Mod Manager by ", 13, P.text, 0);
    double bw = tx_w("Sev's Mod Manager by ", 13, 0);
    if (link(X + 14 + bw, "sev")) smm_open_url("https://sevvy-wevvy.com/");
    tx(X + 14 + bw + tx_w("sev", 13, 0), Y, " :)", 13, P.text, 0);
    Y += 28;
    if (link(X + 14, "Please donate if you like this project!")) smm_open_url("https://sevvy-wevvy.com/donate");
    Y += 34;
    if (button(&bx, "Switch Game...", 1, 0)) dlg_game_picker_open();
    Y += 40;

    
}

static void sec_updates(void)
{
    head("Updates");
    int au = smm_settings.auto_check_updates;
    if (check("Check for Sev's Mod Manager updates on startup", &au)) {
        smm_settings.auto_check_updates = au;
        smm_state_save();
    }
    int rpc = !smm_settings.disable_discord_rpc;
    if (check("Show Discord Rich Presence while modding", &rpc)) {
        smm_settings.disable_discord_rpc = !rpc;
        smm_state_save();
    }

    
}

static void sec_tutorial(void)
{
    double bx = X + 14;
    head("Tutorial");
    if (button(&bx, "Redo Tutorial", 1, 0)) {
        smm_settings.has_seen_tutorial = 0;
        smm_state_save();
        tutorial_start();
    }
    Y += 40;

    
}

static void sec_activity(void)
{
    head("Activity");
    char tg[80];
    snprintf(tg, sizeof(tg), "Recent operations");
    R tr = rect(X + 14, Y, tx_w(tg, 13, 0) + 26, 24);
    if (ui_hover(tr)) ui_cursor(GOOME_CURSOR_HAND);
    if (activity_open) ic_chev_down(rect(X + 15, Y + 9, 9, 5), P.text);
    else goome_draw_triangle(K.c, X + 17, Y + 6, X + 17, Y + 16, X + 22, Y + 11, P.text);
    tx(X + 30, Y + 2, tg, 13, P.text, 0);
    if (ui_click(tr)) activity_open = !activity_open;
    Y += 30;
    if (activity_open) {
        SmmOp ops[40];
        int n = smm_queue_snapshot(ops, 40);
        if (n <= 0) {
            tx(X + 28, Y, "No operations yet.", 13, P.sub, 0);
            Y += 24;
        }
        for (int i = n - 1; i >= 0; i--) {
            SmmOp *o = &ops[i];
            char ln[600];
            uint32_t oc = o->status == SMM_OP_FAILED ? col_rgb(255, 95, 86) : o->status == SMM_OP_DONE ? col_rgb(39, 201, 63) : P.text;
            if (o->status == SMM_OP_RUNNING) snprintf(ln, sizeof(ln), "%s: %d%% %s", o->name, o->percent, o->status_text);
            else if (o->status == SMM_OP_FAILED) snprintf(ln, sizeof(ln), "%s failed: %s", o->name, o->error);
            else if (o->status == SMM_OP_DONE) snprintf(ln, sizeof(ln), "%s", o->name);
            else snprintf(ln, sizeof(ln), "%s (waiting)", o->name);
            if (o->status == SMM_OP_DONE) ic_check(rect(X + 28, Y + 6, 10, 10), oc);
            else if (o->status == SMM_OP_FAILED) ic_cross(rect(X + 28, Y + 6, 9, 9), oc);
            else if (o->status == SMM_OP_RUNNING) goome_draw_triangle(K.c, X + 28, Y + 5, X + 28, Y + 16, X + 37, Y + 10.5, oc);
            else goome_draw_circle(K.c, X + 32, Y + 11, 2.5, oc);
            tx_fit(rect(X + 46, Y, W - 100, 22), ln, 13, oc, 0, AL_LEFT);
            Y += 24;
        }
    }
}

typedef void (*Section)(void);

static void mascot(R r)
{
    double cx = r.x + r.w / 2, cy = r.y + 90;
    uint32_t c = P.accent;
    goome_draw_circle(K.c, cx - 30, cy - 36, 2.5, c);
    goome_draw_circle(K.c, cx + 34, cy - 30, 2.5, c);
    goome_draw_line(K.c, cx - 22, cy - 18, cx + 22, cy - 22, 5, c);
    goome_draw_line(K.c, cx - 22, cy - 18, cx - 14, cy + 4, 5, c);
    goome_draw_line(K.c, cx - 14, cy + 4, cx + 20, cy + 6, 5, c);
    goome_draw_line(K.c, cx - 34, cy + 22, cx - 24, cy + 32, 4, c);
    goome_draw_line(K.c, cx - 24, cy + 32, cx + 24, cy + 32, 4, c);
    goome_draw_line(K.c, cx + 24, cy + 32, cx + 34, cy + 22, 4, c);
    tx_in(rect(cx - 60, cy + 52, 120, 40), "smm", 34, c, 1, AL_CENTER);
}

static double column(const Section *list, int n, double x, double w, double y)
{
    X = x;
    W = w;
    Y = y;
    for (int i = 0; i < n; i++) list[i]();
    return Y;
}

void panel_settings(R r)
{
    check_dialogs();
    R st = cut_bottom(&r, 22);
    tx_in(rect(st.x + 8, st.y, st.w - 8, st.h), status, 12, P.sub, 0, AL_LEFT);
    fill(r, 0, P.alt);
    static double last_h = 1500;
    sc_begin(&scroll, r, last_h);
    double top = r.y - scroll.off + 4;
    double end;
    int lay = smm_settings.layout;
    if (lay == SMM_LAYOUT_MONKE) {
        static const Section left[] = {sec_tutorial, sec_style, sec_game, sec_manage, sec_source};
        static const Section right[] = {sec_activity, sec_about, sec_theme, sec_updates};
        double cw = 320;
        double a = column(left, 5, r.x + 12, cw - 12, top);
        double b = column(right, 4, r.x + r.w - cw, cw - 12, top);
        end = a > b ? a : b;
        mascot(rect(r.x + cw, r.y, r.w - 2 * cw, r.h));
    } else if (lay == SMM_LAYOUT_R2MODMAN) {
        static const Section all[] = {sec_about, sec_game, sec_style, sec_theme, sec_manage, sec_source, sec_updates, sec_tutorial, sec_activity};
        end = column(all, 9, r.x + 24, r.w - 24, top);
    } else {
        static const Section all[] = {sec_style, sec_game, sec_manage, sec_source, sec_theme, sec_about, sec_updates, sec_tutorial, sec_activity};
        end = column(all, 9, r.x + 18, r.w - 18, top);
    }
    last_h = end - top + 30;
    sc_end(&scroll, r, last_h);
}
