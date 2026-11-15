#define _DEFAULT_SOURCE
#include "internal.h"
#include <stdlib.h>

static int g_demo;
static int g_running;
static char g_dir[SMM_PATH];
static char g_fix[SMM_PATH];
static int g_logged_in;

int smm_demo_enabled(void)
{
    return g_demo;
}

const char *smm_demo_dir(void)
{
    return g_dir;
}

const char *smm_demo_fixtures(void)
{
    return g_fix;
}

int smm_demo_running_flag(int set, int val)
{
    if (set) g_running = val;
    return g_running;
}

static void fx(const char *name, char *out, size_t n)
{
    smm_join(out, n, g_fix, name);
}

static int fx_json(const char *name, SmmJson **j)
{
    char p[SMM_PATH];
    fx(name, p, sizeof p);
    SmmBuf b = {0};
    if (smm_file_read(p, &b) != 0) return -1;
    *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    return *j ? 0 : -1;
}

static int serve_file(const char *name, SmmBuf *out)
{
    char p[SMM_PATH];
    fx(name, p, sizeof p);
    return smm_file_read(p, out) == 0 ? 200 : 404;
}

static int serve_text(const char *text, SmmBuf *out)
{
    smm_buf_append(out, text, strlen(text));
    return 200;
}

static const char *query_value(const char *s, const char *key, char *out, size_t n)
{
    char pat[64];
    smm_fmt(pat, sizeof pat, "%s=", key);
    const char *q = strchr(s, '?');
    const char *p = q ? q + 1 : s;
    while (p && *p) {
        if (smm_starts_with(p, pat)) {
            const char *v = p + strlen(pat);
            size_t l = strcspn(v, "&");
            if (l >= n) l = n - 1;
            memcpy(out, v, l);
            out[l] = 0;
            return out;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    out[0] = 0;
    return NULL;
}

static int serve_monkeforge(const char *url, SmmBuf *out)
{
    const char *g = strstr(url, "/api/v1/mods/");
    if (!g) {
        char page[16];
        query_value(url, "page", page, sizeof page);
        if (page[0] && atoi(page) > 1) return serve_text("{\"items\":[],\"total\":22,\"page\":2,\"limit\":100}", out);
        return serve_file("monkeforge_mods.json", out);
    }
    char guid[160];
    size_t l = strcspn(g + 13, "?");
    if (l >= sizeof guid) l = sizeof guid - 1;
    memcpy(guid, g + 13, l);
    guid[l] = 0;
    int zip = strstr(guid, "computerinterface") || strstr(guid, "monkecosmetics") || strstr(guid, "bark");
    const char *loaders[] = {"bepinex5", "melonloader"};
    smm_buf_printf(out, "{\"guid\":\"%s\",\"releases\":[{\"version\":\"1.0.0\",\"downloads\":[", guid);
    for (int i = 0; i < 2; i++)
        smm_buf_printf(out, "%s{\"id\":\"demo%d\",\"filename\":\"%s.%s\",\"size\":2048,\"sha256\":\"\",\"kind\":\"%s\",\"loader\":\"%s\",\"url\":\"%s/api/v1/downloads/demo-%s-%s\"}",
                       i ? "," : "", i, guid, zip ? "zip" : "dll", zip ? "zip" : "dll", loaders[i], "https://monkeforge.org", zip ? "zip" : "dll", guid);
    smm_buf_append(out, "]}]}", 4);
    return 200;
}

int smm_demo_http(const char *url, const char *post, SmmBuf *out, int *handled)
{
    if (!g_demo) {
        *handled = 0;
        return -1;
    }
    const char *real = smm_os_getenv("SMM_DEMO_REAL");
    if (real && *real && strstr(url, "steamstatic.com")) {
        *handled = 0;
        return -1;
    }
    *handled = 1;
    char act[64], tab[64];
    if (strstr(url, "thunderstore.io/c/") && strstr(url, "/api/v1/package/")) return serve_file("thunderstore_packages.json", out);
    if (strstr(url, "monkeforge.org/api/v1/mods")) return serve_monkeforge(url, out);
    if (strstr(url, "cyberstorm/community")) return serve_file("communities.json", out);
    if (strstr(url, "raw.githubusercontent.com") && strstr(url, "ver.txt")) return serve_file("ver.txt", out);
    if (strstr(url, "api.github.com") && strstr(url, "/releases/latest")) return serve_text("{\"tag_name\":\"v1.0.0\"}", out);
    if (strstr(url, "3gv.org/link")) {
        query_value(url, "action", act, sizeof act);
        if (!strcmp(act, "generate")) return serve_text("{\"code\":\"DEMO-4821\"}", out);
        return serve_text(g_logged_in ? "{\"token\":\"demo-token\"}" : "{\"token\":\"\"}", out);
    }
    if (strstr(url, "sevvy-wevvy.com/mods/sb/api.php")) {
        const char *src = post ? post : url;
        query_value(src, "action", act, sizeof act);
        if (!strcmp(act, "register_game")) return serve_text("{\"ok\":true,\"data\":{\"id\":1}}", out);
        if (!strcmp(act, "validate_token")) return serve_text("{\"valid\":true,\"username\":\"sevvy\"}", out);
        if (!strcmp(act, "get_user_upvote")) return serve_text("{\"upvoted\":false}", out);
        if (!strcmp(act, "toggle_upvote")) return serve_text("{\"upvoted\":true}", out);
        if (!strcmp(act, "list_mods")) {
            char page[16];
            query_value(url, "page", page, sizeof page);
            if (page[0] && atoi(page) > 1) return serve_text("{\"success\":true,\"mods\":[]}", out);
            query_value(url, "tab", tab, sizeof tab);
            char fn[96];
            smm_fmt(fn, sizeof fn, real && *real ? "sb_real_%s.json" : "sb_mods_%s.json", tab[0] ? tab : "all");
            return serve_file(fn, out);
        }
        return serve_text("{}", out);
    }
    if (strstr(url, "/icons/") || strstr(url, "gcdn.thunderstore.io")) {
        const char *base = smm_basename(url);
        char fn[300];
        smm_fmt(fn, sizeof fn, "icons/%s", base);
        return serve_file(fn, out);
    }
    return 200;
}

static void fake_dll(SmmBuf *b, const char *name)
{
    smm_buf_append(b, "MZ\x90\x00", 4);
    smm_buf_printf(b, "demo dll for %s\n", name);
    for (int i = 0; i < 40; i++) smm_buf_printf(b, "%s padding line %d\n", name, i);
}

static void fake_exe(const char *path)
{
    unsigned char h[160];
    memset(h, 0, sizeof h);
    h[0] = 'M';
    h[1] = 'Z';
    h[0x3C] = 0x40;
    h[0x40] = 'P';
    h[0x41] = 'E';
    h[0x44] = 0x64;
    h[0x45] = 0x86;
    smm_file_write(path, h, sizeof h);
}

int smm_demo_download(const char *url, const char *dest, int *handled)
{
    if (!g_demo) {
        *handled = 0;
        return -1;
    }
    *handled = 1;
    char dir[SMM_PATH];
    smm_dirname(dest, dir, sizeof dir);
    smm_os_mkdir_p(dir);
    SmmBuf dll = {0};
    if (strstr(url, "BepInEx") || strstr(url, "MelonLoader")) {
        int bep = strstr(url, "BepInEx") != NULL;
        SmmZipWriter w;
        if (smm_zipw_open(&w, dest) != 0) return -1;
        fake_dll(&dll, bep ? "winhttp" : "version");
        smm_zipw_add_mem(&w, bep ? "winhttp.dll" : "version.dll", dll.p, dll.n);
        if (bep) {
            smm_zipw_add_mem(&w, "doorstop_config.ini", "[UnityDoorstop]\nenabled=true\n", strlen("[UnityDoorstop]\nenabled=true\n"));
            smm_zipw_add_mem(&w, "BepInEx/core/BepInEx.dll", dll.p, dll.n);
            smm_zipw_add_mem(&w, "BepInEx/config/BepInEx.cfg", "[Logging]\n", strlen("[Logging]\n"));
        } else {
            smm_zipw_add_mem(&w, "MelonLoader/net6/MelonLoader.dll", dll.p, dll.n);
        }
        smm_buf_free(&dll);
        return smm_zipw_close(&w);
    }
    const char *mf = strstr(url, "monkeforge.org/api/v1/downloads/demo-");
    if (mf) {
        int zip = !strncmp(mf + 37, "zip-", 4);
        const char *guid = mf + 41;
        if (!zip) {
            fake_dll(&dll, guid);
            int rc = smm_file_write(dest, dll.p, dll.n);
            smm_buf_free(&dll);
            return rc;
        }
        SmmZipWriter w;
        if (smm_zipw_open(&w, dest) != 0) return -1;
        char entry[400];
        fake_dll(&dll, guid);
        smm_fmt(entry, sizeof entry, "BepInEx/plugins/%s/%s.dll", guid, guid);
        smm_zipw_add_mem(&w, entry, dll.p, dll.n);
        smm_buf_free(&dll);
        return smm_zipw_close(&w);
    }
    char owner[96] = "", name[96] = "";
    const char *d = strstr(url, "/package/download/");
    if (d) sscanf(d + 18, "%95[^/]/%95[^/]", owner, name);
    if (d && name[0]) {
        SmmZipWriter w;
        if (smm_zipw_open(&w, dest) != 0) return -1;
        char file[200], manifest[400];
        fake_dll(&dll, name);
        smm_fmt(file, sizeof file, "plugins/%s.dll", name);
        smm_fmt(manifest, sizeof manifest, "{\"name\":\"%s\",\"version_number\":\"1.0.0\"}", name);
        smm_zipw_add_mem(&w, "manifest.json", manifest, strlen(manifest));
        smm_zipw_add_mem(&w, "README.md", "# demo package\n", strlen("# demo package\n"));
        smm_zipw_add_mem(&w, file, dll.p, dll.n);
        smm_buf_free(&dll);
        return smm_zipw_close(&w);
    }
    fake_dll(&dll, smm_basename(dest));
    int rc = smm_file_write(dest, dll.p, dll.n);
    smm_buf_free(&dll);
    return rc;
}

static void put_game(const char *steamapps, int appid, const char *name, const char *installdir, const char *exe)
{
    char id[32], fn[64], mf[SMM_PATH], body[600], common[SMM_PATH], gd[SMM_PATH], ep[SMM_PATH];
    smm_fmt(id, sizeof id, "%d", appid);
    smm_fmt(fn, sizeof fn, "appmanifest_%d.acf", appid);
    smm_join(mf, sizeof mf, steamapps, fn);
    smm_fmt(body, sizeof body, "\"AppState\"\n{\n\t\"appid\"\t\t\"%d\"\n\t\"name\"\t\t\"%s\"\n\t\"installdir\"\t\t\"%s\"\n}\n", appid, name, installdir);
    smm_file_write(mf, body, strlen(body));
    smm_join(common, sizeof common, steamapps, "common");
    smm_join(gd, sizeof gd, common, installdir);
    smm_os_mkdir_p(gd);
    smm_join(ep, sizeof ep, gd, exe);
    fake_exe(ep);
}

static void build_games(const char *steamapps, char *gt_exe, size_t n)
{
    SmmJson *j = NULL;
    if (fx_json("demo_games.json", &j) != 0) return;
    for (int i = 0; i < j->n; i++) {
        const SmmJson *g = &j->kids[i];
        if (g->n < 4) continue;
        int appid = (int)g->kids[0].num;
        put_game(steamapps, appid, g->kids[1].str, g->kids[2].str, g->kids[3].str);
        if (appid == 1533390) {
            char common[SMM_PATH], gd[SMM_PATH];
            smm_join(common, sizeof common, steamapps, "common");
            smm_join(gd, sizeof gd, common, g->kids[2].str);
            smm_join(gt_exe, n, gd, g->kids[3].str);
        }
    }
    smm_json_free(j);
    char vdf[SMM_PATH];
    smm_join(vdf, sizeof vdf, steamapps, "libraryfolders.vdf");
    const char *body = "\"libraryfolders\"\n{\n}\n";
    smm_file_write(vdf, body, strlen(body));
}

static void copy_fixture(const char *name, const char *dest)
{
    char src[SMM_PATH];
    fx(name, src, sizeof src);
    smm_os_copy(src, dest);
}

static void build_plugins(const char *game, SmmMap *versions)
{
    SmmJson *j = NULL;
    char plugins[SMM_PATH], bep[SMM_PATH];
    smm_join(bep, sizeof bep, game, "BepInEx");
    smm_join(plugins, sizeof plugins, bep, "plugins");
    if (fx_json("installed.json", &j) != 0) return;
    for (int i = 0; i < j->n; i++) {
        const SmmJson *m = &j->kids[i];
        const char *name = smm_json_str(m, "name", "");
        int folder = smm_json_bool(m, "folder", 0), enabled = smm_json_bool(m, "enabled", 1);
        const char *ver = smm_json_str(m, "version", "");
        SmmBuf dll = {0};
        fake_dll(&dll, name);
        char short_name[160];
        smm_melon_key(name, short_name, sizeof short_name);
        char path[SMM_PATH], file[SMM_PATH], fn[300];
        if (folder) {
            char dir[SMM_PATH];
            smm_join(dir, sizeof dir, plugins, name);
            smm_os_mkdir_p(dir);
            smm_fmt(fn, sizeof fn, "%s.dll%s", short_name, enabled ? "" : ".disabled");
            smm_join(file, sizeof file, dir, fn);
            smm_file_write(file, dll.p, dll.n);
            smm_join(path, sizeof path, dir, "manifest.json");
            smm_file_write(path, "{}", strlen("{}"));
        } else {
            smm_fmt(fn, sizeof fn, "%s.dll%s", name, enabled ? "" : ".disabled");
            smm_join(file, sizeof file, plugins, fn);
            smm_file_write(file, dll.p, dll.n);
        }
        smm_buf_free(&dll);
        if (ver[0]) smm_map_set(versions, name, ver);
    }
    smm_json_free(j);
}

static void build_sb(const char *game, const SmmMap *versions)
{
    char sb[SMM_PATH], settings[SMM_PATH], loadouts[SMM_PATH], p[SMM_PATH];
    smm_join(sb, sizeof sb, game, "Several Bees");
    smm_join(settings, sizeof settings, sb, "settings");
    smm_join(loadouts, sizeof loadouts, sb, "loadouts");
    smm_os_mkdir_p(settings);
    smm_os_mkdir_p(loadouts);
    smm_join(p, sizeof p, sb, "readme.md");
    const char *t = "This folder is used by Sev's Mod Manager and Several Bees to store settings and loadouts.\nDo not delete it.\n";
    smm_file_write(p, t, strlen(t));
    smm_join(p, sizeof p, settings, "settings.json");
    copy_fixture("sb_settings.json", p);
    smm_join(p, sizeof p, loadouts, "loadouts.json");
    copy_fixture("loadouts.json", p);
    SmmBuf b = {0};
    smm_buf_append(&b, "{", 1);
    for (int i = 0; i < versions->n; i++) {
        if (i) smm_buf_append(&b, ",", 1);
        smm_json_escape(&b, versions->k[i]);
        smm_buf_append(&b, ":", 1);
        smm_json_escape(&b, versions->v[i]);
    }
    smm_buf_append(&b, "}", 1);
    smm_join(p, sizeof p, settings, "mod_versions.json");
    smm_file_write(p, b.p, b.n);
    smm_buf_free(&b);
    if (g_logged_in) smm_token_save("demo-token");
    char hist[SMM_PATH], src[SMM_PATH];
    smm_join(hist, sizeof hist, sb, "LogHistory");
    smm_os_mkdir_p(hist);
    fx("log_history", src, sizeof src);
    SmmDirList l = {0};
    if (smm_os_list(src, &l) == 0) {
        for (int i = 0; i < l.n; i++) {
            char a[SMM_PATH], d[SMM_PATH];
            smm_join(a, sizeof a, src, l.v[i].name);
            smm_join(d, sizeof d, hist, l.v[i].name);
            smm_os_copy(a, d);
        }
    }
    smm_dirlist_free(&l);
}

static void build_game_dir(const char *game)
{
    char bep[SMM_PATH], p[SMM_PATH], sub[SMM_PATH];
    smm_join(bep, sizeof bep, game, "BepInEx");
    const char *subs[] = {"plugins", "config", "patchers", "core"};
    for (int i = 0; i < 4; i++) {
        smm_join(sub, sizeof sub, bep, subs[i]);
        smm_os_mkdir_p(sub);
    }
    SmmBuf dll = {0};
    fake_dll(&dll, "winhttp");
    smm_join(p, sizeof p, game, "winhttp.dll");
    smm_file_write(p, dll.p, dll.n);
    smm_buf_free(&dll);
    smm_join(p, sizeof p, game, "doorstop_config.ini");
    smm_file_write(p, "[UnityDoorstop]\nenabled=true\n", strlen("[UnityDoorstop]\nenabled=true\n"));
    smm_join(p, sizeof p, game, "UnityPlayer.dll");
    smm_file_write(p, "MZ", strlen("MZ"));
    char cfg[SMM_PATH], src[SMM_PATH];
    smm_join(cfg, sizeof cfg, bep, "config");
    fx("configs", src, sizeof src);
    smm_os_copy_tree(src, cfg);
    smm_join(p, sizeof p, bep, "LogOutput.log");
    copy_fixture("LogOutput.log", p);
}

static void build_packs(void)
{
    SmmJson *j = NULL;
    if (fx_json("modpacks.json", &j) != 0) return;
    char dir[SMM_PATH];
    smm_packs_dir(dir, sizeof dir);
    smm_os_mkdir_p(dir);
    for (int i = 0; i < j->n; i++) {
        const SmmJson *pk = &j->kids[i];
        const char *name = smm_json_str(pk, "name", "Pack");
        const char *author = smm_json_str(pk, "author", "");
        const SmmJson *mods = smm_json_get(pk, "mods");
        char fn[300], dest[SMM_PATH], when[40];
        smm_fmt(fn, sizeof fn, "%s.smmpack", name);
        smm_join(dest, sizeof dest, dir, fn);
        SmmZipWriter w;
        if (smm_zipw_open(&w, dest) != 0) continue;
        SmmBuf mf = {0};
        smm_iso_time(1763200000 + i * 86400, when, sizeof when);
        smm_buf_append(&mf, "{\n  \"Name\": ", 12);
        smm_json_escape(&mf, name);
        smm_buf_append(&mf, ",\n  \"Author\": ", 14);
        smm_json_escape(&mf, author);
        smm_buf_printf(&mf, ",\n  \"GameSlug\": \"gorillatag\",\n  \"CreatedUtc\": \"%s\",\n  \"Mods\": [", when);
        for (int k = 0; mods && k < mods->n; k++) {
            smm_buf_append(&mf, k ? ", " : "", k ? 2 : 0);
            smm_json_escape(&mf, mods->kids[k].str);
        }
        smm_buf_append(&mf, "],\n  \"LoaderKind\": 0\n}", 22);
        smm_zipw_add_mem(&w, "smmpack.json", mf.p, mf.n);
        smm_buf_free(&mf);
        for (int k = 0; mods && k < mods->n; k++) {
            const char *m = mods->kids[k].str;
            char short_name[160], entry[400];
            smm_melon_key(m, short_name, sizeof short_name);
            SmmBuf dll = {0};
            fake_dll(&dll, m);
            smm_fmt(entry, sizeof entry, "BepInEx/plugins/%s/%s.dll", m, short_name);
            smm_zipw_add_mem(&w, entry, dll.p, dll.n);
            smm_fmt(entry, sizeof entry, "BepInEx/config/%s.cfg", short_name);
            smm_zipw_add_mem(&w, entry, "[General]\n\nEnabled = true\n", 26);
            smm_buf_free(&dll);
        }
        smm_zipw_close(&w);
    }
    smm_json_free(j);
}

static int dir_is_empty_or_marked(const char *dir)
{
    if (!smm_os_exists(dir)) return 1;
    char m[SMM_PATH];
    smm_join(m, sizeof m, dir, ".smm-demo");
    if (smm_os_is_file(m)) return 1;
    SmmDirList l = {0};
    smm_os_list(dir, &l);
    int empty = l.n == 0;
    smm_dirlist_free(&l);
    return empty;
}

int smm_demo_init(const char *fixtures)
{
    const char *cands[] = {fixtures, smm_os_getenv("SMM_FIXTURES"), "fixtures", "../fixtures"};
    g_fix[0] = 0;
    for (int i = 0; i < 4; i++) {
        char probe[SMM_PATH];
        if (!cands[i] || !cands[i][0]) continue;
        smm_join(probe, sizeof probe, cands[i], "thunderstore_packages.json");
        if (smm_os_is_file(probe)) {
            smm_copy(g_fix, sizeof g_fix, cands[i]);
            break;
        }
    }
    if (!g_fix[0]) {
        smm_set_error("Demo fixtures folder not found. Set SMM_FIXTURES.");
        return -1;
    }
    const char *d = smm_os_getenv("SMM_DEMO_DIR");
    if (d && *d) {
        smm_copy(g_dir, sizeof g_dir, d);
    } else {
        char t[SMM_PATH];
        smm_os_temp_dir(t, sizeof t);
        smm_join(g_dir, sizeof g_dir, t, "smm-demo");
    }
    if (!dir_is_empty_or_marked(g_dir)) {
        smm_set_error("%s isn't empty and isn't a demo folder, refusing to wipe it.", g_dir);
        return -1;
    }
    smm_os_remove_tree(g_dir);
    smm_os_mkdir_p(g_dir);
    char marker[SMM_PATH];
    smm_join(marker, sizeof marker, g_dir, ".smm-demo");
    smm_file_write(marker, "demo\n", strlen("demo\n"));
    g_demo = 1;
    const char *login = smm_os_getenv("SMM_DEMO_LOGIN");
    g_logged_in = login && *login && strcmp(login, "0") != 0;
    smm_set_root_override(g_dir);
    char steam[SMM_PATH], steamapps[SMM_PATH], tmp[SMM_PATH];
    smm_join(steam, sizeof steam, g_dir, "steam");
    smm_join(steamapps, sizeof steamapps, steam, "steamapps");
    smm_join(tmp, sizeof tmp, g_dir, "tmp");
    smm_os_mkdir_p(steamapps);
    smm_os_mkdir_p(tmp);
    smm_os_setenv("SMM_STEAM_ROOT", steam);
    smm_os_setenv("SMM_TMP", tmp);
    char exe[SMM_PATH];
    exe[0] = 0;
    build_games(steamapps, exe, sizeof exe);
    smm_state_init();
    smm_copy(smm_settings.game_name, sizeof smm_settings.game_name, "Gorilla Tag");
    smm_copy(smm_settings.game_path, sizeof smm_settings.game_path, exe);
    smm_map_set(&smm_settings.known_game_paths, "Gorilla Tag", exe);
    const char *src = smm_os_getenv("SMM_DEMO_SOURCE");
    if (src && strcmp(src, "mf") == 0) smm_strs_add(&smm_settings.monkeforge_games, "Gorilla Tag");
    else if (!src || strcmp(src, "sb") != 0) smm_map_set(&smm_settings.thunderstore_communities, "Gorilla Tag", "gorilla-tag");
    smm_settings.app_version_seen = SMM_VERSION;
    const char *tut = smm_os_getenv("SMM_DEMO_TUTORIAL");
    smm_settings.has_seen_tutorial = !(tut && *tut && strcmp(tut, "0") != 0);
    smm_settings.disable_discord_rpc = 1;
    char gd[SMM_PATH];
    smm_game_dir(gd, sizeof gd);
    SmmMap versions = {0};
    build_game_dir(gd);
    build_plugins(gd, &versions);
    build_sb(gd, &versions);
    smm_map_free(&versions);
    build_packs();
    smm_state_save();
    smm_dc_load_settings();
    if (g_logged_in) smm_sb_set_token("demo-token");
    return 0;
}
