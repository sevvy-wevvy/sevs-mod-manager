#include "internal.h"
#include <stdlib.h>

#define MAX_HISTORY 25

typedef struct {
    SmmOp op;
    SmmProgress prog;
    SmmWorkFn fn;
    void *arg;
    SmmDoneFn done;
    unsigned ticket;
} Job;

static void *g_lock;
static void *g_gate;
static void *g_gate_cond;
static unsigned g_serving;
static unsigned g_next_ticket;
static SmmOp g_hist[MAX_HISTORY];
static int g_hist_n;
static int g_next_id = 1;
static unsigned g_changed;
static long long g_last_notify;

static void ensure_init(void)
{
    if (!g_lock) {
        g_lock = smm_os_mutex_new();
        g_gate = smm_os_mutex_new();
        g_gate_cond = smm_os_cond_new();
    }
}

long long smm_now(void)
{
    return smm_os_now();
}

void smm_sleep_ms(int ms)
{
    smm_os_sleep(ms);
}

int smm_thread_start(SmmThreadFn fn, void *arg)
{
    return smm_os_thread(fn, arg);
}

static SmmOp *find_op(int id)
{
    for (int i = 0; i < g_hist_n; i++)
        if (g_hist[i].id == id) return &g_hist[i];
    return NULL;
}

void smm_progress(SmmProgress *p, int percent, const char *fmt, ...)
{
    if (!p) return;
    char text[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    smm_os_mutex_lock(g_lock);
    SmmOp *op = find_op(p->op_id);
    if (op) {
        op->percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
        smm_copy(op->status_text, sizeof op->status_text, text);
        long long now = smm_os_ms();
        if (now - g_last_notify >= 100 || percent >= 100) {
            g_last_notify = now;
            g_changed++;
        }
    }
    smm_os_mutex_unlock(g_lock);
}

int smm_op_fail(SmmProgress *p, const char *fmt, ...)
{
    char text[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    smm_set_error("%s", text);
    (void)p;
    return -1;
}

static void worker(void *arg)
{
    Job *job = arg;
    smm_os_mutex_lock(g_gate);
    while (job->ticket != g_serving) smm_os_cond_wait(g_gate_cond, g_gate);
    smm_os_mutex_unlock(g_gate);
    smm_os_mutex_lock(g_lock);
    SmmOp *op = find_op(job->prog.op_id);
    if (op) op->status = SMM_OP_RUNNING;
    g_changed++;
    smm_os_mutex_unlock(g_lock);
    smm_set_error("");
    int rc = job->fn(&job->prog, job->arg);
    char err[256];
    smm_copy(err, sizeof err, smm_error());
    smm_os_mutex_lock(g_lock);
    op = find_op(job->prog.op_id);
    if (op) {
        op->status = rc == 0 ? SMM_OP_DONE : SMM_OP_FAILED;
        if (rc == 0) op->percent = 100;
        else smm_copy(op->error, sizeof op->error, err[0] ? err : "Failed");
        op->finished_at = smm_os_now();
    }
    g_changed++;
    smm_os_mutex_unlock(g_lock);
    smm_os_mutex_lock(g_gate);
    g_serving++;
    smm_os_cond_broadcast(g_gate_cond);
    smm_os_mutex_unlock(g_gate);
    if (job->done) job->done(rc, job->arg);
    free(job);
}

int smm_queue_run(const char *name, SmmWorkFn fn, void *arg, SmmDoneFn done)
{
    ensure_init();
    Job *job = smm_xmalloc(sizeof *job);
    memset(job, 0, sizeof *job);
    job->fn = fn;
    job->arg = arg;
    job->done = done;
    smm_os_mutex_lock(g_lock);
    int id = g_next_id++;
    job->prog.op_id = id;
    job->ticket = g_next_ticket++;
    if (g_hist_n == MAX_HISTORY) {
        memmove(g_hist + 1, g_hist, (MAX_HISTORY - 1) * sizeof(SmmOp));
    } else {
        memmove(g_hist + 1, g_hist, (size_t)g_hist_n * sizeof(SmmOp));
        g_hist_n++;
    }
    memset(&g_hist[0], 0, sizeof g_hist[0]);
    g_hist[0].id = id;
    smm_copy(g_hist[0].name, sizeof g_hist[0].name, name);
    g_hist[0].status = SMM_OP_QUEUED;
    g_hist[0].queued_at = smm_os_now();
    g_changed++;
    smm_os_mutex_unlock(g_lock);
    if (smm_os_thread(worker, job) != 0) {
        smm_os_mutex_lock(g_lock);
        SmmOp *op = find_op(id);
        if (op) {
            op->status = SMM_OP_FAILED;
            smm_copy(op->error, sizeof op->error, "Couldn't start a worker thread");
        }
        smm_os_mutex_unlock(g_lock);
        free(job);
        return -1;
    }
    return id;
}

int smm_queue_snapshot(SmmOp *out, int max)
{
    ensure_init();
    smm_os_mutex_lock(g_lock);
    int n = g_hist_n < max ? g_hist_n : max;
    memcpy(out, g_hist, (size_t)n * sizeof(SmmOp));
    smm_os_mutex_unlock(g_lock);
    return n;
}

unsigned smm_queue_changed(void)
{
    ensure_init();
    smm_os_mutex_lock(g_lock);
    unsigned c = g_changed;
    smm_os_mutex_unlock(g_lock);
    return c;
}

int smm_queue_current(SmmOp *out)
{
    ensure_init();
    int found = 0;
    smm_os_mutex_lock(g_lock);
    for (int i = 0; i < g_hist_n; i++)
        if (g_hist[i].status == SMM_OP_RUNNING) {
            *out = g_hist[i];
            found = 1;
            break;
        }
    smm_os_mutex_unlock(g_lock);
    return found;
}

int smm_queue_busy(void)
{
    ensure_init();
    int busy = 0;
    smm_os_mutex_lock(g_lock);
    for (int i = 0; i < g_hist_n; i++)
        if (g_hist[i].status == SMM_OP_QUEUED || g_hist[i].status == SMM_OP_RUNNING) busy = 1;
    smm_os_mutex_unlock(g_lock);
    return busy;
}

typedef struct {
    SmmMod mod;
} InstallArg;

static int work_install_mod(SmmProgress *p, void *arg)
{
    InstallArg *a = arg;
    return smm_mod_install(&a->mod, p);
}

static void done_install_mod(int rc, void *arg)
{
    (void)rc;
    InstallArg *a = arg;
    smm_strs_free(&a->mod.deps);
    free(a);
}

int smm_queue_install_mod(const SmmMod *m)
{
    InstallArg *a = smm_xmalloc(sizeof *a);
    a->mod = *m;
    memset(&a->mod.deps, 0, sizeof a->mod.deps);
    char name[200];
    smm_fmt(name, sizeof name, "Install %s", m->name);
    return smm_queue_run(name, work_install_mod, a, done_install_mod);
}

static int work_loader(SmmProgress *p, void *arg)
{
    int kind = (int)(intptr_t)arg;
    char dir[SMM_PATH];
    if (!smm_game_dir(dir, sizeof dir)) return smm_op_fail(p, "No game path set.");
    return smm_loader_install(kind, dir, p);
}

int smm_queue_install_loader(int kind)
{
    char name[100];
    smm_fmt(name, sizeof name, "Install %s", smm_loader_name(kind));
    return smm_queue_run(name, work_loader, (void *)(intptr_t)kind, NULL);
}

static int work_apply(SmmProgress *p, void *arg)
{
    return smm_pack_apply(arg, p);
}

static void done_free(int rc, void *arg)
{
    (void)rc;
    free(arg);
}

int smm_queue_apply_pack(const char *pack_path)
{
    char name[200];
    smm_fmt(name, sizeof name, "Apply %s", smm_basename(pack_path));
    return smm_queue_run(name, work_apply, smm_xstrdup(pack_path), done_free);
}

static int work_blank(SmmProgress *p, void *arg)
{
    (void)arg;
    return smm_pack_blank_setup(p);
}

int smm_queue_blank_setup(void)
{
    return smm_queue_run("Create blank setup", work_blank, NULL, NULL);
}

typedef struct {
    char name[128], author[128];
    SmmStrs only;
    int has_only;
} SaveArg;

static int work_save(SmmProgress *p, void *arg)
{
    SaveArg *a = arg;
    char dest[SMM_PATH];
    return smm_pack_save(a->name, a->author, a->has_only ? &a->only : NULL, dest, sizeof dest, p);
}

static void done_save(int rc, void *arg)
{
    (void)rc;
    SaveArg *a = arg;
    smm_strs_free(&a->only);
    free(a);
}

int smm_queue_save_pack(const char *name, const char *author, const SmmStrs *only_files)
{
    SaveArg *a = smm_xmalloc(sizeof *a);
    memset(a, 0, sizeof *a);
    smm_copy(a->name, sizeof a->name, name);
    smm_copy(a->author, sizeof a->author, author);
    if (only_files) {
        a->has_only = 1;
        smm_strs_copy(&a->only, only_files);
    }
    char title[200];
    smm_fmt(title, sizeof title, "Save modpack %s", name);
    return smm_queue_run(title, work_save, a, done_save);
}

typedef struct {
    SmmMod *mods;
    int n;
} UpdateArg;

static int work_update(SmmProgress *p, void *arg)
{
    UpdateArg *a = arg;
    int failed = 0;
    for (int i = 0; i < a->n; i++) {
        smm_progress(p, i * 100 / a->n, "Updating %s...", a->mods[i].name);
        if (smm_mod_install(&a->mods[i], p) != 0) failed++;
    }
    if (failed) return smm_op_fail(p, "%d update(s) failed", failed);
    return 0;
}

static void done_update(int rc, void *arg)
{
    (void)rc;
    UpdateArg *a = arg;
    for (int i = 0; i < a->n; i++) smm_strs_free(&a->mods[i].deps);
    free(a->mods);
    free(a);
}

int smm_queue_update_mods(const SmmMods *latest, const SmmInstalledList *inst)
{
    UpdateArg *a = smm_xmalloc(sizeof *a);
    a->mods = smm_xmalloc((size_t)(inst->n ? inst->n : 1) * sizeof(SmmMod));
    a->n = 0;
    int melon = smm_detect_loader() == SMM_LOADER_MELON;
    for (int i = 0; i < inst->n; i++) {
        const SmmInstalled *m = &inst->v[i];
        if (!smm_installed_update_available(m)) continue;
        for (int k = 0; k < latest->n; k++) {
            char key[200];
            if (melon) smm_melon_key(latest->v[k].dll_name, key, sizeof key);
            else smm_copy(key, sizeof key, latest->v[k].dll_name);
            if (smm_strcasecmp(key, m->name) != 0 || !latest->v[k].download_url[0]) continue;
            a->mods[a->n] = latest->v[k];
            memset(&a->mods[a->n].deps, 0, sizeof(SmmStrs));
            a->n++;
            break;
        }
    }
    if (a->n == 0) {
        free(a->mods);
        free(a);
        return 0;
    }
    return smm_queue_run("Update mods", work_update, a, done_update);
}
