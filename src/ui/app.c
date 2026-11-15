#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static char status[256] = "Starting...";
static int progress;
static int cur_tab;
static SmmInstalledList inst;
static int inst_loaded;
static unsigned queue_seen;
static int queue_was_busy;

typedef struct {
    char url[512];
    volatile int state;
    SmmBuf buf;
    goome_bitmap bm;
    Job job;
} IconEntry;

#define ICON_MAX 96
static IconEntry icons[ICON_MAX];
static int n_icons;
static volatile int icon_busy;

static void job_main(void *p)
{
    Job *j = p;
    j->fn(j->arg);
    __sync_synchronize();
    j->state = 2;
}

int job_start(Job *j, void (*fn)(void *), void *arg)
{
    if (j->state == 1) return 0;
    j->state = 1;
    j->fn = fn;
    j->arg = arg;
    if (smm_thread_start(job_main, j) < 0) job_main(j);
    return 1;
}

int job_running(const Job *j)
{
    return j->state == 1;
}

int job_finished(Job *j)
{
    if (j->state != 2) return 0;
    __sync_synchronize();
    j->state = 0;
    return 1;
}

void app_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status, sizeof(status), fmt, ap);
    va_end(ap);
}

const char *app_status_text(void)
{
    return status;
}

void app_progress(int pct)
{
    progress = pct;
}

int app_progress_pct(void)
{
    return progress;
}

int app_has_game(void)
{
    return smm_settings.game_path[0] != 0;
}

int app_tab(void)
{
    return cur_tab;
}

void app_goto_tab(int tab)
{
    cur_tab = tab;
    switch (tab) {
    case TAB_MODS: mods_activate(); break;
    case TAB_INSTALLED: installed_activate(); break;
    case TAB_LOGS: logs_activate(); break;
    case TAB_LOADOUTS: loadouts_activate(); break;
    case TAB_MODPACKS: modpacks_activate(); break;
    case TAB_CONFIG: config_activate(); break;
    case TAB_SETTINGS: settings_activate(); break;
    default: break;
    }
}

void app_refresh_installed(void)
{
    smm_installed_free(&inst);
    memset(&inst, 0, sizeof(inst));
    if (app_has_game()) smm_installed_scan(&inst);
    inst_loaded = 1;
}

SmmInstalledList *app_installed(void)
{
    if (!inst_loaded) app_refresh_installed();
    return &inst;
}

void app_game_changed(void)
{
    app_refresh_installed();
    app_goto_tab(cur_tab);
}

void app_tick(void)
{
    unsigned q = smm_queue_changed();
    int busy = smm_queue_busy();
    SmmOp op;
    if (busy && smm_queue_current(&op) > 0) {
        app_progress(op.percent);
        app_status("[%d%%] %s", op.percent, op.status_text[0] ? op.status_text : op.name);
    }
    if (q != queue_seen || (queue_was_busy && !busy)) {
        queue_seen = q;
        if (queue_was_busy && !busy) {
            app_progress(0);
            SmmOp ops[40];
            int n = smm_queue_snapshot(ops, 40), last = -1;
            for (int i = 0; i < n; i++)
                if (ops[i].finished_at > 0 && (last < 0 || ops[i].finished_at >= ops[last].finished_at)) last = i;
            if (last >= 0 && ops[last].status == SMM_OP_FAILED) app_status("%s failed: %s", ops[last].name, ops[last].error);
            else app_status("Ready.");
            app_refresh_installed();
            if (cur_tab != TAB_MODS) app_goto_tab(cur_tab);
        }
    }
    queue_was_busy = busy;
}

static void icon_fetch(void *p)
{
    IconEntry *e = p;
    if (smm_http_get(e->url, &e->buf) < 0) e->state = 3;
    else e->state = 2;
    __sync_fetch_and_sub(&icon_busy, 1);
}

int icon_cache_get(const char *url, const goome_bitmap **out)
{
    if (!url || !*url) return 0;
    for (int i = 0; i < n_icons; i++) {
        IconEntry *e = &icons[i];
        if (strcmp(e->url, url) != 0) continue;
        if (e->state == 2) {
            if (e->buf.p && goome_bitmap_load_mem(e->buf.p, e->buf.n, &e->bm) == GOOME_OK) e->state = 4;
            else e->state = 3;
            smm_buf_free(&e->buf);
        }
        if (e->state == 4) {
            *out = &e->bm;
            return 1;
        }
        return 0;
    }
    if (n_icons >= ICON_MAX || icon_busy >= 4) return 0;
    IconEntry *e = &icons[n_icons++];
    memset(e, 0, sizeof(*e));
    snprintf(e->url, sizeof(e->url), "%s", url);
    e->state = 1;
    __sync_fetch_and_add(&icon_busy, 1);
    if (smm_thread_start(icon_fetch, e) < 0) icon_fetch(e);
    return 0;
}

int icon_cache_draw(const char *url, R r, double radius)
{
    const goome_bitmap *bm = NULL;
    if (!icon_cache_get(url, &bm)) return 0;
    goome_draw_bitmap(K.c, bm, r.x, r.y, r.w, r.h, goome_radius(radius), 0xFFFFFFFF);
    return 1;
}

void theme_refresh(void)
{
    SmmTheme t;
    const char *sb = smm_dc_get_string("Theme1", "");
    smm_theme_apply(smm_settings.theme, smm_settings.custom_background, smm_settings.custom_accent, sb, &t);
    P.bg = smm_rgb_pack(t.background) | 0xFF000000u;
    P.surface = smm_rgb_pack(t.surface) | 0xFF000000u;
    P.alt = smm_rgb_pack(t.surface_alt) | 0xFF000000u;
    P.border = smm_rgb_pack(t.border) | 0xFF000000u;
    P.text = smm_rgb_pack(t.text) | 0xFF000000u;
    P.sub = smm_rgb_pack(t.sub_text) | 0xFF000000u;
    P.accent = smm_rgb_pack(t.accent) | 0xFF000000u;
    P.accent_text = smm_rgb_pack(t.accent_text) | 0xFF000000u;
}

#define ART_SIDE 96

typedef struct {
    int appid;
    goome_bitmap bm;
} ArtEntry;

static ArtEntry art[16];
static int n_art;

static int art_square(int appid, const goome_bitmap **out)
{
    for (int i = 0; i < n_art; i++)
        if (art[i].appid == appid) {
            *out = &art[i].bm;
            return 1;
        }
    if (n_art >= 16) return 0;
    char url[200];
    snprintf(url, sizeof(url), "https://cdn.akamai.steamstatic.com/steam/apps/%d/library_600x900.jpg", appid);
    const goome_bitmap *src = NULL;
    if (!icon_cache_get(url, &src)) return 0;
    int side = src->w < src->h ? src->w : src->h;
    int ox = (src->w - side) / 2, oy = (src->h - side) / 4;
    uint32_t *px = malloc(sizeof(uint32_t) * ART_SIDE * ART_SIDE);
    if (!px) return 0;
    for (int y = 0; y < ART_SIDE; y++) {
        int y0 = oy + y * side / ART_SIDE, y1 = oy + (y + 1) * side / ART_SIDE;
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < ART_SIDE; x++) {
            int x0 = ox + x * side / ART_SIDE, x1 = ox + (x + 1) * side / ART_SIDE;
            if (x1 <= x0) x1 = x0 + 1;
            unsigned long sum[4] = {0, 0, 0, 0};
            int cnt = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    uint32_t c = src->px[yy * (src->stride > 0 ? src->stride : src->w) + xx];
                    sum[0] += c >> 24;
                    sum[1] += (c >> 16) & 255;
                    sum[2] += (c >> 8) & 255;
                    sum[3] += c & 255;
                    cnt++;
                }
            px[y * ART_SIDE + x] = ((uint32_t)(sum[0] / cnt) << 24) | ((uint32_t)(sum[1] / cnt) << 16) | ((uint32_t)(sum[2] / cnt) << 8) | (uint32_t)(sum[3] / cnt);
        }
    }
    ArtEntry *e = &art[n_art++];
    e->appid = appid;
    e->bm.px = px;
    e->bm.w = e->bm.h = ART_SIDE;
    e->bm.stride = ART_SIDE;
    e->bm.owned = 1;
    *out = &e->bm;
    return 1;
}

static int draw_steam_art(R r, const char *name)
{
    const SmmPreset *p = name ? smm_preset_find(name) : NULL;
    if (!p || p->steam_app_id <= 0) return 0;
    const goome_bitmap *bm = NULL;
    if (!art_square(p->steam_app_id, &bm)) return 0;
    goome_draw_bitmap(K.c, bm, r.x, r.y, r.w, r.h, goome_radius(r.w * 0.24), 0xFFFFFFFF);
    return 1;
}

void draw_game_dot(R r, const char *name)
{
    if (draw_steam_art(r, name)) return;
    unsigned h = 7;
    for (const char *s = name; s && *s; s++) h = h * 31 + (unsigned char)*s;
    static const uint32_t pal[] = {0xFF7C3AED, 0xFF2563EB, 0xFF059669, 0xFFD97706, 0xFFDC2626, 0xFFDB2777, 0xFF0891B2};
    uint32_t c = pal[h % 7];
    goome_draw_circle(K.c, r.x + r.w / 2, r.y + r.h / 2, r.w / 2, c);
    char ch[2] = {name && name[0] ? name[0] : '?', 0};
    if (ch[0] >= 'a' && ch[0] <= 'z') ch[0] -= 32;
    tx_in(r, ch, r.h * 0.55, 0xFFFFFFFF, 1, AL_CENTER);
}
