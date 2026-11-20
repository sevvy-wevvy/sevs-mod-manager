#include <stdio.h>
#include <string.h>
#include "app.h"

enum { MODE_STATUS, MODE_AUTO, MODE_MANUAL, MODE_PLAY };

typedef struct {
    int mode;
    int appid;
    int kind;
    char proxy[16];
    int state;
    int did_configure;
    int result;
    int accounts, updated;
    char err[256];
} Run;

static Job job;
static Run run;
static int cur_state = -1;
static int cur_appid;
static int cur_kind;
static int cur_installed;
static int dirty = 1;
static char cur_path[SMM_PATH];
static int tried[16];
static int n_tried;
static int told_steam;
static void (*pending_launch)(void);

static int was_tried(int appid)
{
    for (int i = 0; i < n_tried; i++)
        if (tried[i] == appid) return 1;
    return 0;
}

static void mark_tried(int appid)
{
    if (n_tried < 16) tried[n_tried++] = appid;
}

static void work(void *arg)
{
    Run *r = arg;
    r->state = smm_proton_status(r->appid, r->proxy);
    int go = r->mode == MODE_MANUAL || (r->mode != MODE_STATUS && r->state == SMM_PROTON_NEEDS_CONFIG);
    if (!go) return;
    r->did_configure = 1;
    r->result = smm_proton_configure(r->appid, r->proxy, &r->accounts, &r->updated, r->err, sizeof(r->err));
    if (r->result == SMM_PROTON_OK || r->result == SMM_PROTON_ALREADY) r->state = SMM_PROTON_CONFIGURED;
}

static void say_steam_running(int first_dialog)
{
    app_status("Close Steam, then press Configure to Proton in Settings (or just press Play again after closing it).");
    if (first_dialog && !told_steam) {
        told_steam = 1;
        dlg_message(TAG_GENERIC, "Close Steam First",
                    "Steam is running, and it rewrites its settings when it closes, so the Proton launch option can't be saved right now.\n\nClose Steam completely, then press Configure to Proton in Settings (or just press Play again after closing it).",
                    DB_OK, DI_WARN);
    }
}

static void report(const Run *r)
{
    const char *loader = smm_loader_name(r->kind);
    char t[600];
    if (r->mode == MODE_MANUAL) {
        switch (r->result) {
        case SMM_PROTON_OK:
            snprintf(t, sizeof(t), "Steam's launch options now load %s under Proton (%d Steam account%s updated).\n\nPress Play, or start the game from Steam. The old settings file was saved next to it as localconfig.vdf.smm-backup.", loader, r->updated, r->updated == 1 ? "" : "s");
            dlg_message(TAG_GENERIC, "Configured to Proton", t, DB_OK, DI_INFO);
            app_status("Proton launch options set for %s.", loader);
            break;
        case SMM_PROTON_ALREADY:
            snprintf(t, sizeof(t), "Steam already has the launch option %s needs for this game. Nothing was changed.", loader);
            dlg_message(TAG_GENERIC, "Already Configured", t, DB_OK, DI_INFO);
            app_status("Proton launch options were already set.");
            break;
        case SMM_PROTON_STEAM_RUNNING:
            dlg_message(TAG_GENERIC, "Close Steam First",
                        "Steam is running, and it rewrites its settings when it closes, so the change would be lost.\n\nClose Steam completely (right click its tray icon and choose Exit), then press Configure to Proton again.",
                        DB_OK, DI_WARN);
            app_status("Close Steam completely, then press Configure to Proton again.");
            break;
        default:
            snprintf(t, sizeof(t), "%s", r->err[0] ? r->err : "Couldn't update Steam's launch options.");
            dlg_message(TAG_GENERIC, "Couldn't Configure", t, DB_OK, r->result == SMM_PROTON_PARSE_ERROR ? DI_ERROR : DI_WARN);
            app_status("%s", t);
        }
        return;
    }
    if (!r->did_configure) return;
    switch (r->result) {
    case SMM_PROTON_OK: app_status("Set up Steam's launch options so %s loads under Proton.", loader); break;
    case SMM_PROTON_STEAM_RUNNING: say_steam_running(1); break;
    case SMM_PROTON_NO_CONFIG: app_status("Proton: launch the game once through Steam, then press Configure to Proton in Settings."); break;
    case SMM_PROTON_PARSE_ERROR: app_status("Proton: %s", r->err[0] ? r->err : "couldn't update Steam's launch options."); break;
    default: break;
    }
}

static void start_run(int mode)
{
    memset(&run, 0, sizeof(run));
    run.mode = mode;
    run.appid = cur_appid;
    run.kind = cur_kind;
    snprintf(run.proxy, sizeof(run.proxy), "%s", smm_loader_proxy_dll(cur_kind));
    if (smm_demo_enabled()) {
        work(&run);
        cur_state = run.state;
        report(&run);
        return;
    }
    job_start(&job, work, &run);
}

static void evaluate(int force_auto)
{
    dirty = 0;
    snprintf(cur_path, sizeof(cur_path), "%s", smm_settings.game_path);
    cur_appid = smm_proton_game_appid();
    if (cur_appid <= 0) {
        cur_state = -1;
        return;
    }
    cur_kind = smm_detect_loader();
    char dir[SMM_PATH];
    cur_installed = smm_game_dir(dir, sizeof(dir)) > 0 && smm_loader_installed(cur_kind, dir);
    int auto_ok = !smm_demo_enabled() && !smm_settings.disable_auto_proton && cur_installed && (force_auto || !was_tried(cur_appid));
    if (auto_ok) mark_tried(cur_appid);
    start_run(auto_ok ? MODE_AUTO : MODE_STATUS);
}

void proton_refresh(void)
{
    dirty = 1;
}

static void launch_pending(void)
{
    void (*fn)(void) = pending_launch;
    pending_launch = NULL;
    if (fn) fn();
}

void proton_tick(void)
{
    if (job_finished(&job)) {
        cur_state = run.state;
        report(&run);
        if (pending_launch) {
            if (run.mode == MODE_PLAY) launch_pending();
            else if (cur_appid > 0 && cur_installed && !smm_settings.disable_auto_proton) start_run(MODE_PLAY);
            else launch_pending();
        }
    }
    if (job_running(&job)) return;
    if (strcmp(cur_path, smm_settings.game_path) != 0) dirty = 1;
    if (!smm_settings.game_path[0]) {
        cur_appid = 0;
        cur_state = -1;
        dirty = 0;
        cur_path[0] = 0;
        return;
    }
    if (dirty) evaluate(0);
}

int proton_before_play(void (*launch)(void))
{
    if (pending_launch) return 1;
    if (smm_demo_enabled() || smm_settings.disable_auto_proton || cur_appid <= 0 || !cur_installed) return 0;
    pending_launch = launch;
    if (job_running(&job)) return 1;
    app_status("Checking Steam's launch options...");
    start_run(MODE_PLAY);
    return 1;
}

int proton_state(void)
{
    return cur_state;
}

int proton_loader_kind(void)
{
    return cur_kind;
}

int proton_busy(void)
{
    return job_running(&job);
}

void proton_configure_now(void)
{
    if (job_running(&job) || cur_appid <= 0) return;
    app_status("Updating Steam's launch options...");
    start_run(MODE_MANUAL);
}
