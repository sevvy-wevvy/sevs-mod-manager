#ifndef SMM_APP_H
#define SMM_APP_H

#include <stdarg.h>
#include "ui.h"
#include "../core/smm.h"

enum { TAB_MODS, TAB_INSTALLED, TAB_LOGS, TAB_LOADOUTS, TAB_MODPACKS, TAB_CONFIG, TAB_SETTINGS, TAB_COUNT };

enum {
    TAG_NONE, TAG_UNVERIFIED, TAG_LOADER_NEEDED, TAG_UNINSTALL_DEPS, TAG_UNINSTALL, TAG_UNINSTALL_ALL, TAG_CONFLICTS,
    TAG_LOADOUT_DELETE, TAG_LOADOUT_ERR, TAG_PACK_APPLY, TAG_PACK_DELETE, TAG_PACK_BLANK, TAG_PACK_SAVE_FIRST,
    TAG_PACK_IMPORT, TAG_PACK_SAVE_NAME, TAG_PACK_LOADER, TAG_PACK_WRONG_GAME, TAG_PACK_INVALID, TAG_PACK_LOCATE,
    TAG_CFG_RESET, TAG_CFG_RESET_INFO, TAG_LOADER_SWITCH, TAG_LOADER_BACKUP, TAG_BUG_SAVED, TAG_BUG_FAIL,
    TAG_GAME_CLOSED, TAG_TS_FAIL, TAG_TUTORIAL_ERR, TAG_UPDATE, TAG_WHATSNEW, TAG_EXPORT_EMPTY, TAG_GENERIC,
    TAG_LOADOUT_NAME, TAG_COLOR_BG, TAG_COLOR_ACCENT
};

enum { DB_OK, DB_YESNO, DB_YESNOCANCEL };
enum { DR_NONE, DR_OK, DR_YES, DR_NO, DR_CANCEL };
enum { DI_INFO, DI_WARN, DI_ERROR, DI_QUESTION };

typedef struct {
    volatile int state;
    void (*fn)(void *);
    void *arg;
} Job;

int job_start(Job *j, void (*fn)(void *), void *arg);
int job_running(const Job *j);
int job_finished(Job *j);

void app_status(const char *fmt, ...);
const char *app_status_text(void);
void app_progress(int pct);
int app_progress_pct(void);
int app_has_game(void);
void app_goto_tab(int tab);
int app_tab(void);
void app_game_changed(void);
void app_refresh_installed(void);
SmmInstalledList *app_installed(void);
void app_tick(void);

void dlg_message(int tag, const char *title, const char *msg, int buttons, int icon);
int dlg_result(int tag);
void dlg_prompt(int tag, const char *title, const char *label, const char *def);
int dlg_prompt_result(int tag, char *out, size_t n);
int dlg_any(void);
void dlg_draw(void);
void dlg_close_all(void);

void dlg_game_picker_open(void);
void dlg_steam_open(void);
void dlg_community_open(void);
int dlg_community_result(char *slug, size_t n, char *name, size_t nn);
void dlg_export_open(void);
void dlg_update_open(int version);
void dlg_whatsnew_open(void);

int dlg_frame(const char *title, double w, double h, R *content, int closable);
void dlg_scrim(void);

void tutorial_start(void);
void tutorial_pre(void);
R tab_rect(int i);
R play_rect(void);
R installed_list_rect(void);
void tutorial_draw(void);
int tutorial_active(void);
int tutorial_mode(void);
R tutorial_target(int which);

void panel_mods(R r);
void mods_activate(void);
void mods_init(void);
void mods_tutorial_show(int on);
R mods_install_rect(void);
void mods_set_status(const char *s);
void mods_poll(void);
void panel_installed(R r);
void installed_activate(void);
R installed_toggle_rect(void);
void installed_tutorial(int on);
int installed_tutorial_enabled(void);
int installed_tutorial_deleted(void);
int mods_tutorial_installed(void);
int mods_loading(void);
void mods_select(int idx);
const SmmMods *mods_latest(void);
void tutorial_stop(void);
void tutorial_set_step(int s);
R installed_delete_rect(void);
void panel_loadouts(R r);
void loadouts_activate(void);
void panel_modpacks(R r);
void modpacks_activate(void);
void modpacks_import_path(const char *path);
void panel_config(R r);
void config_activate(void);
void panel_logs(R r);
void logs_activate(void);
void logs_tick(void);
void logs_game_state(int running);
void panel_settings(R r);
void settings_activate(void);

int file_pick_open(const char *title, const char *filter_desc, const char *filter_ext, char *out, size_t n);
int file_pick_save(const char *title, const char *default_name, const char *filter_ext, char *out, size_t n);

int icon_cache_get(const char *url, const goome_bitmap **out);
int icon_cache_draw(const char *url, R r, double radius);

void theme_refresh(void);
void play_state(int *starting, int *running);
void draw_game_dot(R r, const char *name);

int shot_run(const char *dir);

#endif
