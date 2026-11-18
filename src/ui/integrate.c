#if !defined(_WIN32) && !defined(__APPLE__)
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "integrate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/internal.h"
#include "appicon_data.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <stdint.h>
int _NSGetExecutablePath(char *buf, uint32_t *size);
#else
#include <limits.h>
#include <unistd.h>
#endif

int integ_write(const char *path, const void *data, size_t n)
{
    char dir[SMM_PATH];
    smm_dirname(path, dir, sizeof dir);
    if (smm_os_mkdir_p(dir) != 0 && !smm_os_is_dir(dir)) return -1;
    return smm_file_write_atomic(path, data, n);
}

int integ_run_code(const char *a, const char *b, const char *c, const char *d, const char *e)
{
    const char *argv[6] = {a, b, c, d, e, NULL};
    int code = -1;
    if (smm_os_run(argv, NULL, &code) != 0) return -1;
    return code;
}

void integ_run_quiet(const char *a, const char *b, const char *c, const char *d, const char *e)
{
    integ_run_code(a, b, c, d, e);
}

int integrate_exe_path(char *out, size_t n)
{
#if defined(_WIN32)
    wchar_t w[SMM_PATH];
    DWORD got = GetModuleFileNameW(NULL, w, SMM_PATH);
    if (got == 0 || got >= SMM_PATH) return -1;
    return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL) > 0 ? 0 : -1;
#elif defined(__APPLE__)
    char raw[SMM_PATH], real[PATH_MAX];
    uint32_t sz = sizeof raw;
    if (_NSGetExecutablePath(raw, &sz) != 0) return -1;
    smm_copy(out, n, realpath(raw, real) ? real : raw);
    return 0;
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (len <= 0) return -1;
    buf[len] = 0;
    if (smm_ends_with(buf, " (deleted)")) buf[len - 10] = 0;
    smm_copy(out, n, buf);
    return 0;
#endif
}

int integrate_register(void)
{
    char exe[SMM_PATH];
    if (integrate_exe_path(exe, sizeof exe) != 0) return -1;
    return integ_os_register(exe, appicon_png, sizeof appicon_png);
}

int integrate_unregister(void)
{
    return integ_os_unregister();
}

static void auto_thread(void *p)
{
    (void)p;
    if (integrate_register() != 0) smm_settings.registered_exe[0] = 0;
}

void integrate_auto(void)
{
    char exe[SMM_PATH];
    if (smm_demo_enabled()) return;
    if (integrate_exe_path(exe, sizeof exe) != 0) return;
    if (strcmp(exe, smm_settings.registered_exe) == 0) return;
    smm_copy(smm_settings.registered_exe, sizeof smm_settings.registered_exe, exe);
    if (smm_os_thread(auto_thread, NULL) != 0) smm_settings.registered_exe[0] = 0;
}

int integrate_cli(const char *arg)
{
    int reg = strcmp(arg, "--register") == 0;
    if (!reg && strcmp(arg, "--unregister") != 0) return -1;
    smm_state_init();
    smm_state_load();
    int rc = reg ? integrate_register() : integrate_unregister();
    if (rc == 0) {
        if (reg) integrate_exe_path(smm_settings.registered_exe, sizeof smm_settings.registered_exe);
        else smm_settings.registered_exe[0] = 0;
        smm_state_save();
        printf(reg ? "registered with the system\n" : "removed from the system\n");
        return 0;
    }
    fprintf(stderr, "%s failed\n", reg ? "register" : "unregister");
    return 1;
}
