#include "internal.h"
#include <stdlib.h>

SmmSettings smm_settings;

static char g_root[SMM_PATH];

static const SmmPreset presets[] = {
    {"Gorilla Tag", "gorillatag",
     {"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Gorilla Tag\\Gorilla Tag.exe",
      "C:\\Program Files\\Steam\\steamapps\\common\\Gorilla Tag\\Gorilla Tag.exe",
      "C:\\Program Files\\Oculus\\Software\\Software\\another-axiom-gorilla-tag\\Gorilla Tag.exe"},
     3, NULL, 1533390},
    {"ULTRAKILL", "ultrakill",
     {"C:\\Program Files (x86)\\Steam\\steamapps\\common\\ULTRAKILL\\ULTRAKILL.exe",
      "C:\\Program Files\\Steam\\steamapps\\common\\ULTRAKILL\\ULTRAKILL.exe"},
     2, "ultrakill", 1229490},
    {"REPO", "repo",
     {"C:\\Program Files (x86)\\Steam\\steamapps\\common\\REPO\\REPO.exe",
      "C:\\Program Files\\Steam\\steamapps\\common\\REPO\\REPO.exe"},
     2, "repo", 3241660},
    {"White Knuckle", "whiteknuckle",
     {"C:\\Program Files (x86)\\Steam\\steamapps\\common\\White Knuckle\\White Knuckle.exe",
      "C:\\Program Files\\Steam\\steamapps\\common\\White Knuckle\\White Knuckle.exe"},
     2, "white-knuckle", 3195790},
    {"BONELAB", "bonelab",
     {"C:\\Program Files (x86)\\Steam\\steamapps\\common\\BONELAB\\BONELAB_Steam_Windows64.exe",
      "C:\\Program Files\\Steam\\steamapps\\common\\BONELAB\\BONELAB_Steam_Windows64.exe"},
     2, "bonelab", 1592190},
    {"Custom", "custom", {NULL}, 0, NULL, 0},
};

int smm_preset_count(void)
{
    return (int)(sizeof presets / sizeof presets[0]);
}

const SmmPreset *smm_preset_at(int i)
{
    return i >= 0 && i < smm_preset_count() ? &presets[i] : NULL;
}

const SmmPreset *smm_preset_find(const char *name)
{
    for (int i = 0; i < smm_preset_count(); i++)
        if (!strcmp(presets[i].name, name)) return &presets[i];
    return NULL;
}

const SmmPreset *smm_current_preset(void)
{
    const SmmPreset *p = smm_preset_find(smm_settings.game_name);
    return p ? p : &presets[smm_preset_count() - 1];
}

void smm_derive_slug(const char *name, char *out, size_t n)
{
    size_t k = 0;
    for (const char *p = name; *p && k + 1 < n; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'A' && c <= 'Z') out[k++] = (char)(c + 32);
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80) out[k++] = (char)c;
    }
    out[k] = 0;
}

void smm_current_slug(char *out, size_t n)
{
    const SmmPreset *p = smm_current_preset();
    if (strcmp(p->name, "Custom") != 0) {
        smm_copy(out, n, p->slug);
    } else if (smm_settings.game_path[0]) {
        char stem[SMM_PATH];
        smm_stem(smm_settings.game_path, stem, sizeof stem);
        smm_derive_slug(stem, out, n);
    } else {
        smm_derive_slug(smm_settings.game_name, out, n);
    }
}

void smm_state_defaults(SmmSettings *s)
{
    memset(s, 0, sizeof *s);
    smm_copy(s->game_name, sizeof s->game_name, "Gorilla Tag");
    smm_copy(s->custom_background, sizeof s->custom_background, "#141414");
    smm_copy(s->custom_accent, sizeof s->custom_accent, "#7C3AED");
    s->auto_check_updates = 1;
}

void smm_settings_free(SmmSettings *s)
{
    smm_map_free(&s->known_game_paths);
    smm_map_free(&s->custom_games);
    smm_map_free(&s->thunderstore_communities);
    smm_strs_free(&s->hidden_presets);
    smm_strs_free(&s->manual_only_games);
    smm_strs_free(&s->monkeforge_games);
    smm_map_free(&s->steam_exe_paths);
    smm_map_free(&s->loader_overrides);
    smm_strs_free(&s->disable_starting_indicator);
}

void smm_state_init(void)
{
    smm_settings_free(&smm_settings);
    smm_state_defaults(&smm_settings);
}

void smm_set_root_override(const char *dir)
{
    smm_copy(g_root, sizeof g_root, dir ? dir : "");
}

const char *smm_root_override(void)
{
    return g_root;
}

void smm_app_data_dir(char *out, size_t n)
{
    if (g_root[0]) {
        smm_join(out, n, g_root, "appdata");
        return;
    }
    char base[SMM_PATH];
    smm_os_config_base(base, sizeof base);
    smm_join(out, n, base, "SevsModManager");
}

void smm_state_path(char *out, size_t n)
{
    char d[SMM_PATH];
    smm_app_data_dir(d, sizeof d);
    smm_join(out, n, d, "state.json");
}

static void load_map(SmmMap *m, const SmmJson *o)
{
    if (!o || o->type != SMM_J_OBJ) return;
    for (int i = 0; i < o->n; i++) {
        const SmmJson *k = &o->kids[i];
        if (k->type == SMM_J_STR) smm_map_set(m, k->key, k->str);
        else if (k->type == SMM_J_NUM) {
            char t[32];
            smm_fmt(t, sizeof t, "%d", (int)k->num);
            smm_map_set(m, k->key, t);
        }
    }
}

static void load_strs(SmmStrs *s, const SmmJson *a)
{
    if (!a || a->type != SMM_J_ARR) return;
    for (int i = 0; i < a->n; i++)
        if (a->kids[i].type == SMM_J_STR) smm_strs_add(s, a->kids[i].str);
}

void smm_state_load(void)
{
    smm_state_init();
    char path[SMM_PATH];
    smm_state_path(path, sizeof path);
    SmmBuf b = {0};
    if (smm_file_read(path, &b) != 0) return;
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    if (!j || j->type != SMM_J_OBJ) {
        smm_json_free(j);
        return;
    }
    SmmSettings *s = &smm_settings;
    smm_copy(s->game_path, sizeof s->game_path, smm_json_str(j, "GamePath", ""));
    smm_copy(s->game_name, sizeof s->game_name, smm_json_str(j, "GameName", "Gorilla Tag"));
    s->theme = (int)smm_json_num(j, "Theme", 0);
    smm_copy(s->custom_background, sizeof s->custom_background, smm_json_str(j, "CustomBackground", "#141414"));
    smm_copy(s->custom_accent, sizeof s->custom_accent, smm_json_str(j, "CustomAccent", "#7C3AED"));
    s->auto_check_updates = smm_json_bool(j, "AutoCheckUpdates", 1);
    s->app_version_seen = (int)smm_json_num(j, "AppVersionSeen", 0);
    s->layout = (int)smm_json_num(j, "Layout", 0);
    s->game_picker_display = (int)smm_json_num(j, "GamePickerDisplay", 0);
    load_map(&s->known_game_paths, smm_json_get(j, "KnownGamePaths"));
    load_map(&s->custom_games, smm_json_get(j, "CustomGames"));
    load_map(&s->thunderstore_communities, smm_json_get(j, "ThunderstoreCommunities"));
    load_strs(&s->hidden_presets, smm_json_get(j, "HiddenPresets"));
    load_strs(&s->manual_only_games, smm_json_get(j, "ManualOnlyGames"));
    load_strs(&s->monkeforge_games, smm_json_get(j, "MonkeForgeGames"));
    load_map(&s->steam_exe_paths, smm_json_get(j, "SteamExePaths"));
    load_map(&s->loader_overrides, smm_json_get(j, "LoaderOverrides"));
    load_strs(&s->disable_starting_indicator, smm_json_get(j, "DisableStartingIndicator"));
    s->has_seen_tutorial = smm_json_bool(j, "HasSeenTutorial", 0);
    s->disable_discord_rpc = smm_json_bool(j, "DisableDiscordRpc", 0);
    smm_copy(s->registered_exe, sizeof s->registered_exe, smm_json_str(j, "RegisteredExe", ""));
    smm_json_free(j);
}

static void write_map(SmmBuf *b, const char *name, const SmmMap *m, int numeric)
{
    smm_buf_printf(b, "  \"%s\": {", name);
    for (int i = 0; i < m->n; i++) {
        smm_buf_append(b, i ? ",\n    " : "\n    ", i ? 6 : 5);
        smm_json_escape(b, m->k[i]);
        smm_buf_append(b, ": ", 2);
        if (numeric) smm_buf_append(b, m->v[i], strlen(m->v[i]));
        else smm_json_escape(b, m->v[i]);
    }
    smm_buf_append(b, m->n ? "\n  }" : "}", m->n ? 4 : 1);
}

static void write_strs(SmmBuf *b, const char *name, const SmmStrs *s)
{
    smm_buf_printf(b, "  \"%s\": [", name);
    for (int i = 0; i < s->n; i++) {
        smm_buf_append(b, i ? ",\n    " : "\n    ", i ? 6 : 5);
        smm_json_escape(b, s->v[i]);
    }
    smm_buf_append(b, s->n ? "\n  ]" : "]", s->n ? 4 : 1);
}

int smm_state_save(void)
{
    const SmmSettings *s = &smm_settings;
    SmmBuf b = {0};
    smm_buf_append(&b, "{\n", 2);
    smm_buf_printf(&b, "  \"GamePath\": ");
    smm_json_escape(&b, s->game_path);
    smm_buf_printf(&b, ",\n  \"GameName\": ");
    smm_json_escape(&b, s->game_name);
    smm_buf_printf(&b, ",\n  \"Theme\": %d,\n  \"CustomBackground\": ", s->theme);
    smm_json_escape(&b, s->custom_background);
    smm_buf_printf(&b, ",\n  \"CustomAccent\": ");
    smm_json_escape(&b, s->custom_accent);
    smm_buf_printf(&b, ",\n  \"AutoCheckUpdates\": %s,\n  \"AppVersionSeen\": %d,\n  \"Layout\": %d,\n  \"GamePickerDisplay\": %d,\n",
                   s->auto_check_updates ? "true" : "false", s->app_version_seen, s->layout, s->game_picker_display);
    write_map(&b, "KnownGamePaths", &s->known_game_paths, 0);
    smm_buf_append(&b, ",\n", 2);
    write_map(&b, "CustomGames", &s->custom_games, 0);
    smm_buf_append(&b, ",\n", 2);
    write_map(&b, "ThunderstoreCommunities", &s->thunderstore_communities, 0);
    smm_buf_append(&b, ",\n", 2);
    write_strs(&b, "HiddenPresets", &s->hidden_presets);
    smm_buf_append(&b, ",\n", 2);
    write_strs(&b, "ManualOnlyGames", &s->manual_only_games);
    smm_buf_append(&b, ",\n", 2);
    write_strs(&b, "MonkeForgeGames", &s->monkeforge_games);
    smm_buf_append(&b, ",\n", 2);
    write_map(&b, "SteamExePaths", &s->steam_exe_paths, 0);
    smm_buf_append(&b, ",\n", 2);
    write_map(&b, "LoaderOverrides", &s->loader_overrides, 1);
    smm_buf_append(&b, ",\n", 2);
    write_strs(&b, "DisableStartingIndicator", &s->disable_starting_indicator);
    smm_buf_printf(&b, ",\n  \"HasSeenTutorial\": %s,\n  \"DisableDiscordRpc\": %s,\n  \"RegisteredExe\": ",
                   s->has_seen_tutorial ? "true" : "false", s->disable_discord_rpc ? "true" : "false");
    smm_json_escape(&b, s->registered_exe);
    smm_buf_append(&b, "\n}", 2);
    char path[SMM_PATH], dir[SMM_PATH];
    smm_state_path(path, sizeof path);
    smm_dirname(path, dir, sizeof dir);
    smm_os_mkdir_p(dir);
    int rc = smm_file_write_atomic(path, b.p, b.n);
    smm_buf_free(&b);
    return rc;
}

int smm_game_dir(char *out, size_t n)
{
    if (!smm_settings.game_path[0]) return 0;
    smm_dirname(smm_settings.game_path, out, n);
    return 1;
}

int smm_sb_dir(char *out, size_t n)
{
    char d[SMM_PATH];
    if (!smm_game_dir(d, sizeof d)) return 0;
    smm_join(out, n, d, "Several Bees");
    return 1;
}

int smm_bepinex_dir(char *out, size_t n)
{
    char d[SMM_PATH];
    if (!smm_game_dir(d, sizeof d)) return 0;
    smm_join(out, n, d, "BepInEx");
    return 1;
}

int smm_plugins_dir(char *out, size_t n)
{
    char d[SMM_PATH];
    if (!smm_bepinex_dir(d, sizeof d)) return 0;
    smm_join(out, n, d, "plugins");
    return 1;
}

int smm_melon_mods_dir(char *out, size_t n)
{
    char d[SMM_PATH];
    if (!smm_game_dir(d, sizeof d)) return 0;
    smm_join(out, n, d, "Mods");
    return 1;
}

int smm_mods_install_dir(char *out, size_t n)
{
    return smm_detect_loader() == SMM_LOADER_MELON ? smm_melon_mods_dir(out, n) : smm_plugins_dir(out, n);
}

int smm_packs_dir(char *out, size_t n)
{
    char d[SMM_PATH], slug[96], m[SMM_PATH];
    smm_app_data_dir(d, sizeof d);
    smm_current_slug(slug, sizeof slug);
    smm_join(m, sizeof m, d, "Modpacks");
    smm_join(out, n, m, slug);
    return 1;
}

int smm_has_sb_folder(void)
{
    char d[SMM_PATH];
    return smm_sb_dir(d, sizeof d) && smm_os_is_dir(d);
}

int smm_bepinex_present(void)
{
    char d[SMM_PATH];
    return smm_bepinex_dir(d, sizeof d) && smm_os_is_dir(d);
}

void smm_ensure_sb_folder(void)
{
    char d[SMM_PATH], f[SMM_PATH];
    if (!smm_sb_dir(d, sizeof d) || smm_os_is_dir(d)) return;
    smm_os_mkdir_p(d);
    smm_join(f, sizeof f, d, "readme.md");
    const char *t = "This folder is used by Sev's Mod Manager and Several Bees to store settings and loadouts.\nDo not delete it.\n";
    smm_file_write(f, t, strlen(t));
}

static char g_cfg_game[SMM_PATH];
static SmmMap g_cfg;

static int sb_paths(char *dir, char *file, size_t n)
{
    char sb[SMM_PATH];
    if (!smm_sb_dir(sb, sizeof sb)) {
        if (dir) dir[0] = 0;
        if (file) file[0] = 0;
        return 0;
    }
    char settings[SMM_PATH];
    smm_join(settings, sizeof settings, sb, "settings");
    if (dir) smm_copy(dir, n, settings);
    if (file) smm_join(file, n, settings, "settings.json");
    return 1;
}

int smm_dc_load_settings(void)
{
    smm_map_free(&g_cfg);
    char file[SMM_PATH];
    if (!sb_paths(NULL, file, sizeof file)) return 0;
    smm_copy(g_cfg_game, sizeof g_cfg_game, file);
    SmmBuf b = {0};
    if (smm_file_read(file, &b) != 0) return 0;
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    if (j && j->type == SMM_J_OBJ) {
        for (int i = 0; i < j->n; i++) {
            const SmmJson *k = &j->kids[i];
            char t[64];
            if (k->type == SMM_J_STR) smm_map_set(&g_cfg, k->key, k->str);
            else if (k->type == SMM_J_BOOL) smm_map_set(&g_cfg, k->key, k->b ? "true" : "false");
            else if (k->type == SMM_J_NUM) {
                smm_fmt(t, sizeof t, "%.9g", k->num);
                smm_map_set(&g_cfg, k->key, t);
            }
        }
    }
    smm_json_free(j);
    return 1;
}

static int is_number(const char *s)
{
    char *end;
    if (!*s) return 0;
    strtod(s, &end);
    return *end == 0;
}

static void flush_settings(void)
{
    char dir[SMM_PATH], file[SMM_PATH];
    if (!sb_paths(dir, file, sizeof dir)) return;
    smm_os_mkdir_p(dir);
    SmmBuf b = {0};
    smm_buf_append(&b, "{", 1);
    for (int i = 0; i < g_cfg.n; i++) {
        if (i) smm_buf_append(&b, ",", 1);
        smm_json_escape(&b, g_cfg.k[i]);
        smm_buf_append(&b, ":", 1);
        const char *v = g_cfg.v[i];
        if (!strcmp(v, "true") || !strcmp(v, "false") || is_number(v)) smm_buf_append(&b, v, strlen(v));
        else smm_json_escape(&b, v);
    }
    smm_buf_append(&b, "}", 1);
    smm_file_write_atomic(file, b.p, b.n);
    smm_buf_free(&b);
}

int smm_dc_get_bool(const char *key, int def)
{
    const char *v = smm_map_get(&g_cfg, key);
    if (!v) return def;
    return !strcmp(v, "true") ? 1 : !strcmp(v, "false") ? 0 : def;
}

void smm_dc_set_bool(const char *key, int val)
{
    smm_map_set(&g_cfg, key, val ? "true" : "false");
    flush_settings();
}

double smm_dc_get_float(const char *key, double def)
{
    const char *v = smm_map_get(&g_cfg, key);
    if (!v || !is_number(v)) return def;
    return atof(v);
}

void smm_dc_set_float(const char *key, double val)
{
    char t[64];
    smm_fmt(t, sizeof t, "%.9g", val);
    smm_map_set(&g_cfg, key, t);
    flush_settings();
}

const char *smm_dc_get_string(const char *key, const char *def)
{
    const char *v = smm_map_get(&g_cfg, key);
    return v ? v : def;
}

void smm_dc_set_string(const char *key, const char *val)
{
    smm_map_set(&g_cfg, key, val);
    flush_settings();
}

static int auth_path(char *out, size_t n)
{
    char d[SMM_PATH];
    if (!sb_paths(d, NULL, sizeof d)) return 0;
    smm_join(out, n, d, "auth.json");
    return 1;
}

int smm_token_load(char *out, size_t n)
{
    char p[SMM_PATH];
    if (!auth_path(p, sizeof p)) return 0;
    SmmBuf b = {0};
    if (smm_file_read(p, &b) != 0) return 0;
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    const char *t = smm_json_str(j, "token", NULL);
    int ok = t && *t;
    if (ok) smm_copy(out, n, t);
    smm_json_free(j);
    return ok;
}

int smm_token_save(const char *token)
{
    char d[SMM_PATH], p[SMM_PATH];
    if (!sb_paths(d, NULL, sizeof d)) return -1;
    smm_os_mkdir_p(d);
    auth_path(p, sizeof p);
    SmmBuf b = {0};
    smm_buf_append(&b, "{\"token\":", 9);
    smm_json_escape(&b, token);
    smm_buf_append(&b, "}", 1);
    int rc = smm_file_write(p, b.p, b.n);
    smm_buf_free(&b);
    return rc;
}

void smm_token_delete(void)
{
    char p[SMM_PATH];
    if (auth_path(p, sizeof p)) smm_os_remove(p);
}

static int modver_path(char *out, size_t n)
{
    char d[SMM_PATH];
    if (!sb_paths(d, NULL, sizeof d)) return 0;
    smm_join(out, n, d, "mod_versions.json");
    return 1;
}

static void modver_load(SmmMap *m)
{
    char p[SMM_PATH];
    if (!modver_path(p, sizeof p)) return;
    SmmBuf b = {0};
    if (smm_file_read(p, &b) != 0) return;
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    load_map(m, j);
    smm_json_free(j);
}

static void modver_save(const SmmMap *m)
{
    char d[SMM_PATH], p[SMM_PATH];
    if (!sb_paths(d, NULL, sizeof d)) return;
    smm_os_mkdir_p(d);
    modver_path(p, sizeof p);
    SmmBuf b = {0};
    smm_buf_append(&b, "{", 1);
    for (int i = 0; i < m->n; i++) {
        if (i) smm_buf_append(&b, ",", 1);
        smm_json_escape(&b, m->k[i]);
        smm_buf_append(&b, ":", 1);
        smm_json_escape(&b, m->v[i]);
    }
    smm_buf_append(&b, "}", 1);
    smm_file_write_atomic(p, b.p, b.n);
    smm_buf_free(&b);
}

void smm_mod_version_get(const char *key, char *out, size_t n)
{
    SmmMap m = {0};
    modver_load(&m);
    const char *v = smm_map_get(&m, key);
    smm_copy(out, n, v ? v : "");
    smm_map_free(&m);
}

void smm_mod_version_set(const char *key, const char *version)
{
    SmmMap m = {0};
    modver_load(&m);
    smm_map_set(&m, key, version);
    modver_save(&m);
    smm_map_free(&m);
}

void smm_mod_version_delete(const char *key)
{
    SmmMap m = {0};
    modver_load(&m);
    if (smm_map_del(&m, key)) modver_save(&m);
    smm_map_free(&m);
}

static int loadouts_path(char *dir, char *file, size_t n)
{
    char sb[SMM_PATH];
    if (!smm_sb_dir(sb, sizeof sb)) return 0;
    char d[SMM_PATH];
    smm_join(d, sizeof d, sb, "loadouts");
    if (dir) smm_copy(dir, n, d);
    if (file) smm_join(file, n, d, "loadouts.json");
    return 1;
}

static void load_strs_into(SmmStrs *s, const SmmJson *a)
{
    load_strs(s, a);
}

int smm_loadouts_load(SmmLoadouts *out)
{
    char f[SMM_PATH];
    if (!loadouts_path(NULL, f, sizeof f)) return 0;
    SmmBuf b = {0};
    if (smm_file_read(f, &b) != 0) return 0;
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    if (j && j->type == SMM_J_ARR) {
        for (int i = 0; i < j->n; i++) {
            const SmmJson *o = &j->kids[i];
            if (o->type != SMM_J_OBJ) continue;
            if (out->n == out->cap) {
                out->cap = out->cap ? out->cap * 2 : 8;
                out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmLoadout));
            }
            SmmLoadout *l = &out->v[out->n++];
            memset(l, 0, sizeof *l);
            l->number = (int)smm_json_num(o, "Number", 0);
            smm_copy(l->name, sizeof l->name, smm_json_str(o, "Name", ""));
            load_strs_into(&l->enabled, smm_json_get(o, "EnabledMods"));
            load_strs_into(&l->disabled, smm_json_get(o, "DisabledMods"));
            load_strs_into(&l->missing, smm_json_get(o, "MissingMods"));
        }
    }
    smm_json_free(j);
    return out->n;
}

static void write_str_array(SmmBuf *b, const char *key, const SmmStrs *s)
{
    smm_buf_printf(b, "\"%s\":[", key);
    for (int i = 0; i < s->n; i++) {
        if (i) smm_buf_append(b, ",", 1);
        smm_json_escape(b, s->v[i]);
    }
    smm_buf_append(b, "]", 1);
}

int smm_loadouts_save(const SmmLoadouts *l)
{
    char d[SMM_PATH], f[SMM_PATH];
    if (!loadouts_path(d, f, sizeof d)) return -1;
    smm_os_mkdir_p(d);
    SmmBuf b = {0};
    smm_buf_append(&b, "[", 1);
    for (int i = 0; i < l->n; i++) {
        const SmmLoadout *x = &l->v[i];
        if (i) smm_buf_append(&b, ",", 1);
        smm_buf_printf(&b, "{\"Number\":%d,", x->number);
        if (x->name[0]) {
            smm_buf_append(&b, "\"Name\":", 7);
            smm_json_escape(&b, x->name);
            smm_buf_append(&b, ",", 1);
        }
        write_str_array(&b, "EnabledMods", &x->enabled);
        smm_buf_append(&b, ",", 1);
        write_str_array(&b, "DisabledMods", &x->disabled);
        smm_buf_append(&b, ",", 1);
        write_str_array(&b, "MissingMods", &x->missing);
        smm_buf_append(&b, "}", 1);
    }
    smm_buf_append(&b, "]", 1);
    int rc = smm_file_write_atomic(f, b.p, b.n);
    smm_buf_free(&b);
    return rc;
}
