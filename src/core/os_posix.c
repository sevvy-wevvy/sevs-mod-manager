#if !defined(_WIN32)
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#define SMM_MAC 1
#else
#define SMM_MAC 0
#endif

struct SmmProc {
    pid_t pid;
};

FILE *smm_os_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int smm_os_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int smm_os_is_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int smm_os_is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

long long smm_os_size(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

long long smm_os_mtime(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long long)st.st_mtime : 0;
}

int smm_os_mkdir_p(const char *path)
{
    char tmp[SMM_PATH];
    smm_copy(tmp, sizeof tmp, path);
    size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
        *p = '/';
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int smm_os_remove(const char *path)
{
    return remove(path) == 0 ? 0 : -1;
}

void smm_dirlist_free(SmmDirList *l)
{
    free(l->v);
    memset(l, 0, sizeof *l);
}

int smm_os_list(const char *dir, SmmDirList *out)
{
    DIR *d = opendir(dir);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (out->n == out->cap) {
            out->cap = out->cap ? out->cap * 2 : 32;
            out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmDirEntry));
        }
        SmmDirEntry *x = &out->v[out->n++];
        smm_copy(x->name, sizeof x->name, e->d_name);
        char full[SMM_PATH];
        smm_join(full, sizeof full, dir, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            x->is_dir = S_ISDIR(st.st_mode);
            x->size = (long long)st.st_size;
            x->mtime = (long long)st.st_mtime;
        } else {
            x->is_dir = 0;
            x->size = 0;
            x->mtime = 0;
        }
    }
    closedir(d);
    return 0;
}

int smm_os_remove_tree(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return remove(path) == 0 ? 0 : -1;
    SmmDirList l = {0};
    smm_os_list(path, &l);
    for (int i = 0; i < l.n; i++) {
        char full[SMM_PATH];
        smm_join(full, sizeof full, path, l.v[i].name);
        smm_os_remove_tree(full);
    }
    smm_dirlist_free(&l);
    return rmdir(path) == 0 ? 0 : -1;
}

int smm_os_rename(const char *from, const char *to)
{
    return rename(from, to) == 0 ? 0 : -1;
}

int smm_os_copy(const char *from, const char *to)
{
    struct stat sa, sb;
    if (stat(from, &sa) == 0 && stat(to, &sb) == 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino) return 0;
    FILE *a = fopen(from, "rb");
    if (!a) return -1;
    char dir[SMM_PATH];
    smm_dirname(to, dir, sizeof dir);
    if (dir[0]) smm_os_mkdir_p(dir);
    FILE *b = fopen(to, "wb");
    if (!b) {
        fclose(a);
        return -1;
    }
    char buf[65536];
    size_t r;
    int ok = 1;
    while ((r = fread(buf, 1, sizeof buf, a)) > 0)
        if (fwrite(buf, 1, r, b) != r) ok = 0;
    fclose(a);
    if (fclose(b) != 0) ok = 0;
    return ok ? 0 : -1;
}

static int copy_tree(const char *from, const char *to, int depth)
{
    if (!smm_os_is_dir(from)) return smm_os_copy(from, to);
    if (depth > 40) return -1;
    smm_os_mkdir_p(to);
    SmmDirList l = {0};
    if (smm_os_list(from, &l) != 0) return -1;
    int rc = 0;
    for (int i = 0; i < l.n; i++) {
        char a[SMM_PATH], b[SMM_PATH];
        smm_join(a, sizeof a, from, l.v[i].name);
        smm_join(b, sizeof b, to, l.v[i].name);
        if (copy_tree(a, b, depth + 1) != 0) rc = -1;
    }
    smm_dirlist_free(&l);
    return rc;
}

int smm_os_copy_tree(const char *from, const char *to)
{
    return copy_tree(from, to, 0);
}

int smm_os_move_tree(const char *from, const char *to)
{
    if (rename(from, to) == 0) return 0;
    if (smm_os_copy_tree(from, to) != 0) return -1;
    return smm_os_remove_tree(from);
}

static void walk(const char *dir, SmmStrs *out, int depth)
{
    if (depth > 40) return;
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return;
    for (int i = 0; i < l.n; i++) {
        char full[SMM_PATH];
        smm_join(full, sizeof full, dir, l.v[i].name);
        if (l.v[i].is_dir) walk(full, out, depth + 1);
        else smm_strs_add(out, full);
    }
    smm_dirlist_free(&l);
}

int smm_os_walk_files(const char *dir, SmmStrs *out)
{
    if (!smm_os_is_dir(dir)) return -1;
    walk(dir, out, 0);
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
    const char *h = getenv("HOME");
    return h && *h ? h : "/";
}

void smm_os_config_base(char *out, size_t n)
{
    const char *x = getenv("XDG_CONFIG_HOME");
    if (SMM_MAC) smm_join(out, n, smm_os_home(), "Library/Application Support");
    else if (x && *x) smm_copy(out, n, x);
    else smm_join(out, n, smm_os_home(), ".config");
}

void smm_os_temp_dir(char *out, size_t n)
{
    const char *t = getenv("SMM_TMP");
    if (!t || !*t) t = getenv("TMPDIR");
    smm_copy(out, n, t && *t ? t : "/tmp");
}

const char *smm_os_getenv(const char *k)
{
    return getenv(k);
}

int smm_os_setenv(const char *k, const char *v)
{
    return setenv(k, v, 1);
}

static pid_t spawn_pid(const char *const *argv, int out_fd, const char *cwd)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int dn = open("/dev/null", O_RDWR);
        if (out_fd >= 0) dup2(out_fd, 1);
        else if (dn >= 0) dup2(dn, 1);
        if (dn >= 0) {
            dup2(dn, 2);
            dup2(dn, 0);
        }
        if (cwd && *cwd && chdir(cwd) != 0) _exit(127);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    return pid;
}

int smm_os_run(const char *const *argv, SmmBuf *out, int *exit_code)
{
    int fds[2];
    if (pipe(fds) != 0) return -1;
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    pid_t pid = spawn_pid(argv, fds[1], NULL);
    close(fds[1]);
    if (pid < 0) {
        close(fds[0]);
        return -1;
    }
    char buf[65536];
    ssize_t r;
    while ((r = read(fds[0], buf, sizeof buf)) > 0 || (r < 0 && errno == EINTR))
        if (r > 0 && out) smm_buf_append(out, buf, (size_t)r);
    close(fds[0]);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    int code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    if (exit_code) *exit_code = code;
    if (out) smm_bufs_cstr(out);
    return code == 127 ? -1 : 0;
}

SmmProc *smm_os_spawn(const char *const *argv)
{
    pid_t pid = spawn_pid(argv, -1, NULL);
    if (pid < 0) return NULL;
    SmmProc *p = smm_xmalloc(sizeof *p);
    p->pid = pid;
    return p;
}

int smm_os_proc_running(SmmProc *p, int *exit_code)
{
    int st;
    pid_t r = waitpid(p->pid, &st, WNOHANG);
    if (r == 0) return 1;
    if (exit_code) *exit_code = (r > 0 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
    return 0;
}

void smm_os_proc_free(SmmProc *p)
{
    int st;
    if (waitpid(p->pid, &st, WNOHANG) == 0) {
        kill(p->pid, SIGKILL);
        waitpid(p->pid, &st, 0);
    }
    free(p);
}

int smm_os_launch_detached(const char *const *argv, const char *cwd)
{
    int ef[2];
    if (pipe(ef) != 0) return -1;
    fcntl(ef[0], F_SETFD, FD_CLOEXEC);
    fcntl(ef[1], F_SETFD, FD_CLOEXEC);
    pid_t pid = fork();
    if (pid < 0) {
        close(ef[0]);
        close(ef[1]);
        return -1;
    }
    if (pid == 0) {
        close(ef[0]);
        setsid();
        pid_t pid2 = fork();
        if (pid2 != 0) _exit(0);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, 0);
            dup2(dn, 1);
            dup2(dn, 2);
        }
        if (!(cwd && *cwd && chdir(cwd) != 0)) execvp(argv[0], (char *const *)argv);
        char fail = 1;
        if (write(ef[1], &fail, 1) < 0) fail = 0;
        _exit(127);
    }
    close(ef[1]);
    int st;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    char got = 0;
    ssize_t r;
    while ((r = read(ef[0], &got, 1)) < 0 && errno == EINTR) {
    }
    close(ef[0]);
    if (r > 0) {
        smm_set_error("Couldn't start %s", argv[0]);
        return -1;
    }
    return 0;
}

static void regex_escape(const char *s, char *out, size_t n)
{
    size_t k = 0;
    for (; *s && k + 2 < n; s++) {
        if (strchr(".^$*+?()[]{}|\\", *s)) out[k++] = '\\';
        out[k++] = *s;
    }
    out[k] = 0;
}

int smm_os_process_running(const char *exe_name)
{
    if (SMM_MAC) {
        char pat[SMM_PATH * 2];
        regex_escape(exe_name, pat, sizeof pat);
        const char *argv[] = {"pgrep", "-f", pat, NULL};
        int code = 1;
        smm_os_run(argv, NULL, &code);
        return code == 0;
    }
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    pid_t self = getpid();
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        if (atoi(e->d_name) == (int)self) continue;
        char path[64];
        smm_fmt(path, sizeof path, "/proc/%s/cmdline", e->d_name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        char buf[4096];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = 0;
        for (size_t i = 0; i < n && !found;) {
            const char *arg = buf + i;
            if (smm_strcasecmp(smm_basename(arg), exe_name) == 0) found = 1;
            i += strlen(arg) + 1;
        }
    }
    closedir(d);
    return found;
}

int smm_os_process_kill(const char *exe_name)
{
    if (SMM_MAC) {
        char pat[SMM_PATH * 2];
        regex_escape(exe_name, pat, sizeof pat);
        const char *argv[] = {"pkill", "-f", pat, NULL};
        return smm_os_run(argv, NULL, NULL);
    }
    DIR *d = opendir("/proc");
    if (!d) return -1;
    struct dirent *e;
    pid_t self = getpid();
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        int pid = atoi(e->d_name);
        if (pid == (int)self) continue;
        char path[64];
        smm_fmt(path, sizeof path, "/proc/%s/cmdline", e->d_name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        char buf[4096];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = 0;
        for (size_t i = 0; i < n;) {
            const char *arg = buf + i;
            if (smm_strcasecmp(smm_basename(arg), exe_name) == 0) {
                kill(pid, SIGTERM);
                break;
            }
            i += strlen(arg) + 1;
        }
    }
    closedir(d);
    return 0;
}

int smm_os_open(const char *target)
{
    const char *argv[] = {SMM_MAC ? "open" : "xdg-open", target, NULL};
    return smm_os_launch_detached(argv, NULL);
}

int smm_os_is_windows(void)
{
    return 0;
}

int smm_os_is_mac(void)
{
    return SMM_MAC;
}

long long smm_os_now(void)
{
    return (long long)time(NULL);
}

void smm_os_sleep(int ms)
{
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

long long smm_os_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

typedef struct {
    SmmThreadFn fn;
    void *arg;
} ThreadStart;

static void *thread_main(void *p)
{
    ThreadStart t = *(ThreadStart *)p;
    free(p);
    t.fn(t.arg);
    return NULL;
}

int smm_os_realpath(const char *path, char *out, size_t n)
{
    char *r = realpath(path, NULL);
    if (!r) return -1;
    smm_copy(out, n, r);
    free(r);
    return 0;
}

int smm_os_copy_mode(const char *from, const char *to)
{
    struct stat st;
    if (stat(from, &st) != 0) return -1;
    return chmod(to, st.st_mode & 07777);
}

int smm_os_thread(SmmThreadFn fn, void *arg)
{
    ThreadStart *t = smm_xmalloc(sizeof *t);
    t->fn = fn;
    t->arg = arg;
    pthread_t th;
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&th, &at, thread_main, t);
    pthread_attr_destroy(&at);
    if (rc != 0) {
        free(t);
        return -1;
    }
    return 0;
}

void *smm_os_mutex_new(void)
{
    pthread_mutex_t *m = smm_xmalloc(sizeof *m);
    pthread_mutex_init(m, NULL);
    return m;
}

void smm_os_mutex_lock(void *m)
{
    pthread_mutex_lock(m);
}

void smm_os_mutex_unlock(void *m)
{
    pthread_mutex_unlock(m);
}

void *smm_os_cond_new(void)
{
    pthread_cond_t *c = smm_xmalloc(sizeof *c);
    pthread_cond_init(c, NULL);
    return c;
}

void smm_os_cond_wait(void *c, void *m)
{
    pthread_cond_wait(c, m);
}

void smm_os_cond_signal(void *c)
{
    pthread_cond_signal(c);
}

void smm_os_cond_broadcast(void *c)
{
    pthread_cond_broadcast(c);
}

int smm_os_registry_steam(char *out, size_t n)
{
    (void)out;
    (void)n;
    return -1;
}

void smm_os_prepare_args(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
}
#endif
