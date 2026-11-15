#include "../goome.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/app.h"

void chrome_draw(void);
int shot_run(const char *dir);
void app_startup(const char *pack_path, int skip_picker);
void app_drop(const char *path);
void app_frame(goome_ui *u);

static Job update_job;
static volatile int update_latest;

static void check_update(void *p)
{
    (void)p;
    update_latest = smm_update_latest_version();
}

void app_drop(const char *path)
{
    size_t n = strlen(path);
    if (n > 4 && strcasecmp(path + n - 4, ".dll") == 0) {
        if (!app_has_game()) {
            app_status("No game selected, can't install.");
            return;
        }
        if (smm_mod_install_local(path) >= 0) {
            app_status("Installed dropped %s.", "dll file");
            app_refresh_installed();
        }
    } else if (n > 8 && strcasecmp(path + n - 8, ".smmpack") == 0) {
        modpacks_import_path(path);
    }
}

void app_startup(const char *pack_path, int skip_picker)
{
    int demo = smm_demo_enabled();
    if (!demo && !skip_picker) smm_settings.game_path[0] = 0;
    if (!demo && app_has_game()) {
        FILE *f = fopen(smm_settings.game_path, "rb");
        if (f) fclose(f);
        else smm_settings.game_path[0] = 0;
    }
    theme_refresh();
    if (!app_has_game()) dlg_game_picker_open();
    if (smm_settings.app_version_seen > 0 && smm_settings.app_version_seen < SMM_VERSION) dlg_whatsnew_open();
    smm_settings.app_version_seen = SMM_VERSION;
    smm_state_save();
    if (!smm_settings.has_seen_tutorial && !dlg_any()) tutorial_start();
    app_status("Loading mod catalog...");
    mods_init();
    if (smm_settings.auto_check_updates && !smm_demo_enabled()) job_start(&update_job, check_update, NULL);
    if (pack_path) {
        app_goto_tab(TAB_MODPACKS);
        modpacks_import_path(pack_path);
    }
}

static void poll_update(void)
{
    if (job_finished(&update_job) && update_latest > SMM_VERSION) dlg_update_open(update_latest);
}

void app_frame(goome_ui *u)
{
    goome_canvas *c = goome_ui_begin(u, 0);
    if (!c) return;
    goome_monitor m;
    goome_monitors(K.g, &m, 1);
    if (getenv("SMM_DBG")) fprintf(stderr, "mon %d %d %d %d scale %f\n", m.area.x, m.area.y, m.area.w, m.area.h, m.scale);
    if (goome_ui_app(u, "smm", GOOME_APP_NO_PAD)) {
        ui_begin_frame(goome_ui_canvas(u), m.area.w, m.area.h);
        chrome_draw();
    }
    goome_ui_app_end(u);
    goome_ui_end(u);
    ui_end_frame();
}

static int run_window(const char *pack, int skip)
{
    int err = 0;
    goome_opts o;
    memset(&o, 0, sizeof(o));
    o.title = "Sev's Mod Manager";
    o.mode = GOOME_MODE_WINDOW;
    o.width = 1100;
    o.height = 720;
    o.min_width = 860;
    o.min_height = 560;
    o.window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_BORDERLESS | GOOME_WINDOW_TRANSPARENT | GOOME_WINDOW_CENTER;
    goome *g = goome_open(&o, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    ui_init(g, u);
    K.windowed = 1;
    goome_ui_auto_grab(u, 0);
    goome_ui_theme(u)->bg = 0;
    goome_ui_theme(u)->anim_ms = 0;
    app_startup(pack, skip);
    long last_draw = 0;
    while (!K.quit) {
        goome_event ev;
        int got = 0;
        int rc = goome_poll(g, &ev, K.animating ? 8 : 40);
        while (rc > 0) {
            got = 1;
            if (ev.type == GOOME_CLOSE || ev.type == GOOME_QUIT) K.quit = 1;
            else if (ev.type == GOOME_DROP) app_drop(goome_drop_path(g));
            ui_event(&ev);
            goome_ui_feed(u, &ev);
            rc = goome_poll(g, &ev, 0);
        }
        K.now = goome_now_ms();
        if (!got && !K.animating && K.now - last_draw < 150) continue;
        last_draw = K.now;
        app_tick();
        logs_tick();
        poll_update();
        app_frame(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}

int main(int argc, char **argv)
{
    const char *pack = NULL;
    int skip = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--skip-picker") == 0) skip = 1;
        else if (!pack) pack = argv[i];
    }
    smm_state_init();
    smm_state_load();
    const char *demo = getenv("SMM_DEMO");
    if (demo && *demo && strcmp(demo, "0") != 0 && smm_demo_init(getenv("SMM_FIXTURES") ? getenv("SMM_FIXTURES") : "fixtures") < 0) fprintf(stderr, "demo mode failed: %s\n", smm_error());
    const char *shot = getenv("SMM_SHOT");
    if (shot && *shot) return shot_run(shot);
    return run_window(pack, skip);
}
