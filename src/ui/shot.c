#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

void chrome_draw(void);
void chrome_reset(void);
void app_frame(goome_ui *u);
void app_startup(const char *pack_path, int skip_picker);
void mods_select(int idx);
int mods_loading(void);
void loadouts_select(int i);
void modpacks_select(int i);
void logs_select_session(int i);
void shot_prepare(int step);

static uint32_t crc_table[256];

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static uint32_t crc_update(uint32_t crc, const unsigned char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) crc = crc_table[(crc ^ b[i]) & 255] ^ (crc >> 8);
    return crc;
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24), p[1] = (unsigned char)(v >> 16), p[2] = (unsigned char)(v >> 8), p[3] = (unsigned char)v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n)
{
    unsigned char h[8];
    put32(h, (uint32_t)n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t crc = crc_update(0xFFFFFFFFu, (const unsigned char *)type, 4);
    crc = crc_update(crc, data, n) ^ 0xFFFFFFFFu;
    unsigned char t[4];
    put32(t, crc);
    fwrite(t, 1, 4, f);
}

static int png_write(const char *path, const uint32_t *px, int w, int h, int stride)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    crc_init();
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    unsigned char ihdr[13];
    put32(ihdr, (uint32_t)w);
    put32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8, ihdr[9] = 2, ihdr[10] = 0, ihdr[11] = 0, ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t row = (size_t)w * 3 + 1, raw = row * (size_t)h;
    unsigned char *buf = malloc(raw);
    if (!buf) {
        fclose(f);
        return -1;
    }
    for (int y = 0; y < h; y++) {
        unsigned char *d = buf + row * (size_t)y;
        *d++ = 0;
        for (int x = 0; x < w; x++) {
            uint32_t s = px[(size_t)y * (size_t)stride + (size_t)x];
            *d++ = (unsigned char)(s >> 16), *d++ = (unsigned char)(s >> 8), *d++ = (unsigned char)s;
        }
    }
    size_t nblocks = (raw + 65534) / 65535;
    size_t zn = 2 + raw + nblocks * 5 + 4;
    unsigned char *z = malloc(zn);
    size_t zp = 0;
    z[zp++] = 0x78, z[zp++] = 1;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw;) {
        size_t n = raw - off > 65535 ? 65535 : raw - off;
        z[zp++] = off + n >= raw;
        z[zp++] = (unsigned char)n, z[zp++] = (unsigned char)(n >> 8);
        z[zp++] = (unsigned char)~n, z[zp++] = (unsigned char)(~n >> 8);
        memcpy(z + zp, buf + off, n);
        for (size_t i = 0; i < n; i++) a = (a + buf[off + i]) % 65521, b = (b + a) % 65521;
        zp += n, off += n;
    }
    put32(z + zp, b << 16 | a);
    zp += 4;
    chunk(f, "IDAT", z, zp);
    chunk(f, "IEND", NULL, 0);
    free(z);
    free(buf);
    fclose(f);
    return 0;
}

static void frames(goome_ui *u, int n, int wait_ms)
{
    for (int i = 0; i < n; i++) {
        goome_event ev;
        while (goome_poll(K.g, &ev, 0) > 0) ui_event(&ev), goome_ui_feed(u, &ev);
        app_tick();
        logs_tick();
        app_frame(u);
        if (wait_ms) smm_sleep_ms(wait_ms);
    }
}

static int min_settle = 12;

static void settle(goome_ui *u)
{
    for (int i = 0; i < 400; i++) {
        frames(u, 1, 15);
        if (!mods_loading() && !K.animating && i > min_settle) break;
    }
    frames(u, 4, 10);
}

static void mouse(goome_ui *u, int type, double x, double y)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type;
    e.x = x;
    e.y = y;
    e.button = 1;
    goome_push(K.g, &e);
    frames(u, 1, 0);
}

static void click_at(goome_ui *u, double x, double y)
{
    mouse(u, GOOME_MOTION, x, y);
    mouse(u, GOOME_DOWN, x, y);
    mouse(u, GOOME_UP, x, y);
}

void shot_prepare(int step)
{
    if (step == 3) loadouts_select(0);
    if (step == 4) modpacks_select(0);
    if (step == 6) logs_select_session(1);
}

static int save_shot(goome_ui *u, const char *dir, const char *name)
{
    settle(u);
    goome_image img;
    if (goome_frame(K.g, &img) != GOOME_OK) return -1;
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    int rc = png_write(path, img.px, img.w, img.h, img.stride);
    printf("%s %s [%s]\n", rc == 0 ? "wrote" : "FAILED", path, smm_settings.game_name);
    return rc;
}

static void reset(goome_ui *u)
{
    tutorial_stop();
    chrome_reset();
    dlg_close_all();
    w_close_popups();
    mouse(u, GOOME_MOTION, 5, 5);
}

int shot_run(const char *dir)
{
    int err = 0;
    goome_opts o;
    memset(&o, 0, sizeof(o));
    o.mode = GOOME_MODE_OFFSCREEN;
    o.width = 1100;
    o.height = 720;
    o.scale = 1;
    goome *g = goome_open(&o, &err);
    if (!g) return 1;
    goome_ui *u = goome_ui_new(g);
    ui_init(g, u);
    goome_ui_theme(u)->bg = 0;
    goome_ui_theme(u)->anim_ms = 0;
    smm_settings.has_seen_tutorial = 1;
    smm_settings.app_version_seen = SMM_VERSION;
    app_startup(NULL, 1);
    dlg_close_all();
    char mk[1100];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
    if (system(mk) != 0) return 1;

    settle(u);
    mods_select(5);
    save_shot(u, dir, "01-mods");
    app_goto_tab(TAB_INSTALLED);
    save_shot(u, dir, "02-installed");
    app_goto_tab(TAB_LOADOUTS);
    shot_prepare(3);
    save_shot(u, dir, "03-loadouts");
    app_goto_tab(TAB_MODPACKS);
    shot_prepare(4);
    save_shot(u, dir, "04-modpacks");
    app_goto_tab(TAB_CONFIG);
    shot_prepare(5);
    save_shot(u, dir, "05-config");
    app_goto_tab(TAB_LOGS);
    shot_prepare(6);
    save_shot(u, dir, "06-logs");
    app_goto_tab(TAB_SETTINGS);
    save_shot(u, dir, "07-settings");
    app_goto_tab(TAB_MODS);
    dlg_game_picker_open();
    save_shot(u, dir, "08-game-picker");
    reset(u);
    dlg_steam_open();
    min_settle = 60;
    save_shot(u, dir, "09-steam-browser");
    min_settle = 12;
    reset(u);
    dlg_community_open();
    min_settle = 60;
    save_shot(u, dir, "10-community-picker");
    min_settle = 12;
    reset(u);
    app_goto_tab(TAB_MODPACKS);
    dlg_export_open();
    save_shot(u, dir, "11-modpack-export");
    reset(u);
    dlg_prompt(TAG_PACK_SAVE_NAME, "Save Current Setup", "Modpack name:", "My Setup 2026-11-15");
    save_shot(u, dir, "12-prompt");
    reset(u);
    dlg_update_open(3);
    save_shot(u, dir, "13-update");
    reset(u);
    app_goto_tab(TAB_MODS);
    smm_settings.has_seen_tutorial = 0;
    tutorial_start();
    save_shot(u, dir, "14-tutorial");
    reset(u);
    dlg_whatsnew_open();
    save_shot(u, dir, "15-whatsnew");
    reset(u);
    dlg_message(TAG_GENERIC, "Confirm", "Uninstall all 6 mods?", DB_YESNO, DI_WARN);
    save_shot(u, dir, "16-confirm");
    reset(u);
    smm_settings.has_seen_tutorial = 0;
    tutorial_start();
    tutorial_set_step(1);
    save_shot(u, dir, "17-tutorial-nav");
    reset(u);
    smm_settings.has_seen_tutorial = 0;
    tutorial_start();
    tutorial_set_step(5);
    save_shot(u, dir, "18-tutorial-install");
    reset(u);
    smm_settings.has_seen_tutorial = 1;
    app_goto_tab(TAB_MODS);
    settle(u);
    click_at(u, 70, 100);
    save_shot(u, dir, "19-mods-dropdown");
    reset(u);
    w_close_popups();
    click_at(u, 1030, 20);
    save_shot(u, dir, "20-quick-switch");
    reset(u);
    w_close_popups();
    app_goto_tab(TAB_SETTINGS);
    settle(u);
    for (int i = 0; i < 6; i++) {
        goome_event e;
        memset(&e, 0, sizeof(e));
        e.type = GOOME_SCROLL;
        e.x = 500;
        e.y = 400;
        e.dy = 10;
        goome_push(K.g, &e);
        frames(u, 2, 0);
    }
    save_shot(u, dir, "21-settings-scrolled");
    reset(u);
    smm_settings.game_picker_display = SMM_PICKER_ICONS;
    app_goto_tab(TAB_MODS);
    dlg_game_picker_open();
    save_shot(u, dir, "22-game-picker-icons");
    smm_settings.game_picker_display = SMM_PICKER_BOTH;
    reset(u);
    smm_map_del(&smm_settings.thunderstore_communities, smm_settings.game_name);
    smm_mf_set_active(1);
    app_goto_tab(TAB_MODS);
    mods_activate();
    settle(u);
    mods_select(1);
    save_shot(u, dir, "25-monkeforge-mods");
    R ir = mods_install_rect();
    click_at(u, ir.x + ir.w / 2, ir.y + ir.h / 2);
    for (int i = 0; i < 300 && (i < 20 || smm_queue_busy()); i++) frames(u, 1, 15);
    save_shot(u, dir, "25-monkeforge-installed-mod");
    app_goto_tab(TAB_INSTALLED);
    save_shot(u, dir, "25-monkeforge-installed");
    (void)click_at;
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
