#include "internal.h"
#include <stdlib.h>

static int vdf_value(const char *text, const char *key, char *out, size_t n)
{
    char pat[128];
    smm_fmt(pat, sizeof pat, "\"%s\"", key);
    const char *p = text;
    while ((p = smm_stristr(p, pat))) {
        const char *q = p + strlen(pat);
        while (*q == ' ' || *q == '\t') q++;
        if (*q == '"') {
            q++;
            const char *e = strchr(q, '"');
            if (!e) return 0;
            size_t l = (size_t)(e - q);
            if (l >= n) l = n - 1;
            memcpy(out, q, l);
            out[l] = 0;
            return 1;
        }
        p = q;
    }
    return 0;
}

int smm_steam_root(char *out, size_t n)
{
    const char *env = smm_os_getenv("SMM_STEAM_ROOT");
    if (env && *env) {
        smm_copy(out, n, env);
        return 1;
    }
    if (smm_os_is_windows()) return smm_os_registry_steam(out, n) == 0 && smm_os_is_dir(out);
    const char *home = smm_os_home();
    const char *rel[] = {
        ".steam/steam", ".local/share/Steam", ".steam/root", ".var/app/com.valvesoftware.Steam/.local/share/Steam",
        "snap/steam/common/.local/share/Steam", "Library/Application Support/Steam",
    };
    for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++) {
        char p[SMM_PATH], sa[SMM_PATH];
        smm_join(p, sizeof p, home, rel[i]);
        smm_join(sa, sizeof sa, p, "steamapps");
        if (smm_os_is_dir(sa)) {
            smm_copy(out, n, p);
            return 1;
        }
    }
    return 0;
}

int smm_steam_roots(SmmStrs *out)
{
    const char *env = smm_os_getenv("SMM_STEAM_ROOT");
    if (env && *env) {
        smm_strs_add(out, env);
        return out->n;
    }
    const char *home = smm_os_home();
    const char *rel[] = {
        ".steam/steam", ".local/share/Steam", ".steam/root", ".var/app/com.valvesoftware.Steam/.local/share/Steam",
        "snap/steam/common/.local/share/Steam",
    };
    for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++) {
        char p[SMM_PATH], sub[SMM_PATH], real[SMM_PATH];
        smm_join(p, sizeof p, home, rel[i]);
        smm_join(sub, sizeof sub, p, "userdata");
        if (!smm_os_is_dir(sub)) continue;
        if (smm_os_realpath(p, real, sizeof real) != 0) smm_copy(real, sizeof real, p);
        if (smm_strs_find(out, real, 0) < 0) smm_strs_add(out, real);
    }
    return out->n;
}

static int vdf_string(const char **pp, char *out, size_t n)
{
    const char *p = *pp;
    size_t k = 0;
    if (*p != '"') return 0;
    p++;
    while (*p && *p != '"') {
        if (p[0] == '\\' && (p[1] == '\\' || p[1] == '"')) p++;
        if (k + 1 < n) out[k++] = *p;
        p++;
    }
    out[k] = 0;
    *pp = *p == '"' ? p + 1 : p;
    return 1;
}

static int all_digits(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

static void add_library(SmmStrs *out, const char *path)
{
    if (path[0] && smm_strs_find(out, path, 1) < 0) smm_strs_add(out, path);
}

static void read_library_file(SmmStrs *out, const char *file)
{
    SmmBuf b = {0};
    if (smm_file_read(file, &b) != 0) return;
    const char *p = b.p;
    char key[SMM_PATH], val[SMM_PATH];
    while (p && *p) {
        if (*p != '"') {
            p++;
            continue;
        }
        if (!vdf_string(&p, key, sizeof key)) break;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != '"') continue;
        if (!vdf_string(&p, val, sizeof val)) break;
        if (smm_strcasecmp(key, "path") == 0 || all_digits(key)) add_library(out, val);
    }
    smm_buf_free(&b);
}

static void library_folders(SmmStrs *out)
{
    SmmStrs roots = {0};
    smm_steam_roots(&roots);
    char first[SMM_PATH];
    if (smm_steam_root(first, sizeof first)) add_library(&roots, first);
    for (int i = 0; i < roots.n; i++) {
        add_library(out, roots.v[i]);
        char f[SMM_PATH], sa[SMM_PATH], cfg[SMM_PATH];
        smm_join(sa, sizeof sa, roots.v[i], "steamapps");
        smm_join(f, sizeof f, sa, "libraryfolders.vdf");
        read_library_file(out, f);
        smm_join(cfg, sizeof cfg, roots.v[i], "config");
        smm_join(f, sizeof f, cfg, "libraryfolders.vdf");
        read_library_file(out, f);
    }
    smm_strs_free(&roots);
}

static int find_install(int appid, char *game_dir, size_t gn, char *lib, size_t ln)
{
    SmmStrs libs = {0};
    library_folders(&libs);
    int found = 0;
    for (int i = 0; i < libs.n && !found; i++) {
        char sa[SMM_PATH], mf[SMM_PATH], fn[64];
        smm_join(sa, sizeof sa, libs.v[i], "steamapps");
        smm_fmt(fn, sizeof fn, "appmanifest_%d.acf", appid);
        smm_join(mf, sizeof mf, sa, fn);
        SmmBuf b = {0};
        if (smm_file_read(mf, &b) != 0) continue;
        char inst[SMM_PATH];
        if (vdf_value(b.p, "installdir", inst, sizeof inst)) {
            char common[SMM_PATH], gd[SMM_PATH];
            smm_join(common, sizeof common, sa, "common");
            smm_join(gd, sizeof gd, common, inst);
            if (smm_os_is_dir(gd)) {
                smm_copy(game_dir, gn, gd);
                smm_copy(lib, ln, libs.v[i]);
                found = 1;
            }
        }
        smm_buf_free(&b);
    }
    smm_strs_free(&libs);
    return found;
}

static const char *under_common(const char *path, const char *common)
{
    size_t cl = strlen(common);
    if (strncmp(path, common, cl) != 0 || path[cl] != '/') return NULL;
    return path + cl + 1;
}

int smm_steam_appid_for_path(const char *path)
{
    char real_path[SMM_PATH];
    if (smm_os_realpath(path, real_path, sizeof real_path) != 0) smm_copy(real_path, sizeof real_path, path);
    SmmStrs libs = {0};
    library_folders(&libs);
    int found = 0;
    for (int i = 0; i < libs.n && !found; i++) {
        char sa[SMM_PATH], common[SMM_PATH], real_common[SMM_PATH];
        smm_join(sa, sizeof sa, libs.v[i], "steamapps");
        smm_join(common, sizeof common, sa, "common");
        if (smm_os_realpath(common, real_common, sizeof real_common) != 0) smm_copy(real_common, sizeof real_common, common);
        const char *rest = under_common(path, common);
        if (!rest) rest = under_common(path, real_common);
        if (!rest) rest = under_common(real_path, common);
        if (!rest) rest = under_common(real_path, real_common);
        if (!rest) continue;
        char dirname[SMM_PATH];
        size_t k = strcspn(rest, "/");
        if (k == 0 || k >= sizeof dirname) continue;
        memcpy(dirname, rest, k);
        dirname[k] = 0;
        SmmDirList l = {0};
        if (smm_os_list(sa, &l) != 0) continue;
        for (int m = 0; m < l.n && !found; m++) {
            if (!smm_starts_with(l.v[m].name, "appmanifest_") || !smm_ends_with(l.v[m].name, ".acf")) continue;
            char f[SMM_PATH], inst[SMM_PATH], id[32];
            smm_join(f, sizeof f, sa, l.v[m].name);
            SmmBuf b = {0};
            if (smm_file_read(f, &b) != 0) continue;
            if (vdf_value(b.p, "installdir", inst, sizeof inst) && strcmp(inst, dirname) == 0 && vdf_value(b.p, "appid", id, sizeof id))
                found = atoi(id);
            smm_buf_free(&b);
        }
        smm_dirlist_free(&l);
    }
    smm_strs_free(&libs);
    return found > 0 ? found : 0;
}

int smm_steam_find_game_path(int appid, const char *exe_name, char *out, size_t n)
{
    char gd[SMM_PATH], lib[SMM_PATH];
    if (!find_install(appid, gd, sizeof gd, lib, sizeof lib)) return 0;
    char p[SMM_PATH];
    smm_join(p, sizeof p, gd, exe_name);
    if (smm_os_is_file(p)) {
        smm_copy(out, n, p);
        return 1;
    }
    SmmDirList l = {0};
    int found = 0;
    if (smm_os_list(gd, &l) == 0) {
        for (int i = 0; i < l.n && !found; i++) {
            if (!l.v[i].is_dir) continue;
            char sub[SMM_PATH];
            smm_join(sub, sizeof sub, gd, l.v[i].name);
            smm_join(p, sizeof p, sub, exe_name);
            if (smm_os_is_file(p)) {
                smm_copy(out, n, p);
                found = 1;
            }
        }
    }
    smm_dirlist_free(&l);
    return found;
}

int smm_steam_exe_candidates(int appid, SmmStrs *out)
{
    char gd[SMM_PATH], lib[SMM_PATH];
    if (!find_install(appid, gd, sizeof gd, lib, sizeof lib)) return 0;
    SmmDirList l = {0};
    if (smm_os_list(gd, &l) == 0) {
        for (int i = 0; i < l.n; i++) {
            if (l.v[i].is_dir || !smm_ends_with_nocase(l.v[i].name, ".exe")) continue;
            if (smm_stristr(l.v[i].name, "CrashHandler") || smm_stristr(l.v[i].name, "redist")) continue;
            char p[SMM_PATH];
            smm_join(p, sizeof p, gd, l.v[i].name);
            smm_strs_add(out, p);
        }
    }
    smm_dirlist_free(&l);
    return out->n;
}

int smm_steam_compat_prefix(int appid, char *out, size_t n)
{
    char gd[SMM_PATH], lib[SMM_PATH];
    if (!find_install(appid, gd, sizeof gd, lib, sizeof lib)) return 0;
    char a[SMM_PATH], b[SMM_PATH], c[SMM_PATH], id[32];
    smm_join(a, sizeof a, lib, "steamapps");
    smm_join(b, sizeof b, a, "compatdata");
    smm_fmt(id, sizeof id, "%d", appid);
    smm_join(c, sizeof c, b, id);
    smm_join(out, n, c, "pfx");
    return smm_os_is_dir(out);
}

static int cmp_games(const void *a, const void *b)
{
    return smm_strcasecmp(((const SmmSteamGame *)a)->name, ((const SmmSteamGame *)b)->name);
}

static int is_tool(const char *name)
{
    return smm_starts_with(name, "Proton") || smm_starts_with(name, "Steam Linux Runtime") ||
           smm_starts_with(name, "Steamworks Common");
}

int smm_steam_list_installed(SmmSteamGames *out)
{
    SmmStrs libs = {0};
    library_folders(&libs);
    for (int i = 0; i < libs.n; i++) {
        char sa[SMM_PATH];
        smm_join(sa, sizeof sa, libs.v[i], "steamapps");
        SmmDirList l = {0};
        if (smm_os_list(sa, &l) != 0) continue;
        for (int k = 0; k < l.n; k++) {
            if (!smm_starts_with(l.v[k].name, "appmanifest_") || !smm_ends_with(l.v[k].name, ".acf")) continue;
            char f[SMM_PATH];
            smm_join(f, sizeof f, sa, l.v[k].name);
            SmmBuf b = {0};
            if (smm_file_read(f, &b) != 0) continue;
            char id[32], name[128];
            if (vdf_value(b.p, "appid", id, sizeof id) && vdf_value(b.p, "name", name, sizeof name) && !is_tool(name)) {
                int appid = atoi(id);
                int dup = 0;
                for (int d = 0; d < out->n; d++)
                    if (out->v[d].appid == appid) dup = 1;
                if (!dup && appid > 0) {
                    if (out->n == out->cap) {
                        out->cap = out->cap ? out->cap * 2 : 32;
                        out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmSteamGame));
                    }
                    out->v[out->n].appid = appid;
                    smm_copy(out->v[out->n].name, sizeof out->v[out->n].name, name);
                    out->n++;
                }
            }
            smm_buf_free(&b);
        }
        smm_dirlist_free(&l);
    }
    smm_strs_free(&libs);
    if (out->n > 1) qsort(out->v, (size_t)out->n, sizeof(SmmSteamGame), cmp_games);
    return out->n;
}
