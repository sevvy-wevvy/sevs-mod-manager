#include "internal.h"
#include <stdlib.h>

#define BEPINEX_VERSION "5.4.23.5"
#define MELON_VERSION "0.7.3"

static const char *ml_url(int x64)
{
    return x64 ? "https://github.com/LavaGang/MelonLoader/releases/download/v" MELON_VERSION "/MelonLoader.x64.zip"
               : "https://github.com/LavaGang/MelonLoader/releases/download/v" MELON_VERSION "/MelonLoader.x86.zip";
}

static const char *bep_url(int x64)
{
    return x64 ? "https://github.com/BepInEx/BepInEx/releases/download/v" BEPINEX_VERSION "/BepInEx_win_x64_" BEPINEX_VERSION ".zip"
               : "https://github.com/BepInEx/BepInEx/releases/download/v" BEPINEX_VERSION "/BepInEx_win_x86_" BEPINEX_VERSION ".zip";
}

static const char *entry_dll(int kind)
{
    return kind == SMM_LOADER_BEPINEX ? "winhttp.dll" : "version.dll";
}

static int bepinex_installed(const char *dir)
{
    char a[SMM_PATH], b[SMM_PATH];
    smm_join(a, sizeof a, dir, "BepInEx");
    smm_join(b, sizeof b, dir, "winhttp.dll");
    return smm_os_is_dir(a) && smm_os_is_file(b);
}

static int melon_installed(const char *dir)
{
    char a[SMM_PATH], b[SMM_PATH];
    smm_join(a, sizeof a, dir, "MelonLoader");
    smm_join(b, sizeof b, dir, "version.dll");
    return smm_os_is_dir(a) && smm_os_is_file(b);
}

const char *smm_loader_name(int kind)
{
    return kind == SMM_LOADER_BEPINEX ? "BepInEx" : "MelonLoader";
}

int smm_loader_installed(int kind, const char *game_dir)
{
    return kind == SMM_LOADER_BEPINEX ? bepinex_installed(game_dir) : melon_installed(game_dir);
}

int smm_detect_loader(void)
{
    char dir[SMM_PATH];
    if (!smm_game_dir(dir, sizeof dir)) return SMM_LOADER_BEPINEX;
    const char *forced = smm_map_get(&smm_settings.loader_overrides, smm_settings.game_name);
    if (forced) return atoi(forced) == SMM_LOADER_MELON ? SMM_LOADER_MELON : SMM_LOADER_BEPINEX;
    if (bepinex_installed(dir)) return SMM_LOADER_BEPINEX;
    if (melon_installed(dir)) return SMM_LOADER_MELON;
    char ga[SMM_PATH];
    smm_join(ga, sizeof ga, dir, "GameAssembly.dll");
    return smm_os_is_file(ga) ? SMM_LOADER_MELON : SMM_LOADER_BEPINEX;
}

int smm_exe_is_x64(const char *exe)
{
    FILE *f = smm_os_fopen(exe, "rb");
    if (!f) return 1;
    unsigned char h[64];
    int x64 = 1;
    if (fread(h, 1, 64, f) == 64 && h[0] == 'M' && h[1] == 'Z') {
        long pe = (long)(h[0x3C] | h[0x3D] << 8 | h[0x3E] << 16 | (unsigned)h[0x3F] << 24);
        unsigned char s[6];
        if (pe > 0 && fseek(f, pe, SEEK_SET) == 0 && fread(s, 1, 6, f) == 6 && s[0] == 'P' && s[1] == 'E' && s[2] == 0 && s[3] == 0)
            x64 = !(s[4] == 0x4c && s[5] == 0x01);
    }
    fclose(f);
    return x64;
}

int smm_loader_install_after(int kind, const char *game_dir, SmmProgress *p, void (*wipe)(void))
{
    int x64 = smm_exe_is_x64(smm_settings.game_path);
    const char *url = kind == SMM_LOADER_BEPINEX ? bep_url(x64) : ml_url(x64);
    const char *name = smm_loader_name(kind);
    char tmp[SMM_PATH], file[96];
    smm_os_temp_dir(tmp, sizeof tmp);
    smm_os_mkdir_p(tmp);
    smm_fmt(file, sizeof file, "%s_install.zip", name);
    char zip[SMM_PATH];
    smm_join(zip, sizeof zip, tmp, file);
    smm_progress(p, 5, "Downloading %s...", name);
    if (smm_http_download(url, zip, p, name, 5, 85) != 0) return -1;
    SmmZip z;
    if (smm_zip_open_file(&z, zip) != 0) {
        smm_os_remove(zip);
        return -1;
    }
    if (wipe) wipe();
    smm_progress(p, 88, "Extracting %s...", name);
    int rc = smm_zip_extract_all(&z, game_dir);
    smm_zip_close(&z);
    smm_os_remove(zip);
    if (rc != 0) return -1;
    char d[SMM_PATH], sub[SMM_PATH];
    if (kind == SMM_LOADER_BEPINEX) {
        smm_join(d, sizeof d, game_dir, "BepInEx");
        const char *subs[] = {"plugins", "config", "patchers"};
        for (int i = 0; i < 3; i++) {
            smm_join(sub, sizeof sub, d, subs[i]);
            smm_os_mkdir_p(sub);
        }
    } else {
        const char *subs[] = {"Mods", "Plugins", "UserData"};
        for (int i = 0; i < 3; i++) {
            smm_join(sub, sizeof sub, game_dir, subs[i]);
            smm_os_mkdir_p(sub);
        }
    }
    smm_progress(p, 98, "Cleaning up...");
    smm_progress(p, 100, "%s installed.", name);
    return 0;
}

int smm_loader_install(int kind, const char *game_dir, SmmProgress *p)
{
    return smm_loader_install_after(kind, game_dir, p, NULL);
}

int smm_loader_uninstall(int kind, const char *game_dir)
{
    char path[SMM_PATH];
    if (kind == SMM_LOADER_BEPINEX) {
        smm_join(path, sizeof path, game_dir, "BepInEx");
        smm_os_remove_tree(path);
        const char *files[] = {"winhttp.dll", "winhttp.dll.disabled", "doorstop_config.ini"};
        for (int i = 0; i < 3; i++) {
            smm_join(path, sizeof path, game_dir, files[i]);
            smm_os_remove(path);
        }
    } else {
        const char *dirs[] = {"MelonLoader", "Mods", "Plugins", "UserData"};
        for (int i = 0; i < 4; i++) {
            smm_join(path, sizeof path, game_dir, dirs[i]);
            smm_os_remove_tree(path);
        }
        const char *files[] = {"version.dll", "version.dll.disabled"};
        for (int i = 0; i < 2; i++) {
            smm_join(path, sizeof path, game_dir, files[i]);
            smm_os_remove(path);
        }
    }
    return 0;
}

int smm_loader_mods_enabled(int kind, const char *game_dir)
{
    char p[SMM_PATH];
    smm_join(p, sizeof p, game_dir, entry_dll(kind));
    return smm_os_is_file(p);
}

int smm_loader_toggle_mods(int kind, const char *game_dir)
{
    char active[SMM_PATH], off[SMM_PATH];
    smm_join(active, sizeof active, game_dir, entry_dll(kind));
    smm_fmt(off, sizeof off, "%s.disabled", active);
    if (smm_os_is_file(active)) {
        smm_os_remove(off);
        return smm_os_rename(active, off);
    }
    if (smm_os_is_file(off)) return smm_os_rename(off, active);
    return -1;
}

int smm_autodetect_game(char *out, size_t n)
{
    const SmmPreset *p = smm_current_preset();
    for (int i = 0; i < p->n_paths; i++)
        if (smm_os_is_file(p->default_paths[i])) {
            smm_copy(out, n, p->default_paths[i]);
            return 1;
        }
    if (p->n_paths > 0 && p->steam_app_id > 0) {
        const char *exe = smm_basename(p->default_paths[0]);
        if (smm_steam_find_game_path(p->steam_app_id, exe, out, n)) return 1;
    }
    return 0;
}
