#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

typedef struct {
    int mode;
    char tab[16];
    int page;
    int game_id;
    char community[96];
    int use_ts;
    int use_mf;
    int local;
    int manual;
    SmmMods out;
    int more;
    int ok;
    char err[200];
    char user[96];
    int token_ok;
    int new_game_id;
    int upvoted[512];
    int n_upvoted;
} LoadReq;

enum { LM_INIT, LM_LOAD, LM_MORE };

static const char *const TABS[4] = {"all", "featured", "verified", "unverified"};
static const char *const TAB_NAMES[4] = {"All", "Featured", "Verified", "Unverified"};

static SmmMods cache;
static SmmMods view;
static int tab_idx;
static int page = 1;
static int game_id;
static int more_visible;
static char search[160];
static char list_status[200] = "Loading...";
static int sel = -1;
static char sel_key[200];
static Scroll list_scroll;
static Job load_job;
static LoadReq *req;
static char token[256];
static char username[96];
static int upvoted[512];
static int n_upvoted;
static int login_pending;
static char login_code[32];
static Job login_job;
static volatile int login_state;
static char login_user[96];
static char login_tok[256];
static int tutorial_on;
static int tutorial_busy;
static int tutorial_pct;
static int tutorial_done;
static int ctx_open;
static double ctx_x, ctx_y;
static int ctx_frame;

static void drop_view(void)
{
    free(view.v);
    memset(&view, 0, sizeof(view));
    sel = -1;
}

static int use_mf(void)
{
    return smm_mf_active();
}

static int use_ts(void)
{
    return !use_mf() && smm_map_get(&smm_settings.thunderstore_communities, smm_settings.game_name) != NULL;
}

static int local_list(void)
{
    return use_ts() || use_mf();
}

static const char *community(void)
{
    return smm_map_get(&smm_settings.thunderstore_communities, smm_settings.game_name);
}

static int manual_only(void)
{
    return smm_strs_find(&smm_settings.manual_only_games, smm_settings.game_name, 0) >= 0;
}

int mods_loading(void)
{
    return job_running(&load_job);
}

void mods_set_status(const char *s)
{
    snprintf(list_status, sizeof(list_status), "%s", s);
}

static void mod_key(const SmmMod *m, char *out, size_t n)
{
    smm_mod_key(m, out, n);
}

static int is_installed(const SmmMod *m)
{
    return smm_mod_is_installed(m, app_installed());
}

static int is_disabled(const SmmMod *m)
{
    char key[200];
    mod_key(m, key, sizeof(key));
    const SmmInstalledList *l = app_installed();
    for (int i = 0; i < l->n; i++)
        if (!l->v[i].enabled && strcasecmp(l->v[i].name, key) == 0) return 1;
    return 0;
}

static void remember_selection(void)
{
    sel_key[0] = 0;
    if (sel >= 0 && sel < view.n) snprintf(sel_key, sizeof(sel_key), "%d|%s", view.v[sel].source, view.v[sel].dll_name);
}

static void rebuild_view(void)
{
    remember_selection();
    free(view.v);
    memset(&view, 0, sizeof(view));
    SmmMods tmp;
    memset(&tmp, 0, sizeof(tmp));
    if (local_list()) smm_filter_mods(&cache, TABS[tab_idx], &tmp);
    else for (int i = 0; i < cache.n; i++) smm_mods_add(&tmp, &cache.v[i]);
    smm_sort_mods(&tmp);
    char q[160];
    snprintf(q, sizeof(q), "%s", search);
    for (char *p = q; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    for (int i = 0; i < tmp.n; i++) {
        if (q[0]) {
            char a[256], b[256];
            snprintf(a, sizeof(a), "%s", tmp.v[i].name);
            snprintf(b, sizeof(b), "%s", tmp.v[i].author);
            for (char *p = a; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p += 32;
            for (char *p = b; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p += 32;
            if (!strstr(a, q) && !strstr(b, q)) continue;
        }
        smm_mods_add(&view, &tmp.v[i]);
    }
    free(tmp.v);
    int found = -1;
    if (sel_key[0])
        for (int i = 0; i < view.n; i++) {
            char k[200];
            snprintf(k, sizeof(k), "%d|%s", view.v[i].source, view.v[i].dll_name);
            if (strcmp(k, sel_key) == 0) found = i;
        }
    sel = found;
}

typedef struct {
    SmmMod *mod;
    int *pending;
    int ok;
} ExistJob;

static void exist_check(void *p)
{
    ExistJob *j = p;
    char url[512];
    j->ok = smm_mod_download_url(j->mod, url, sizeof(url)) >= 0 && smm_http_url_exists(url) > 0;
    __sync_fetch_and_sub(j->pending, 1);
}

static void load_worker(void *p)
{
    LoadReq *r = p;
    r->ok = 1;
    if (r->manual) return;
    if (r->mode == LM_INIT && !r->local) {
        if (smm_token_load(token, sizeof(token)) > 0 && token[0]) {
            smm_sb_set_token(token);
            if (smm_sb_validate_token(token, r->user, sizeof(r->user)) > 0) r->token_ok = 1;
            else smm_sb_set_token(NULL);
        }
        char slug[64];
        smm_current_slug(slug, sizeof(slug));
        int id = smm_sb_register_game(slug);
        r->new_game_id = id > 0 ? id : 0;
        r->game_id = r->new_game_id;
    }
    if (r->use_mf) {
        if (smm_mf_list(&r->out) < 0) r->ok = 0;
    } else if (r->use_ts) {
        if (smm_ts_list_packages(r->community, &r->out) < 0) r->ok = 0;
    } else {
        SmmMods got;
        memset(&got, 0, sizeof(got));
        if (smm_sb_list_mods(r->tab, r->page, r->game_id, &got) < 0) r->ok = 0;
        r->more = got.n >= 15;
        if (!smm_demo_enabled()) {
            ExistJob *jobs = calloc((size_t)(got.n ? got.n : 1), sizeof(*jobs));
            int pending = got.n;
            for (int i = 0; i < got.n; i++) {
                jobs[i].mod = &got.v[i];
                jobs[i].pending = &pending;
                if (smm_thread_start(exist_check, &jobs[i]) < 0) exist_check(&jobs[i]);
            }
            while (__sync_fetch_and_add(&pending, 0) > 0) smm_sleep_ms(20);
            for (int i = 0; i < got.n; i++) {
                if (jobs[i].ok) smm_mods_add(&r->out, &got.v[i]);
                else smm_strs_free(&got.v[i].deps);
            }
            free(jobs);
            free(got.v);
        } else {
            r->out = got;
        }
        if (r->ok && smm_sb_token()) {
            for (int i = 0; i < r->out.n && r->n_upvoted < 512; i++)
                if (smm_sb_user_upvoted(r->out.v[i].id) > 0) r->upvoted[r->n_upvoted++] = r->out.v[i].id;
        }
    }
    if (!r->ok) snprintf(r->err, sizeof(r->err), "%s", smm_error());
}

static void start_load(int mode)
{
    if (job_running(&load_job)) return;
    if (manual_only()) {
        drop_view();
    smm_mods_free(&cache);
        memset(&cache, 0, sizeof(cache));
        rebuild_view();
        mods_set_status("No mod library for this game. Manage mods manually via Installed.");
        return;
    }
    free(req);
    req = calloc(1, sizeof(*req));
    req->mode = mode;
    snprintf(req->tab, sizeof(req->tab), "%s", TABS[tab_idx]);
    req->page = page;
    req->game_id = game_id;
    req->use_ts = use_ts();
    req->use_mf = use_mf();
    req->local = req->use_ts || req->use_mf;
    snprintf(req->community, sizeof(req->community), "%s", community() ? community() : "");
    if (req->use_ts && !req->community[0]) {
        mods_set_status("No Thunderstore community configured.");
        return;
    }
    if (req->local && cache.n > 0 && mode != LM_INIT) {
        rebuild_view();
        char t[64];
        snprintf(t, sizeof(t), "%d mods", view.n);
        mods_set_status(t);
        return;
    }
    mods_set_status("Loading...");
    more_visible = 0;
    job_start(&load_job, load_worker, req);
}

void mods_poll(void)
{
    if (job_finished(&load_job) && req) {
        if (!req->ok) {
            char t[260];
            snprintf(t, sizeof(t), "Load failed: %s", req->err);
            mods_set_status(t);
        } else {
            if (req->mode == LM_INIT && !req->local) {
                game_id = req->new_game_id;
                snprintf(username, sizeof(username), "%s", req->user);
                if (!req->token_ok) token[0] = 0;
            }
            if (req->local) {
                drop_view();
    smm_mods_free(&cache);
                cache = req->out;
            } else {
                if (page == 1 || req->mode != LM_MORE) {
                    drop_view();
    smm_mods_free(&cache);
                    cache = req->out;
                } else {
                    for (int i = 0; i < req->out.n; i++) smm_mods_add(&cache, &req->out.v[i]);
                    free(req->out.v);
                }
                n_upvoted = req->n_upvoted;
                memcpy(upvoted, req->upvoted, sizeof(upvoted));
                more_visible = req->more;
            }
            memset(&req->out, 0, sizeof(req->out));
            rebuild_view();
            char t[64];
            snprintf(t, sizeof(t), "%d mods", view.n);
            mods_set_status(t);
            app_status("Ready.");
        }
        free(req);
        req = NULL;
    }
    if (login_state == 2) {
        login_state = 0;
        login_pending = 0;
        login_code[0] = 0;
        snprintf(token, sizeof(token), "%s", login_tok);
        snprintf(username, sizeof(username), "%s", login_user);
        char t[160];
        snprintf(t, sizeof(t), "Logged in as @%s", username);
        mods_set_status(t);
    } else if (login_state == 3) {
        login_state = 0;
        login_pending = 0;
        login_code[0] = 0;
        mods_set_status("Login timed out.");
    }
}

void mods_init(void)
{
    sel = -1;
    start_load(LM_INIT);
}

void mods_activate(void)
{
    page = 1;
    drop_view();
    smm_mods_free(&cache);
    memset(&cache, 0, sizeof(cache));
    start_load(LM_INIT);
}

void mods_select(int idx)
{
    mods_poll();
    if (idx >= 0 && idx < view.n) sel = idx;
}

static void switch_tab(int i)
{
    tab_idx = i;
    page = 1;
    if (local_list()) rebuild_view();
    else {
        drop_view();
    smm_mods_free(&cache);
        memset(&cache, 0, sizeof(cache));
        rebuild_view();
        start_load(LM_LOAD);
    }
}

static void toggle_source(void)
{
    const char *game = smm_settings.game_name;
    if (use_mf()) {
        smm_mf_set_active(0);
        smm_state_save();
        char t[200];
        snprintf(t, sizeof(t), "%s now uses Sev's Mod Manager's own mod list.", game);
        mods_set_status(t);
        mods_activate();
        return;
    }
    if (use_ts() && smm_mf_supported()) {
        smm_map_del(&smm_settings.thunderstore_communities, game);
        smm_mf_set_active(1);
        smm_state_save();
        mods_set_status("Using MonkeForge.");
        mods_activate();
        return;
    }
    if (use_ts()) {
        smm_map_del(&smm_settings.thunderstore_communities, game);
        smm_state_save();
        char t[200];
        snprintf(t, sizeof(t), "%s now uses Sev's Mod Manager's own mod list.", game);
        mods_set_status(t);
        mods_activate();
        return;
    }
    char guess[96];
    if (smm_ts_guess_community(game, guess, sizeof(guess)) >= 0 && guess[0]) {
        smm_map_set(&smm_settings.thunderstore_communities, game, guess);
        smm_state_save();
        char t[200];
        snprintf(t, sizeof(t), "Using Thunderstore community \"%s\".", guess);
        mods_set_status(t);
        mods_activate();
    } else {
        mods_set_status("Couldn't guess a community, pick one.");
        dlg_community_open();
    }
}

static void login_worker(void *p)
{
    (void)p;
    char code[32] = "";
    if (smm_sb_login_code(code, sizeof(code)) < 0 || !code[0]) {
        login_state = 3;
        return;
    }
    snprintf(login_code, sizeof(login_code), "%s", code);
    char url[120];
    snprintf(url, sizeof(url), "https://3gv.org/link?code=%s", code);
    smm_open_url(url);
    for (int i = 0; i < 60; i++) {
        smm_sleep_ms(3000);
        char tok[256] = "";
        if (smm_sb_poll_login(code, tok, sizeof(tok)) > 0 && tok[0]) {
            smm_token_save(tok);
            smm_sb_set_token(tok);
            char user[96] = "";
            smm_sb_validate_token(tok, user, sizeof(user));
            snprintf(login_tok, sizeof(login_tok), "%s", tok);
            snprintf(login_user, sizeof(login_user), "%s", user);
            login_state = 2;
            return;
        }
    }
    login_state = 3;
}

static void do_login(void)
{
    if (login_pending && login_code[0]) {
        char url[120];
        snprintf(url, sizeof(url), "https://3gv.org/link?code=%s", login_code);
        smm_open_url(url);
        return;
    }
    login_pending = 1;
    job_start(&login_job, login_worker, NULL);
}

static void do_logout(void)
{
    token[0] = 0;
    username[0] = 0;
    smm_sb_set_token(NULL);
    smm_token_delete();
    n_upvoted = 0;
    mods_set_status("Logged out.");
}

static int is_upvoted(int id)
{
    for (int i = 0; i < n_upvoted; i++)
        if (upvoted[i] == id) return 1;
    return 0;
}

static void do_upvote(SmmMod *m)
{
    if (!token[0]) {
        do_login();
        return;
    }
    int now = 0;
    if (smm_sb_toggle_upvote(m->id, &now) < 0) return;
    if (now) {
        if (n_upvoted < 512) upvoted[n_upvoted++] = m->id;
        m->upvotes++;
    } else {
        for (int i = 0; i < n_upvoted; i++)
            if (upvoted[i] == m->id) upvoted[i] = upvoted[--n_upvoted];
        if (m->upvotes > 0) m->upvotes--;
    }
}

static SmmMod tutorial_mod;

static SmmMod *selected(void)
{
    if (tutorial_on) return &tutorial_mod;
    return sel >= 0 && sel < view.n ? &view.v[sel] : NULL;
}

void mods_tutorial_show(int on)
{
    tutorial_on = on;
    tutorial_busy = 0;
    tutorial_pct = 0;
    tutorial_done = 0;
    if (on) {
        memset(&tutorial_mod, 0, sizeof(tutorial_mod));
        tutorial_mod.id = -1;
        snprintf(tutorial_mod.name, sizeof(tutorial_mod.name), "Example Mod");
        snprintf(tutorial_mod.dll_name, sizeof(tutorial_mod.dll_name), "TutorialExampleMod");
        snprintf(tutorial_mod.description, sizeof(tutorial_mod.description), "A demo mod for the tutorial. Nothing real gets downloaded.");
        snprintf(tutorial_mod.author, sizeof(tutorial_mod.author), "Tutorial");
        tutorial_mod.verified = 1;
        tutorial_mod.source = SMM_SRC_API;
    }
}

int mods_tutorial_installed(void)
{
    return tutorial_done;
}

static void queue_install(SmmMod *m)
{
    if (!app_has_game()) {
        mods_set_status("No game path set.");
        return;
    }
    int kind = smm_detect_loader();
    char game[SMM_PATH];
    smm_game_dir(game, sizeof(game));
    if (!smm_loader_installed(kind, game)) {
        smm_queue_install_loader(kind);
    }
    if (m->source == SMM_SRC_THUNDERSTORE && !m->download_url[0]) {
        mods_set_status("This package has no download available.");
        return;
    }
    SmmMods missing;
    memset(&missing, 0, sizeof(missing));
    if (m->source == SMM_SRC_THUNDERSTORE) {
        smm_missing_deps(m, &cache, app_installed(), &missing);
        for (int i = 0; i < missing.n; i++) smm_queue_install_mod(&missing.v[i]);
        free(missing.v);
    }
    smm_queue_install_mod(m);
    app_status("Installing %s...", m->name);
}

static void do_install(SmmMod *m)
{
    if (tutorial_on) {
        tutorial_busy = 1;
        tutorial_pct = 0;
        return;
    }
    if (!m->verified) {
        char t[400];
        snprintf(t, sizeof(t), "\"%s\" is unverified. It hasn't been reviewed and could contain malicious code. Install anyway?", m->name);
        dlg_message(TAG_UNVERIFIED, "Unverified Mod", t, DB_YESNO, DI_WARN);
        return;
    }
    queue_install(m);
}

static void do_uninstall(SmmMod *m)
{
    SmmStrs deps;
    memset(&deps, 0, sizeof(deps));
    smm_dependents_installed(m, &cache, app_installed(), &deps);
    if (deps.n > 0) {
        char list[300] = "";
        for (int i = 0; i < deps.n; i++) {
            if (i) strncat(list, ", ", sizeof(list) - strlen(list) - 1);
            strncat(list, deps.v[i], sizeof(list) - strlen(list) - 1);
        }
        char t[700];
        snprintf(t, sizeof(t), "%s depend%s on this mod and may break without it. Uninstall anyway?", list, deps.n == 1 ? "s" : "");
        dlg_message(TAG_UNINSTALL_DEPS, "Other Mods Depend On This", t, DB_YESNO, DI_WARN);
        smm_strs_free(&deps);
        return;
    }
    smm_strs_free(&deps);
    char key[200];
    mod_key(m, key, sizeof(key));
    smm_mod_uninstall(key);
    smm_mod_version_delete(key);
    app_refresh_installed();
}

static void finish_uninstall(SmmMod *m)
{
    char key[200];
    mod_key(m, key, sizeof(key));
    smm_mod_uninstall(key);
    smm_mod_version_delete(key);
    app_refresh_installed();
}

static void install_external(void)
{
    char path[SMM_PATH];
    if (file_pick_open("Select Mod DLL", "DLL files", "dll", path, sizeof(path)) <= 0) return;
    char name[200];
    const char *b = strrchr(path, '/');
    if (!b) b = strrchr(path, '\\');
    snprintf(name, sizeof(name), "%s", b ? b + 1 : path);
    char *dot = strrchr(name, '.');
    if (dot) *dot = 0;
    if (smm_mod_install_local(path) >= 0) {
        char t[260];
        snprintf(t, sizeof(t), "%s installed.", name);
        mods_set_status(t);
        app_refresh_installed();
    } else {
        char t[300];
        snprintf(t, sizeof(t), "Install failed: %s", smm_error());
        mods_set_status(t);
    }
}

static R install_r;

const SmmMods *mods_latest(void)
{
    return local_list() ? &cache : NULL;
}

R mods_install_rect(void)
{
    return install_r;
}

static void draw_row(R row, const SmmMod *m, int selected_row, int hov)
{
    if (selected_row || hov) {
        SmmTheme t;
        smm_theme_current(&t);
        uint32_t hi = col_dark(P.bg) ? col_lighten(P.alt, 22) : col_darken(P.alt, 22);
        fill(rect(row.x + 4, row.y + 2, row.w - 8, row.h - 4), 8, selected_row ? hi : col_alpha(hi, 90));
    }
    const char *badge = m->featured ? "Featured" : m->verified ? "Verified" : "Unverified";
    uint32_t bc = m->featured ? col_rgb(255, 189, 46) : m->verified ? col_rgb(39, 201, 63) : col_rgb(136, 136, 136);
    double right = row.x + row.w - 12;
    if (m->icon_url[0]) {
        R ir = rect(row.x + row.w - 52, row.y + (row.h - 40) / 2, 40, 40);
        fill(ir, 6, P.alt);
        icon_cache_draw(m->icon_url, ir, 6);
        right = ir.x - 8;
    }
    double x = row.x + 12, y = row.y + 6;
    tx(x, y, badge, 9.3, bc, 0);
    tx_fit(rect(x, y + 12, right - x, 20), m->name, 13.3, P.text, 1, AL_LEFT);
    char sub[200];
    snprintf(sub, sizeof(sub), "@%s  \xc2\xb7  ", m->author);
    double sw = tx_w(sub, 10.7, 0);
    tx(x, y + 31, sub, 10.7, P.sub, 0);
    char up[24];
    if (m->source == SMM_SRC_MONKEFORGE) {
        snprintf(up, sizeof(up), "%d downloads", m->upvotes);
        tx(x + sw, y + 31, up, 10.7, P.sub, 0);
    } else {
        snprintf(up, sizeof(up), "%d", m->upvotes);
        ic_up_tri(rect(x + sw, y + 35, 7, 7), P.sub);
        tx(x + sw + 8, y + 31, up, 10.7, P.sub, 0);
    }
    char key[200];
    mod_key(m, key, sizeof(key));
    char ver[48];
    smm_mod_version_get(m->repo_url, ver, sizeof(ver));
    if (ver[0]) goome_draw_circle(K.c, (m->icon_url[0] ? row.x + row.w - 56 : row.x + row.w - 16) + 4, row.y + 12, 4, col_rgb(255, 95, 86));
    hline(row.x, row.x + row.w, row.y + row.h - 1, P.border);
}

static void draw_list(R lp)
{
    R inner = inset(lp, 2);
    R status = cut_bottom(&inner, 22);
    R prog = cut_bottom(&inner, 3);
    R more = rect(0, 0, 0, 0);
    if (more_visible) more = cut_bottom(&inner, 32);
    tx_in(rect(status.x + 6, status.y, status.w - 6, status.h), list_status, 12, P.sub, 0, AL_LEFT);
    int pct = tutorial_busy ? tutorial_pct : app_progress_pct();
    if (pct > 0) fill(rect(prog.x, prog.y, prog.w * pct / 100.0, prog.h), 0, P.accent);
    if (more_visible && w_button(rect(more.x + 2, more.y + 2, more.w - 4, more.h - 4), "Load More", BTN_SOLID, !job_running(&load_job))) {
        page++;
        start_load(LM_MORE);
    }
    fill(inner, 0, P.surface);
    double rh = 56;
    sc_begin(&list_scroll, inner, view.n * rh);
    int first = (int)(list_scroll.off / rh), last = (int)((list_scroll.off + inner.h) / rh) + 1;
    if (first < 0) first = 0;
    if (last > view.n) last = view.n;
    int ctx_row = -1;
    for (int i = first; i < last; i++) {
        R row = rect(inner.x, inner.y + i * rh - list_scroll.off, inner.w, rh);
        int hov = ui_hover(row);
        if (hov) ui_cursor(GOOME_CURSOR_HAND);
        if (ui_click(row)) sel = i;
        if (ui_rclick(row)) {
            sel = i;
            ctx_row = i;
        }
        draw_row(row, &view.v[i], i == sel, hov && i != sel);
    }
    if (ctx_row >= 0) {
        ctx_open = 1;
        ctx_x = K.mx;
        ctx_y = K.my;
        ctx_frame = K.frame;
    }
    sc_end(&list_scroll, inner, view.n * rh);
}

static void draw_ctx_menu(void)
{
    if (!ctx_open || sel < 0 || sel >= view.n) {
        ctx_open = 0;
        return;
    }
    SmmMod *m = &view.v[sel];
    int inst = is_installed(m), dis = !inst && is_disabled(m);
    const char *items[3];
    int n = 0;
    items[n++] = inst || dis ? "Uninstall" : "Install";
    if (m->repo_url[0]) {
        items[n++] = m->source == SMM_SRC_MONKEFORGE ? "Open MonkeForge Page" : m->source == SMM_SRC_THUNDERSTORE ? "Open Thunderstore Page" : "Open GitHub Page";
        items[n++] = "Copy Link";
    }
    double w = 200, ih = 30, h = n * ih + 8;
    R b = rect(ctx_x, ctx_y, w, h);
    if (b.x + w > K.w) b.x = K.w - w - 4;
    if (b.y + h > K.h) b.y = K.h - h - 4;
    int saved = K.blocked;
    K.blocked = 0;
    shadow(b, 8, 12, 0x77000000);
    fill(b, 8, col_lighten(P.surface, 6));
    stroke(inset(b, 0.5), 8, 1, P.border);
    int pick = -1;
    for (int i = 0; i < n; i++) {
        R ir = rect(b.x + 4, b.y + 4 + i * ih, b.w - 8, ih);
        int hov = ui_hover(ir);
        if (hov) fill(ir, 6, P.alt), ui_cursor(GOOME_CURSOR_HAND);
        tx_in(rect(ir.x + 10, ir.y, ir.w - 20, ir.h), items[i], 13, P.text, 0, AL_LEFT);
        if (ui_click(ir)) pick = i;
    }
    if (K.pressed && !inside(b, K.mx, K.my) && K.frame != ctx_frame) ctx_open = 0;
    K.blocked = saved;
    if (pick < 0) return;
    ctx_open = 0;
    if (pick == 0) {
        if (inst || dis) do_uninstall(m);
        else do_install(m);
    } else if (pick == 1) smm_open_url(m->repo_url);
    else if (pick == 2) goome_clipboard_set(K.g, m->repo_url);
}

static double btn_w(const char *s)
{
    return tx_w(s, 13, 0) + 22;
}

static void draw_detail(R dp)
{
    fill(dp, 0, P.alt);
    SmmMod *m = selected();
    if (!m) return;
    R c = inset(dp, 16);
    tx(c.x, c.y, m->name, 20, P.text, 1);
    char a[200];
    snprintf(a, sizeof(a), "by @%s  \xc2\xb7  ", m->author);
    double aw = tx_w(a, 13, 0);
    tx(c.x, c.y + 32, a, 13, P.sub, 0);
    char up[96];
    if (m->source == SMM_SRC_MONKEFORGE) {
        snprintf(up, sizeof(up), "%d downloads  \xc2\xb7  v%s", m->upvotes, m->version);
        tx(c.x + aw, c.y + 32, up, 13, P.sub, 0);
    } else {
        snprintf(up, sizeof(up), "%d", m->upvotes);
        ic_up_tri(rect(c.x + aw, c.y + 37, 9, 9), P.sub);
        tx(c.x + aw + 12, c.y + 32, up, 13, P.sub, 0);
    }
    R bottom = rect(c.x, c.y + c.h - 60, c.w, 60);
    R desc = rect(c.x, c.y + 64, c.w, c.h - 64 - 60);
    tx_wrap(desc, m->description[0] ? m->description : "(no description)", 13, P.sub, 0, NULL);

    int inst = tutorial_on ? tutorial_done : is_installed(m);
    int dis = !tutorial_on && !inst && is_disabled(m);
    char st[64];
    snprintf(st, sizeof(st), "%s %s", inst || dis ? "\xe2\x97\x8f" : "\xe2\x97\x8b", inst ? "Installed" : dis ? "Disabled" : "Not installed");
    uint32_t stc = inst ? col_rgb(39, 201, 63) : dis ? col_rgb(255, 189, 46) : P.sub;
    double sx = c.x + c.w - 150;
    R sr = rect(sx, bottom.y, 150, bottom.h);
    double dot = 8;
    double tw = tx_w(inst || dis ? "Installed" : "Not installed", 13, 1);
    if (dis) tw = tx_w("Disabled", 13, 1);
    const char *word = inst ? "Installed" : dis ? "Disabled" : "Not installed";
    tx_in(sr, word, 13, stc, 1, AL_RIGHT);
    R dr = rect(sr.x + sr.w - tw - 16, sr.y + sr.h / 2 - dot / 2, dot, dot);
    if (inst || dis) goome_draw_circle(K.c, dr.x + 4, dr.y + 4, 4, stc);
    else goome_draw_ring(K.c, dr.x + 4, dr.y + 4, 3.4, 1.3, stc);
    (void)st;

    double x = bottom.x, by = bottom.y + (bottom.h - 30) / 2;
    char ibl[48];
    snprintf(ibl, sizeof(ibl), "%s", m->verified ? "Install" : "Install (Unverified)");
    if (!inst && !dis) {
        R br = rect(x, by, btn_w(ibl), 30);
        install_r = br;
        int busy = tutorial_busy || smm_queue_busy();
        if (w_button(br, ibl, BTN_ACCENT, !busy)) do_install(m);
        x += br.w + 6;
    }
    if (m->source == SMM_SRC_API) {
        char t[32];
        int uv = is_upvoted(m->id);
        snprintf(t, sizeof(t), "%s", uv ? "Upvoted" : "Upvote");
        R br = rect(x, by, btn_w(t) + 14, 30);
        if (w_button(br, t, BTN_SOLID, 1)) do_upvote(m);
        if (uv) tx_in(br, t, 13, P.accent, 0, AL_CENTER);
        ic_up_tri(rect(br.x + 10, br.y + 11, 8, 8), uv ? P.accent : P.text);
        x += br.w + 6;
    }
    if (inst || dis) {
        R br = rect(x, by, btn_w("Uninstall"), 30);
        if (w_button(br, "Uninstall", BTN_SOLID, 1)) {
            if (tutorial_on) tutorial_done = 0;
            else do_uninstall(m);
        }
        x += br.w + 6;
    }
    const char *gh = m->source == SMM_SRC_MONKEFORGE ? "MonkeForge" : m->source == SMM_SRC_THUNDERSTORE ? "Thunderstore" : "GitHub";
    R gr = rect(x, by, btn_w(gh) + 16, 30);
    if (w_button(gr, "", BTN_SOLID, 1) && !tutorial_on && m->repo_url[0]) smm_open_url(m->repo_url);
    tx_in(rect(gr.x, gr.y, gr.w - 16, gr.h), gh, 13, P.text, 0, AL_CENTER);
    ic_ext(rect(gr.x + gr.w - 22, gr.y + 10, 10, 10), P.text);
    x += gr.w + 6;
    if (m->source == SMM_SRC_THUNDERSTORE && m->repo_url[0]) {
        R cr = rect(x, by, btn_w("Changelog") + 16, 30);
        int cl = w_button(cr, "", BTN_SOLID, 1);
        tx_in(rect(cr.x, cr.y, cr.w - 16, cr.h), "Changelog", 13, P.text, 0, AL_CENTER);
        if (cl) {
            char url[600];
            snprintf(url, sizeof(url), "%s/changelog/", m->repo_url);
            smm_open_url(url);
        }
        ic_ext(rect(cr.x + cr.w - 22, cr.y + 10, 10, 10), P.text);
    }
}

void panel_mods(R r)
{
    mods_poll();
    if (tutorial_busy) {
        tutorial_pct += 2;
        K.animating = 1;
        if (tutorial_pct >= 100) {
            tutorial_busy = 0;
            tutorial_done = 1;
            tutorial_pct = 0;
            mods_set_status("Example Mod installed.");
        } else {
            char t[80];
            snprintf(t, sizeof(t), "[%d%%] Installing Example Mod...", tutorial_pct);
            mods_set_status(t);
        }
    }
    int r_ = dlg_result(TAG_UNVERIFIED);
    if (r_ == DR_YES && sel >= 0 && sel < view.n) queue_install(&view.v[sel]);
    r_ = dlg_result(TAG_UNINSTALL_DEPS);
    if (r_ == DR_YES && sel >= 0 && sel < view.n) finish_uninstall(&view.v[sel]);

    R top = cut_top(&r, 44);
    fill(top, 0, P.surface);
    goome_radii bot = {0, 0, 8, 8};
    R row = rect(top.x + 8, top.y, top.w - 16, 38);
    R dd = cut_left(&row, 130);
    if (w_dropdown("tabdd", dd, TAB_NAMES, 4, &tab_idx, bot)) switch_tab(tab_idx);
    cut_left(&row, 6);
    char src[200];
    if (manual_only()) snprintf(src, sizeof(src), "Source: None (manual)");
    else if (use_mf()) snprintf(src, sizeof(src), "Source: MonkeForge");
    else if (use_ts()) snprintf(src, sizeof(src), "Source: Thunderstore (%s)", community());
    else snprintf(src, sizeof(src), "Source: Sev's Mod Manager");
    R sb = rect(row.x, row.y, btn_w(src) + 6, 38);
    if (w_button_rd(sb, src, BTN_GHOST, 1, bot)) toggle_source();
    int login_vis = !manual_only() && !local_list();
    R right = row;
    R lg = rect(0, 0, 0, 0);
    if (login_vis) lg = cut_right(&right, 70);
    if (login_vis) cut_right(&right, 6);
    R se = cut_right(&right, 160);
    char lt[96];
    if (login_pending) snprintf(lt, sizeof(lt), "%s", login_code[0] ? login_code : "...");
    else if (token[0]) snprintf(lt, sizeof(lt), "@%s", username);
    else snprintf(lt, sizeof(lt), "Login");
    if (login_vis) {
        double lw = btn_w(lt);
        if (lw > 70) lg = rect(lg.x + lg.w - lw, lg.y, lw, lg.h), se.x -= lw - 70;
        if (w_button_rd(lg, lt, BTN_SOLID, 1, bot)) {
            if (token[0]) do_logout();
            else do_login();
        }
    }
    if (w_input("search", se, search, sizeof(search), "Search mods...", 0, bot)) {
        rebuild_view();
        char t[64];
        snprintf(t, sizeof(t), "%d mods", view.n);
        mods_set_status(t);
    }
    double ew = btn_w("Install External Mod");
    R ex = rect(top.x + (top.w - ew) / 2, top.y + 8, ew, 28);
    if (w_button(ex, "Install External Mod", BTN_SOLID, 1)) install_external();

    R lp = cut_left(&r, 380);
    fill(lp, 0, P.alt);
    vline(lp.x + lp.w - 1, lp.y, lp.y + lp.h, col_rgb(55, 55, 55));
    draw_list(rect(lp.x, lp.y, lp.w - 1, lp.h));
    draw_detail(r);
    draw_ctx_menu();
}
