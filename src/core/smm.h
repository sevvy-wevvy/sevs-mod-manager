#ifndef SMM_H
#define SMM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMM_VERSION 2
#define SMM_VERSION_TEXT "2.0"
#define SMM_PATH 1024
#define SMM_USER_AGENT "SevsModManager/1.0"
#define SMM_MAX_LOADOUTS 20

typedef struct {
    char **v;
    int n, cap;
} SmmStrs;

typedef struct {
    char **k, **v;
    int n, cap;
} SmmMap;

typedef struct {
    char *p;
    size_t n, cap;
} SmmBuf;

typedef struct SmmProgress SmmProgress;

enum { SMM_SRC_API = 0, SMM_SRC_THUNDERSTORE = 1, SMM_SRC_MONKEFORGE = 2 };
enum { SMM_LOADER_BEPINEX = 0, SMM_LOADER_MELON = 1 };
enum { SMM_THEME_BLACK = 0, SMM_THEME_WHITE = 1, SMM_THEME_CUSTOM = 2, SMM_THEME_SYNCED_SB = 3, SMM_THEME_R2MODMAN = 4 };
enum { SMM_LAYOUT_SMM = 0, SMM_LAYOUT_R2MODMAN = 1, SMM_LAYOUT_MONKE = 2 };
enum { SMM_PICKER_BOTH = 0, SMM_PICKER_TEXT = 1, SMM_PICKER_ICONS = 2 };

typedef struct {
    int id;
    char name[128];
    char dll_name[160];
    char repo_url[256];
    char description[512];
    char author[96];
    char download_url[512];
    char icon_url[512];
    char version[48];
    int upvotes;
    int verified;
    int featured;
    int source;
    SmmStrs deps;
} SmmMod;

typedef struct {
    SmmMod *v;
    int n, cap;
} SmmMods;

typedef struct {
    char name[160];
    char path[SMM_PATH];
    int enabled;
    int is_folder;
    long long size_bytes;
    char known_version[48];
    char latest_version[48];
} SmmInstalled;

typedef struct {
    SmmInstalled *v;
    int n, cap;
} SmmInstalledList;

typedef struct {
    char name[128];
    char identifier[96];
} SmmCommunity;

typedef struct {
    SmmCommunity *v;
    int n, cap;
} SmmCommunities;

typedef struct {
    int number;
    char name[128];
    SmmStrs enabled;
    SmmStrs disabled;
    SmmStrs missing;
} SmmLoadout;

typedef struct {
    SmmLoadout *v;
    int n, cap;
} SmmLoadouts;

typedef struct {
    char name[128];
    char author[128];
    char game_slug[96];
    char created_utc[40];
    SmmStrs mods;
    int loader_kind;
} SmmManifest;

typedef struct {
    char path[SMM_PATH];
    SmmManifest manifest;
} SmmPack;

typedef struct {
    SmmPack *v;
    int n, cap;
} SmmPacks;

typedef struct {
    char name[16];
    char prefix[16];
    char abs_path[SMM_PATH];
} SmmPackRoot;

typedef struct {
    char name[32];
    char slug[32];
    const char *default_paths[4];
    int n_paths;
    const char *default_community;
    int steam_app_id;
} SmmPreset;

typedef struct {
    char game_path[SMM_PATH];
    char game_name[128];
    int theme;
    char custom_background[16];
    char custom_accent[16];
    int auto_check_updates;
    int app_version_seen;
    int layout;
    int game_picker_display;
    SmmMap known_game_paths;
    SmmMap custom_games;
    SmmMap thunderstore_communities;
    SmmStrs hidden_presets;
    SmmStrs manual_only_games;
    SmmStrs monkeforge_games;
    SmmMap steam_exe_paths;
    SmmMap loader_overrides;
    SmmStrs disable_starting_indicator;
    int has_seen_tutorial;
    int disable_discord_rpc;
    char registered_exe[SMM_PATH];
} SmmSettings;

typedef struct {
    char key[128];
    char value[512];
    SmmStrs description;
    char setting_type[40];
    int has_default;
    char default_value[256];
    int has_range;
    double range_min, range_max;
    SmmStrs acceptable;
} SmmConfigEntry;

typedef struct {
    char name[160];
    SmmConfigEntry *v;
    int n, cap;
} SmmConfigSection;

typedef struct {
    char path[SMM_PATH];
    char display_name[160];
    SmmStrs header;
    SmmConfigSection *sections;
    int n_sections, cap_sections;
} SmmConfigFile;

typedef struct {
    char display_name[160];
    char path[SMM_PATH];
} SmmConfigRef;

typedef struct {
    SmmConfigRef *v;
    int n, cap;
} SmmConfigRefs;

typedef struct {
    char path[SMM_PATH];
    long long timestamp;
    char display_name[64];
} SmmLogSession;

typedef struct {
    SmmLogSession *v;
    int n, cap;
} SmmLogSessions;

typedef struct {
    char mod_name[160];
    char reason[400];
} SmmCrashSuspect;

typedef struct {
    int appid;
    char name[128];
} SmmSteamGame;

typedef struct {
    SmmSteamGame *v;
    int n, cap;
} SmmSteamGames;

typedef struct {
    unsigned char r, g, b;
} SmmRgb;

typedef struct {
    SmmRgb background, surface, surface_alt, border, text, sub_text, accent, accent_text;
} SmmTheme;

enum { SMM_OP_QUEUED = 0, SMM_OP_RUNNING = 1, SMM_OP_DONE = 2, SMM_OP_FAILED = 3 };

typedef struct {
    int id;
    char name[160];
    int status;
    int percent;
    char status_text[256];
    char error[256];
    long long queued_at;
    long long finished_at;
} SmmOp;

typedef int (*SmmWorkFn)(SmmProgress *p, void *arg);
typedef void (*SmmDoneFn)(int rc, void *arg);
typedef void (*SmmThreadFn)(void *arg);

void smm_strs_add(SmmStrs *s, const char *str);
void smm_strs_free(SmmStrs *s);
int smm_strs_find(const SmmStrs *s, const char *str, int nocase);
int smm_strs_remove(SmmStrs *s, const char *str);
void smm_strs_copy(SmmStrs *dst, const SmmStrs *src);
const char *smm_map_get(const SmmMap *m, const char *key);
void smm_map_set(SmmMap *m, const char *key, const char *val);
int smm_map_del(SmmMap *m, const char *key);
void smm_map_free(SmmMap *m);
void smm_buf_append(SmmBuf *b, const void *data, size_t n);
void smm_buf_free(SmmBuf *b);

const char *smm_error(void);
void smm_set_error(const char *fmt, ...);

void smm_progress(SmmProgress *p, int percent, const char *fmt, ...);

void smm_mods_free(SmmMods *m);
void smm_mods_add(SmmMods *m, const SmmMod *mod);
void smm_installed_free(SmmInstalledList *l);
int smm_installed_update_available(const SmmInstalled *m);
void smm_loadouts_free(SmmLoadouts *l);
void smm_loadout_display_name(const SmmLoadout *l, char *out, size_t n);
void smm_packs_free(SmmPacks *p);
void smm_pack_display_name(const SmmPack *p, char *out, size_t n);
void smm_communities_free(SmmCommunities *c);
void smm_config_file_free(SmmConfigFile *f);
void smm_config_refs_free(SmmConfigRefs *r);
void smm_log_sessions_free(SmmLogSessions *l);
void smm_steam_games_free(SmmSteamGames *g);

extern SmmSettings smm_settings;

int smm_preset_count(void);
const SmmPreset *smm_preset_at(int i);
const SmmPreset *smm_preset_find(const char *name);
const SmmPreset *smm_current_preset(void);
void smm_current_slug(char *out, size_t n);
void smm_derive_slug(const char *name, char *out, size_t n);

void smm_state_init(void);
void smm_state_load(void);
int smm_state_save(void);
void smm_state_path(char *out, size_t n);
void smm_app_data_dir(char *out, size_t n);

int smm_game_dir(char *out, size_t n);
int smm_sb_dir(char *out, size_t n);
int smm_bepinex_dir(char *out, size_t n);
int smm_plugins_dir(char *out, size_t n);
int smm_melon_mods_dir(char *out, size_t n);
int smm_mods_install_dir(char *out, size_t n);
int smm_packs_dir(char *out, size_t n);
int smm_has_sb_folder(void);
int smm_bepinex_present(void);
void smm_ensure_sb_folder(void);
int smm_detect_loader(void);
const char *smm_loader_name(int kind);
int smm_loader_installed(int kind, const char *game_dir);
int smm_loader_install(int kind, const char *game_dir, SmmProgress *p);
int smm_loader_uninstall(int kind, const char *game_dir);
int smm_loader_mods_enabled(int kind, const char *game_dir);
int smm_loader_toggle_mods(int kind, const char *game_dir);
int smm_exe_is_x64(const char *exe_path);
int smm_autodetect_game(char *out, size_t n);

int smm_dc_load_settings(void);
int smm_dc_get_bool(const char *key, int def);
void smm_dc_set_bool(const char *key, int val);
double smm_dc_get_float(const char *key, double def);
void smm_dc_set_float(const char *key, double val);
const char *smm_dc_get_string(const char *key, const char *def);
void smm_dc_set_string(const char *key, const char *val);
int smm_token_load(char *out, size_t n);
int smm_token_save(const char *token);
void smm_token_delete(void);
void smm_mod_version_get(const char *key, char *out, size_t n);
void smm_mod_version_set(const char *key, const char *version);
void smm_mod_version_delete(const char *key);
int smm_loadouts_load(SmmLoadouts *out);
int smm_loadouts_save(const SmmLoadouts *l);

int smm_installed_scan(SmmInstalledList *out);
int smm_mod_install_dll(const char *url, const char *dll_name, SmmProgress *p);
int smm_mod_install_thunderstore(const char *url, const char *folder_name, SmmProgress *p);
int smm_mod_install_local(const char *dll_path);
int smm_mod_uninstall(const char *name);
int smm_mod_enable(const char *name);
int smm_mod_disable(const char *name);
int smm_mod_install(const SmmMod *m, SmmProgress *p);
void smm_mod_key(const SmmMod *m, char *out, size_t n);
int smm_mod_download_url(const SmmMod *m, char *out, size_t n);
void smm_melon_key(const char *full_name, char *out, size_t n);
int smm_mod_is_installed(const SmmMod *m, const SmmInstalledList *inst);
int smm_mod_is_disabled(const SmmMod *m);
int smm_mod_files_present(const SmmMod *m);
void smm_apply_latest_versions(SmmInstalledList *inst, const SmmMods *latest);
void smm_strip_dep_version(const char *dep, char *out, size_t n);
int smm_missing_deps(const SmmMod *m, const SmmMods *all, const SmmInstalledList *inst, SmmMods *out);
int smm_dependents_installed(const SmmMod *m, const SmmMods *all, const SmmInstalledList *inst, SmmStrs *out);
int smm_find_conflicts(const SmmInstalledList *inst, SmmStrs *out);
void smm_sort_mods(SmmMods *m);
void smm_filter_mods(const SmmMods *src, const char *tab, SmmMods *out);

int smm_loadout_capture(SmmLoadouts *l);
int smm_loadout_apply(SmmLoadouts *l, int number);
int smm_loadout_delete(SmmLoadouts *l, int number);
int smm_loadout_rename(SmmLoadouts *l, int number, const char *name);

int smm_pack_roots(SmmPackRoot *out, int max);
void smm_pack_default_roots(SmmStrs *out);
int smm_packs_list(SmmPacks *out);
int smm_pack_entries(const char *pack_path, SmmStrs *out);
int smm_pack_read_manifest(const char *pack_path, SmmManifest *out);
void smm_manifest_free(SmmManifest *m);
void smm_current_mod_names(SmmStrs *out);
int smm_setup_matches_saved_pack(void);
int smm_pack_blank_setup(SmmProgress *p);
int smm_pack_save(const char *name, const char *author, const SmmStrs *only_files, char *dest_out, size_t n, SmmProgress *p);
int smm_pack_apply(const char *pack_path, SmmProgress *p);
int smm_pack_delete(const char *pack_path);
int smm_pack_export(const char *pack_path, const char *dest);
int smm_pack_import(const char *src, char *dest_out, size_t n);
int smm_pack_files_for_export(SmmStrs *abs_paths);

int smm_config_list(SmmConfigRefs *out);
int smm_config_parse(const char *path, SmmConfigFile *out);
int smm_config_save(const SmmConfigFile *f);

int smm_log_path(char *out, size_t n);
int smm_log_read_lines(const char *path, SmmStrs *out);
int smm_log_read_from(const char *path, long long *offset, SmmStrs *out);
int smm_log_history_list(SmmLogSessions *out);
int smm_log_history_archive(long long started_at, const SmmStrs *lines);
int smm_log_history_dir(char *out, size_t n);
int smm_crash_analyze(const SmmStrs *lines, SmmCrashSuspect *out);
int smm_bug_report_export(const char *dest_zip, const SmmStrs *lines);
int smm_log_line_level(const char *line);

int smm_http_get(const char *url, SmmBuf *out);
int smm_http_post_form(const char *url, const SmmMap *fields, SmmBuf *out);
int smm_http_download(const char *url, const char *dest, SmmProgress *p, const char *label, int pct_from, int pct_to);
int smm_http_url_exists(const char *url);
const char *smm_http_backend(void);

int smm_sb_register_game(const char *slug);
int smm_sb_list_mods(const char *tab, int page, int game_id, SmmMods *out);
int smm_sb_user_upvoted(int mod_id);
int smm_sb_toggle_upvote(int mod_id, int *upvoted);
int smm_sb_validate_token(const char *token, char *username, size_t n);
int smm_sb_login_code(char *out, size_t n);
int smm_sb_poll_login(const char *code, char *token_out, size_t n);
int smm_sb_latest_tag(const char *owner, const char *repo, char *out, size_t n);
void smm_sb_set_token(const char *token);
const char *smm_sb_token(void);
int smm_ts_list_packages(const char *community, SmmMods *out);
int smm_ts_list_communities(SmmCommunities *out);
int smm_ts_guess_community(const char *game_name, char *out, size_t n);
int smm_update_latest_version(void);

int smm_mf_supported(void);
int smm_mf_active(void);
void smm_mf_set_active(int on);
int smm_mf_list(SmmMods *out);
int smm_mf_install(const SmmMod *m, SmmProgress *p);
int smm_mf_is_installed(const SmmMod *m);
int smm_mf_is_disabled(const SmmMod *m);

int smm_steam_list_installed(SmmSteamGames *out);
int smm_steam_find_game_path(int appid, const char *exe_name, char *out, size_t n);
int smm_steam_exe_candidates(int appid, SmmStrs *out);
int smm_steam_root(char *out, size_t n);
int smm_steam_compat_prefix(int appid, char *out, size_t n);

int smm_game_launch(void);
int smm_game_running(void);
int smm_game_stop(void);
const char *smm_loader_dll_override(int kind);
void smm_launch_option_hint(int kind, char *out, size_t n);
int smm_open_path(const char *path);
int smm_open_url(const char *url);
int smm_is_linux_proton(void);

void smm_theme_apply(int mode, const char *custom_bg, const char *custom_accent, const char *sb_accent_hex, SmmTheme *out);
void smm_theme_current(SmmTheme *out);
SmmRgb smm_rgb_highlight(const SmmTheme *t);
SmmRgb smm_rgb_lighten(SmmRgb c, int amount);
SmmRgb smm_rgb_darken(SmmRgb c, int amount);
int smm_rgb_parse_hex(const char *hex, SmmRgb *out);
void smm_rgb_to_hex(SmmRgb c, char *out);
unsigned smm_rgb_pack(SmmRgb c);
int smm_rgb_is_dark(SmmRgb c);

int smm_queue_run(const char *name, SmmWorkFn fn, void *arg, SmmDoneFn done);
int smm_queue_snapshot(SmmOp *out, int max);
unsigned smm_queue_changed(void);
int smm_queue_busy(void);
int smm_queue_current(SmmOp *out);
int smm_queue_install_mod(const SmmMod *m);
int smm_queue_install_loader(int kind);
int smm_queue_apply_pack(const char *pack_path);
int smm_queue_blank_setup(void);
int smm_queue_save_pack(const char *name, const char *author, const SmmStrs *only_files);
int smm_queue_update_mods(const SmmMods *latest, const SmmInstalledList *inst);
int smm_thread_start(SmmThreadFn fn, void *arg);
void smm_sleep_ms(int ms);
long long smm_now(void);

int smm_demo_enabled(void);
int smm_demo_init(const char *fixtures_dir);
const char *smm_demo_dir(void);
const char *smm_demo_fixtures(void);

#ifdef __cplusplus
}
#endif

#endif
