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

static void library_folders(SmmStrs *out)
{
    char root[SMM_PATH];
    if (!smm_steam_root(root, sizeof root)) return;
    smm_strs_add(out, root);
    char sa[SMM_PATH], vdf[SMM_PATH];
    smm_join(sa, sizeof sa, root, "steamapps");
    smm_join(vdf, sizeof vdf, sa, "libraryfolders.vdf");
    SmmBuf b = {0};
    if (smm_file_read(vdf, &b) != 0) return;
    const char *p = b.p;
    while ((p = smm_stristr(p, "\"path\""))) {
        p += 6;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '"') continue;
        p++;
        const char *e = strchr(p, '"');
        if (!e) break;
        char path[SMM_PATH];
        size_t k = 0;
        for (const char *c = p; c < e && k + 1 < sizeof path; c++) {
            if (c[0] == '\\' && c + 1 < e && c[1] == '\\') c++;
            path[k++] = *c;
        }
        path[k] = 0;
        if (smm_strs_find(out, path, 1) < 0) smm_strs_add(out, path);
        p = e + 1;
    }
    smm_buf_free(&b);
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
