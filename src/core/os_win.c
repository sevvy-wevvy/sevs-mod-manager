#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "internal.h"
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdlib.h>
#include <wchar.h>

struct SmmProc {
    HANDLE h;
};

static wchar_t *to_wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = smm_xmalloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    for (wchar_t *p = w; *p; p++)
        if (*p == L'/') *p = L'\\';
    return w;
}

static void from_wide(const wchar_t *w, char *out, size_t n)
{
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL);
}

FILE *smm_os_fopen(const char *path, const char *mode)
{
    wchar_t *w = to_wide(path);
    wchar_t m[8];
    size_t i = 0;
    for (; mode[i] && i < 7; i++) m[i] = (wchar_t)mode[i];
    m[i] = 0;
    FILE *f = _wfopen(w, m);
    free(w);
    return f;
}

static DWORD attrs(const char *path)
{
    wchar_t *w = to_wide(path);
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a;
}

int smm_os_exists(const char *path)
{
    return attrs(path) != INVALID_FILE_ATTRIBUTES;
}

int smm_os_is_file(const char *path)
{
    DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int smm_os_is_dir(const char *path)
{
    DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static long long ft_to_unix(FILETIME ft)
{
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (long long)(t / 10000000ULL) - 11644473600LL;
}

long long smm_os_size(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    wchar_t *w = to_wide(path);
    BOOL ok = GetFileAttributesExW(w, GetFileExInfoStandard, &d);
    free(w);
    if (!ok) return -1;
    return ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}

long long smm_os_mtime(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    wchar_t *w = to_wide(path);
    BOOL ok = GetFileAttributesExW(w, GetFileExInfoStandard, &d);
    free(w);
    return ok ? ft_to_unix(d.ftLastWriteTime) : 0;
}

int smm_os_mkdir_p(const char *path)
{
    wchar_t *w = to_wide(path);
    size_t n = wcslen(w);
    while (n > 3 && w[n - 1] == L'\\') w[--n] = 0;
    for (wchar_t *p = w + 3; *p; p++) {
        if (*p != L'\\') continue;
        *p = 0;
        CreateDirectoryW(w, NULL);
        *p = L'\\';
    }
    CreateDirectoryW(w, NULL);
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) ? 0 : -1;
}

int smm_os_remove(const char *path)
{
    wchar_t *w = to_wide(path);
    BOOL ok = DeleteFileW(w);
    if (!ok) ok = RemoveDirectoryW(w);
    free(w);
    return ok ? 0 : -1;
}

void smm_dirlist_free(SmmDirList *l)
{
    free(l->v);
    memset(l, 0, sizeof *l);
}

int smm_os_list(const char *dir, SmmDirList *out)
{
    char pat[SMM_PATH];
    smm_join(pat, sizeof pat, dir, "*");
    wchar_t *w = to_wide(pat);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(w, &fd);
    free(w);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (out->n == out->cap) {
            out->cap = out->cap ? out->cap * 2 : 32;
            out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmDirEntry));
        }
        SmmDirEntry *x = &out->v[out->n++];
        from_wide(fd.cFileName, x->name, sizeof x->name);
        x->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        x->size = ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        x->mtime = ft_to_unix(fd.ftLastWriteTime);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

int smm_os_remove_tree(const char *path)
{
    if (!smm_os_exists(path)) return 0;
    if (!smm_os_is_dir(path)) return smm_os_remove(path);
    SmmDirList l = {0};
    smm_os_list(path, &l);
    for (int i = 0; i < l.n; i++) {
        char full[SMM_PATH];
        smm_join(full, sizeof full, path, l.v[i].name);
        smm_os_remove_tree(full);
    }
    smm_dirlist_free(&l);
    return smm_os_remove(path);
}

int smm_os_rename(const char *from, const char *to)
{
    wchar_t *a = to_wide(from), *b = to_wide(to);
    BOOL ok = MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING);
    free(a);
    free(b);
    return ok ? 0 : -1;
}

int smm_os_copy(const char *from, const char *to)
{
    char dir[SMM_PATH];
    smm_dirname(to, dir, sizeof dir);
    if (dir[0]) smm_os_mkdir_p(dir);
    wchar_t *a = to_wide(from), *b = to_wide(to);
    BOOL ok = CopyFileW(a, b, FALSE);
    free(a);
    free(b);
    return ok ? 0 : -1;
}

int smm_os_copy_tree(const char *from, const char *to)
{
    if (!smm_os_is_dir(from)) return smm_os_copy(from, to);
    smm_os_mkdir_p(to);
    SmmDirList l = {0};
    if (smm_os_list(from, &l) != 0) return -1;
    int rc = 0;
    for (int i = 0; i < l.n; i++) {
        char a[SMM_PATH], b[SMM_PATH];
        smm_join(a, sizeof a, from, l.v[i].name);
        smm_join(b, sizeof b, to, l.v[i].name);
        if (smm_os_copy_tree(a, b) != 0) rc = -1;
    }
    smm_dirlist_free(&l);
    return rc;
}

int smm_os_move_tree(const char *from, const char *to)
{
    if (smm_os_rename(from, to) == 0) return 0;
    if (smm_os_copy_tree(from, to) != 0) return -1;
    return smm_os_remove_tree(from);
}

static void walk(const char *dir, SmmStrs *out)
{
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return;
    for (int i = 0; i < l.n; i++) {
        char full[SMM_PATH];
        smm_join(full, sizeof full, dir, l.v[i].name);
        if (l.v[i].is_dir) walk(full, out);
        else smm_strs_add(out, full);
    }
    smm_dirlist_free(&l);
}

int smm_os_walk_files(const char *dir, SmmStrs *out)
{
    if (!smm_os_is_dir(dir)) return -1;
    walk(dir, out);
    return 0;
}

long long smm_os_tree_size(const char *dir)
{
    SmmStrs f = {0};
    smm_os_walk_files(dir, &f);
    long long t = 0;
    for (int i = 0; i < f.n; i++) {
        long long s = smm_os_size(f.v[i]);
        if (s > 0) t += s;
    }
    smm_strs_free(&f);
    return t;
}

int smm_os_count_ext(const char *dir, const char *ext, int recursive)
{
    int n = 0;
    if (recursive) {
        SmmStrs f = {0};
        smm_os_walk_files(dir, &f);
        for (int i = 0; i < f.n; i++)
            if (smm_ends_with_nocase(f.v[i], ext)) n++;
        smm_strs_free(&f);
    } else {
        SmmDirList l = {0};
        smm_os_list(dir, &l);
        for (int i = 0; i < l.n; i++)
            if (!l.v[i].is_dir && smm_ends_with_nocase(l.v[i].name, ext)) n++;
        smm_dirlist_free(&l);
    }
    return n;
}

const char *smm_os_home(void)
{
    static char home[SMM_PATH];
    if (!home[0]) {
        const char *h = getenv("USERPROFILE");
        smm_copy(home, sizeof home, h ? h : "C:\\");
    }
    return home;
}

void smm_os_config_base(char *out, size_t n)
{
    const char *a = getenv("APPDATA");
    if (a && *a) smm_copy(out, n, a);
    else smm_join(out, n, smm_os_home(), "AppData\\Roaming");
}

void smm_os_temp_dir(char *out, size_t n)
{
    const char *t = getenv("SMM_TMP");
    if (!t || !*t) t = getenv("TEMP");
    smm_copy(out, n, t && *t ? t : "C:\\Windows\\Temp");
}

const char *smm_os_getenv(const char *k)
{
    return getenv(k);
}

int smm_os_setenv(const char *k, const char *v)
{
    return _putenv_s(k, v);
}

static wchar_t *build_cmdline(const char *const *argv)
{
    SmmBuf b = {0};
    smm_cmdline_quote(argv, &b);
    int n = MultiByteToWideChar(CP_UTF8, 0, b.p, -1, NULL, 0);
    wchar_t *w = smm_xmalloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, b.p, -1, w, n);
    smm_buf_free(&b);
    return w;
}

static HANDLE create_proc(const char *const *argv, HANDLE out_h, const char *cwd, int detached)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW | (out_h ? STARTF_USESTDHANDLES : 0);
    si.wShowWindow = SW_HIDE;
    if (out_h) {
        si.hStdOutput = out_h;
        si.hStdError = INVALID_HANDLE_VALUE;
        si.hStdInput = INVALID_HANDLE_VALUE;
    }
    wchar_t *cmd = build_cmdline(argv);
    wchar_t *wd = cwd && *cwd ? to_wide(cwd) : NULL;
    DWORD flags = CREATE_NO_WINDOW | (detached ? DETACHED_PROCESS : 0);
    if (detached) {
        flags = DETACHED_PROCESS;
        si.wShowWindow = SW_SHOWNORMAL;
    }
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, out_h ? TRUE : FALSE, flags, NULL, wd, &si, &pi);
    free(cmd);
    free(wd);
    if (!ok) return NULL;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

int smm_os_run(const char *const *argv, SmmBuf *out, int *exit_code)
{
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE rd, wr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE h = create_proc(argv, wr, NULL, 0);
    CloseHandle(wr);
    if (!h) {
        CloseHandle(rd);
        return -1;
    }
    char buf[65536];
    DWORD r;
    while (ReadFile(rd, buf, sizeof buf, &r, NULL) && r > 0)
        if (out) smm_buf_append(out, buf, r);
    CloseHandle(rd);
    WaitForSingleObject(h, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    if (exit_code) *exit_code = (int)code;
    if (out) smm_bufs_cstr(out);
    return 0;
}

SmmProc *smm_os_spawn(const char *const *argv)
{
    HANDLE h = create_proc(argv, NULL, NULL, 0);
    if (!h) return NULL;
    SmmProc *p = smm_xmalloc(sizeof *p);
    p->h = h;
    return p;
}

int smm_os_proc_running(SmmProc *p, int *exit_code)
{
    DWORD code = 0;
    GetExitCodeProcess(p->h, &code);
    if (code == STILL_ACTIVE) return 1;
    if (exit_code) *exit_code = (int)code;
    return 0;
}

void smm_os_proc_free(SmmProc *p)
{
    CloseHandle(p->h);
    free(p);
}

int smm_os_launch_detached(const char *const *argv, const char *cwd)
{
    HANDLE h = create_proc(argv, NULL, cwd, 1);
    if (!h) return -1;
    CloseHandle(h);
    return 0;
}

int smm_os_process_running(const char *exe_name)
{
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    int found = 0;
    char want[260], have[260];
    smm_fmt(want, sizeof want, "%s", exe_name);
    for (BOOL ok = Process32FirstW(s, &pe); ok && !found; ok = Process32NextW(s, &pe)) {
        from_wide(pe.szExeFile, have, sizeof have);
        char stem[260];
        smm_stem(have, stem, sizeof stem);
        found = smm_strcasecmp(stem, want) == 0 || smm_strcasecmp(have, want) == 0;
    }
    CloseHandle(s);
    return found;
}

int smm_os_process_kill(const char *exe_name)
{
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return -1;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    char have[260];
    for (BOOL ok = Process32FirstW(s, &pe); ok; ok = Process32NextW(s, &pe)) {
        from_wide(pe.szExeFile, have, sizeof have);
        char stem[260];
        smm_stem(have, stem, sizeof stem);
        if (smm_strcasecmp(stem, exe_name) != 0 && smm_strcasecmp(have, exe_name) != 0) continue;
        HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
        if (p) {
            TerminateProcess(p, 1);
            CloseHandle(p);
        }
    }
    CloseHandle(s);
    return 0;
}

int smm_os_open(const char *target)
{
    wchar_t *w = to_wide(target);
    if (strstr(target, "://"))
        for (wchar_t *p = w; *p; p++)
            if (*p == L'\\') *p = L'/';
    HINSTANCE r = ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
    free(w);
    return (INT_PTR)r > 32 ? 0 : -1;
}

int smm_os_is_windows(void)
{
    return 1;
}

int smm_os_is_mac(void)
{
    return 0;
}

long long smm_os_now(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ft_to_unix(ft);
}

void smm_os_sleep(int ms)
{
    Sleep((DWORD)ms);
}

long long smm_os_ms(void)
{
    return (long long)GetTickCount64();
}

typedef struct {
    SmmThreadFn fn;
    void *arg;
} ThreadStart;

static DWORD WINAPI thread_main(LPVOID p)
{
    ThreadStart t = *(ThreadStart *)p;
    free(p);
    t.fn(t.arg);
    return 0;
}

int smm_os_thread(SmmThreadFn fn, void *arg)
{
    ThreadStart *t = smm_xmalloc(sizeof *t);
    t->fn = fn;
    t->arg = arg;
    HANDLE h = CreateThread(NULL, 0, thread_main, t, 0, NULL);
    if (!h) {
        free(t);
        return -1;
    }
    CloseHandle(h);
    return 0;
}

void *smm_os_mutex_new(void)
{
    CRITICAL_SECTION *c = smm_xmalloc(sizeof *c);
    InitializeCriticalSection(c);
    return c;
}

void smm_os_mutex_lock(void *m)
{
    EnterCriticalSection(m);
}

void smm_os_mutex_unlock(void *m)
{
    LeaveCriticalSection(m);
}

void *smm_os_cond_new(void)
{
    CONDITION_VARIABLE *c = smm_xmalloc(sizeof *c);
    InitializeConditionVariable(c);
    return c;
}

void smm_os_cond_wait(void *c, void *m)
{
    SleepConditionVariableCS(c, m, INFINITE);
}

void smm_os_cond_signal(void *c)
{
    WakeConditionVariable(c);
}

void smm_os_cond_broadcast(void *c)
{
    WakeAllConditionVariable(c);
}

int smm_os_registry_steam(char *out, size_t n)
{
    HKEY k;
    char buf[SMM_PATH];
    DWORD sz = sizeof buf, type = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "SteamPath", NULL, &type, (BYTE *)buf, &sz) == ERROR_SUCCESS && type == REG_SZ) {
            RegCloseKey(k);
            smm_copy(out, n, buf);
            for (char *p = out; *p; p++)
                if (*p == '/') *p = '\\';
            return 0;
        }
        RegCloseKey(k);
    }
    sz = sizeof buf;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS ||
        RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "InstallPath", NULL, &type, (BYTE *)buf, &sz) == ERROR_SUCCESS && type == REG_SZ) {
            RegCloseKey(k);
            smm_copy(out, n, buf);
            return 0;
        }
        RegCloseKey(k);
    }
    return -1;
}
#endif
