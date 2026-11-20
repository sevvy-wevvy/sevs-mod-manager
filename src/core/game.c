#include "internal.h"
#include <stdlib.h>

int smm_game_steam_appid(void)
{
    const SmmPreset *p = smm_current_preset();
    if (p->steam_app_id > 0) return p->steam_app_id;
    const SmmMap *m = &smm_settings.steam_exe_paths;
    for (int i = 0; i < m->n; i++)
        if (smm_strcasecmp(m->v[i], smm_settings.game_path) == 0) return atoi(m->k[i]);
    return 0;
}

int smm_is_linux_proton(void)
{
    return !smm_os_is_windows() && !smm_os_is_mac() && smm_ends_with_nocase(smm_settings.game_path, ".exe");
}

const char *smm_loader_dll_override(int kind)
{
    return kind == SMM_LOADER_MELON ? "version=n,b" : "winhttp=n,b";
}

const char *smm_loader_proxy_dll(int kind)
{
    return kind == SMM_LOADER_MELON ? "version" : "winhttp";
}

void smm_launch_option_hint(int kind, char *out, size_t n)
{
    smm_fmt(out, n, "WINEDLLOVERRIDES=\"%s\" %%command%%", smm_loader_dll_override(kind));
}

int smm_open_path(const char *path)
{
    if (smm_demo_enabled()) return 0;
    return smm_os_open(path);
}

int smm_open_url(const char *url)
{
    if (smm_demo_enabled()) return 0;
    return smm_os_open(url);
}

int smm_game_launch(void)
{
    const char *path = smm_settings.game_path;
    if (smm_demo_enabled()) {
        smm_demo_running_flag(1, 1);
        return 0;
    }
    if (!path[0] || !smm_os_is_file(path)) {
        smm_set_error("The game executable wasn't found.");
        return -1;
    }
    int appid = smm_game_steam_appid();
    if (appid > 0) {
        char url[64];
        smm_fmt(url, sizeof url, "steam://run/%d", appid);
        return smm_os_open(url);
    }
    char dir[SMM_PATH];
    smm_dirname(path, dir, sizeof dir);
    if (smm_is_linux_proton()) {
        smm_os_setenv("WINEDLLOVERRIDES", smm_loader_dll_override(smm_detect_loader()));
        const char *argv[] = {"wine", path, NULL};
        return smm_os_launch_detached(argv, dir);
    }
    const char *argv[] = {path, NULL};
    return smm_os_launch_detached(argv, dir);
}

int smm_game_running(void)
{
    if (smm_demo_enabled()) return smm_demo_running_flag(0, 0);
    if (!smm_settings.game_path[0]) return 0;
    return smm_os_process_running(smm_basename(smm_settings.game_path));
}

int smm_game_stop(void)
{
    if (smm_demo_enabled()) {
        smm_demo_running_flag(1, 0);
        return 0;
    }
    if (!smm_settings.game_path[0]) return -1;
    return smm_os_process_kill(smm_basename(smm_settings.game_path));
}
