#define _DEFAULT_SOURCE
#include "internal.h"
#include <stdlib.h>
#include <time.h>

int smm_log_path(char *out, size_t n)
{
    char g[SMM_PATH], d[SMM_PATH];
    if (!smm_game_dir(g, sizeof g)) return 0;
    if (smm_detect_loader() == SMM_LOADER_MELON) {
        smm_join(d, sizeof d, g, "MelonLoader");
        smm_join(out, n, d, "Latest.log");
    } else {
        smm_join(d, sizeof d, g, "BepInEx");
        smm_join(out, n, d, "LogOutput.log");
    }
    return 1;
}

int smm_log_read_lines(const char *path, SmmStrs *out)
{
    return smm_read_lines(path, out);
}

int smm_log_read_from(const char *path, long long *offset, SmmStrs *out)
{
    FILE *f = smm_os_fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long long size = ftell(f);
    if (*offset > size) *offset = 0;
    fseek(f, (long)*offset, SEEK_SET);
    SmmBuf b = {0};
    char tmp[65536];
    size_t r;
    while ((r = fread(tmp, 1, sizeof tmp, f)) > 0) smm_buf_append(&b, tmp, r);
    fclose(f);
    size_t last = b.n;
    while (last > 0 && b.p[last - 1] != '\n') last--;
    if (last > 0) {
        smm_split_lines(b.p, last, out);
        *offset += (long long)last;
    }
    smm_buf_free(&b);
    return out->n;
}

int smm_log_line_level(const char *line)
{
    if (smm_stristr(line, "Exception") || smm_stristr(line, "Error") || smm_stristr(line, "Fatal")) return 2;
    if (smm_stristr(line, "Warn")) return 1;
    return 0;
}

int smm_log_history_dir(char *out, size_t n)
{
    char sb[SMM_PATH];
    if (!smm_sb_dir(sb, sizeof sb)) return 0;
    smm_join(out, n, sb, "LogHistory");
    return 1;
}

static void stamp_name(long long t, char *out, size_t n)
{
    time_t tt = (time_t)t;
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    strftime(out, n, "%Y-%m-%d_%H-%M-%S", &tmv);
}

static void pretty_name(long long t, char *out, size_t n)
{
    time_t tt = (time_t)t;
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    static const char *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int h = tmv.tm_hour % 12;
    if (h == 0) h = 12;
    smm_fmt(out, n, "%s %d, %d %d:%02d %s", mon[tmv.tm_mon], tmv.tm_mday, tmv.tm_year + 1900, h, tmv.tm_min, tmv.tm_hour >= 12 ? "PM" : "AM");
}

static int cmp_sessions(const void *a, const void *b)
{
    long long x = ((const SmmLogSession *)a)->timestamp, y = ((const SmmLogSession *)b)->timestamp;
    return x < y ? 1 : x > y ? -1 : 0;
}

int smm_log_history_list(SmmLogSessions *out)
{
    char dir[SMM_PATH];
    if (!smm_log_history_dir(dir, sizeof dir)) return 0;
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return 0;
    for (int i = 0; i < l.n; i++) {
        if (l.v[i].is_dir || !smm_ends_with_nocase(l.v[i].name, ".log")) continue;
        struct tm tmv;
        memset(&tmv, 0, sizeof tmv);
        int y, mo, d, h, mi, s;
        if (sscanf(l.v[i].name, "%d-%d-%d_%d-%d-%d.log", &y, &mo, &d, &h, &mi, &s) != 6) continue;
        tmv.tm_year = y - 1900;
        tmv.tm_mon = mo - 1;
        tmv.tm_mday = d;
        tmv.tm_hour = h;
        tmv.tm_min = mi;
        tmv.tm_sec = s;
        tmv.tm_isdst = -1;
        if (out->n == out->cap) {
            out->cap = out->cap ? out->cap * 2 : 16;
            out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmLogSession));
        }
        SmmLogSession *x = &out->v[out->n++];
        smm_join(x->path, sizeof x->path, dir, l.v[i].name);
        x->timestamp = (long long)mktime(&tmv);
        pretty_name(x->timestamp, x->display_name, sizeof x->display_name);
    }
    smm_dirlist_free(&l);
    if (out->n > 1) qsort(out->v, (size_t)out->n, sizeof(SmmLogSession), cmp_sessions);
    return out->n;
}

int smm_log_history_archive(long long started_at, const SmmStrs *lines)
{
    char dir[SMM_PATH];
    if (!smm_log_history_dir(dir, sizeof dir) || lines->n == 0) return 0;
    smm_os_mkdir_p(dir);
    char name[64], fn[96], path[SMM_PATH];
    stamp_name(started_at, name, sizeof name);
    smm_fmt(fn, sizeof fn, "%s.log", name);
    smm_join(path, sizeof path, dir, fn);
    SmmBuf b = {0};
    for (int i = 0; i < lines->n; i++) {
        smm_buf_append(&b, lines->v[i], strlen(lines->v[i]));
        smm_buf_append(&b, "\n", 1);
    }
    smm_bufs_cstr(&b);
    int rc = smm_file_write(path, b.p, b.n);
    smm_buf_free(&b);
    if (rc != 0) return -1;
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) == 0) {
        for (;;) {
            int logs = 0, oldest = -1;
            for (int i = 0; i < l.n; i++) {
                if (l.v[i].is_dir || !smm_ends_with_nocase(l.v[i].name, ".log")) continue;
                logs++;
                if (oldest < 0 || l.v[i].mtime < l.v[oldest].mtime) oldest = i;
            }
            if (logs <= 10 || oldest < 0) break;
            char victim[SMM_PATH];
            smm_join(victim, sizeof victim, dir, l.v[oldest].name);
            smm_os_remove(victim);
            l.v[oldest].is_dir = 1;
        }
    }
    smm_dirlist_free(&l);
    return 0;
}

int smm_crash_analyze(const SmmStrs *lines, SmmCrashSuspect *out)
{
    SmmInstalledList inst = {0};
    smm_installed_scan(&inst);
    int found = 0;
    if (inst.n > 0) {
        int at = -1;
        for (int i = 0; i < lines->n; i++)
            if (smm_stristr(lines->v[i], "Exception") || smm_stristr(lines->v[i], "Error") || smm_stristr(lines->v[i], "Fatal")) {
                at = i;
                break;
            }
        if (at >= 0) {
            int end = lines->n < at + 40 ? lines->n : at + 40;
            for (int i = at; i < end && !found; i++) {
                for (int k = 0; k < inst.n && !found; k++) {
                    if (strlen(inst.v[k].name) < 3) continue;
                    const char *dash = strchr(inst.v[k].name, '-');
                    int hit = smm_stristr(lines->v[i], inst.v[k].name) != NULL;
                    if (!hit && dash && strlen(dash + 1) >= 3) hit = smm_stristr(lines->v[i], dash + 1) != NULL;
                    if (!hit) continue;
                    smm_copy(out->mod_name, sizeof out->mod_name, inst.v[k].name);
                    smm_fmt(out->reason, sizeof out->reason,
                            "\"%s\" appears in the stack trace near a reported error/exception. This is a best-effort guess, not a certainty.",
                            inst.v[k].name);
                    found = 1;
                }
            }
        }
    }
    smm_installed_free(&inst);
    return found;
}

static const char *os_name(void)
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Linux";
#endif
}

int smm_bug_report_export(const char *dest, const SmmStrs *lines)
{
    SmmBuf log = {0}, info = {0}, mods = {0};
    for (int i = 0; i < lines->n; i++) {
        smm_buf_append(&log, lines->v[i], strlen(lines->v[i]));
        smm_buf_append(&log, "\n", 1);
    }
    char when[40];
    smm_iso_time(smm_os_now(), when, sizeof when);
    int loader = smm_detect_loader();
    char gd[SMM_PATH];
    int have = smm_game_dir(gd, sizeof gd);
    smm_buf_printf(&info, "Sev's Mod Manager Bug Report\nGenerated: %s\nApp build: %d\nOS: %s\nGame: %s\nGame path: %s\nMod loader: %s (%s)\n",
                   when, SMM_VERSION, os_name(), smm_settings.game_name, smm_settings.game_path, smm_loader_name(loader),
                   have && smm_loader_installed(loader, gd) ? "installed" : "not found");
    SmmInstalledList inst = {0};
    smm_installed_scan(&inst);
    if (inst.n == 0) smm_buf_append(&mods, "(no mods installed)", 19);
    for (int i = 0; i < inst.n; i++) {
        smm_buf_printf(&mods, "%s - %s", inst.v[i].name, inst.v[i].enabled ? "enabled" : "disabled");
        if (inst.v[i].known_version[0]) smm_buf_printf(&mods, " - v%s", inst.v[i].known_version);
        smm_buf_append(&mods, "\n", 1);
    }
    smm_installed_free(&inst);
    smm_os_remove(dest);
    SmmZipWriter w;
    int rc = smm_zipw_open(&w, dest);
    if (rc == 0) {
        rc = smm_zipw_add_mem(&w, "log.txt", log.p ? log.p : "", log.n);
        if (rc == 0) rc = smm_zipw_add_mem(&w, "system-info.txt", info.p, info.n);
        if (rc == 0) rc = smm_zipw_add_mem(&w, "installed-mods.txt", mods.p, mods.n);
        if (smm_zipw_close(&w) != 0) rc = -1;
    }
    smm_buf_free(&log);
    smm_buf_free(&info);
    smm_buf_free(&mods);
    return rc;
}
