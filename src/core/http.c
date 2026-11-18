#include "internal.h"
#include <ctype.h>
#include <stdlib.h>

void smm_url_escape(const char *s, char *out, size_t n)
{
    size_t k = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (; *s && k + 4 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[k++] = (char)c;
        else {
            out[k++] = '%';
            out[k++] = hex[c >> 4];
            out[k++] = hex[c & 15];
        }
    }
    out[k] = 0;
}

const char *smm_http_backend(void)
{
    return "curl";
}

static const char *revoke_flag(void)
{
    return smm_os_is_windows() ? "--ssl-no-revoke" : "-s";
}

static int bad_url(const char *url)
{
    if (smm_starts_with_nocase(url, "https://") || smm_starts_with_nocase(url, "http://")) return 0;
    smm_set_error("Refusing to fetch a link that isn't http or https");
    return 1;
}

static int curl_missing(void)
{
    smm_set_error("curl was not found. Install curl to use online features.");
    return -1;
}

static int parse_status(SmmBuf *b)
{
    if (b->n < 4) return -1;
    size_t cut = b->n - 4;
    if (b->p[cut] != '\n') return -1;
    int code = atoi(b->p + cut + 1);
    b->n = cut;
    b->p[cut] = 0;
    return code;
}

static int run_curl(const char *const *argv, SmmBuf *out)
{
    SmmBuf tmp = {0};
    int exit_code = 0;
    if (smm_os_run(argv, &tmp, &exit_code) != 0) {
        smm_buf_free(&tmp);
        return curl_missing();
    }
    if (exit_code != 0) {
        smm_buf_free(&tmp);
        smm_set_error("Network request failed (curl exit %d)", exit_code);
        return -1;
    }
    int code = parse_status(&tmp);
    if (out) {
        smm_buf_append(out, tmp.p ? tmp.p : "", tmp.n);
    }
    smm_buf_free(&tmp);
    if (code < 0) smm_set_error("Bad response from curl");
    return code;
}

int smm_http_get(const char *url, SmmBuf *out)
{
    int handled = 0;
    int r = smm_demo_http(url, NULL, out, &handled);
    if (handled) return r;
    if (bad_url(url)) return -1;
    const char *argv[18];
    int n = 0;
    argv[n++] = "curl";
    argv[n++] = revoke_flag();
    argv[n++] = "-sSL";
    argv[n++] = "--max-time";
    argv[n++] = "120";
    argv[n++] = "-A";
    argv[n++] = SMM_USER_AGENT;
    if (strstr(url, "api.github.com")) {
        argv[n++] = "-H";
        argv[n++] = "Accept: application/vnd.github+json";
    }
    argv[n++] = "-w";
    argv[n++] = "\n%{http_code}";
    argv[n++] = url;
    argv[n] = NULL;
    return run_curl(argv, out);
}

int smm_http_post_form(const char *url, const SmmMap *fields, SmmBuf *out)
{
    SmmBuf body = {0};
    for (int i = 0; i < fields->n; i++) {
        char k[512], v[1024];
        smm_url_escape(fields->k[i], k, sizeof k);
        smm_url_escape(fields->v[i], v, sizeof v);
        smm_buf_printf(&body, "%s%s=%s", i ? "&" : "", k, v);
    }
    smm_bufs_cstr(&body);
    int handled = 0;
    int r = smm_demo_http(url, body.p, out, &handled);
    if (handled) {
        smm_buf_free(&body);
        return r;
    }
    if (bad_url(url)) {
        smm_buf_free(&body);
        return -1;
    }
        const char *argv[] = {"curl", revoke_flag(), "-sSL", "--max-time", "15", "-A", SMM_USER_AGENT, "-X", "POST", "--data", body.p,
                          "-w", "\n%{http_code}", url, NULL};
    r = run_curl(argv, out);
    smm_buf_free(&body);
    return r;
}

int smm_http_url_exists(const char *url)
{
    if (!url || !*url) return 1;
    int handled = 0;
    SmmBuf junk = {0};
    int r = smm_demo_http(url, NULL, &junk, &handled);
    smm_buf_free(&junk);
    if (handled) return r != 404;
    if (bad_url(url)) return 0;
        const char *argv[] = {"curl", revoke_flag(), "-sL", "--max-time", "15", "-A", SMM_USER_AGENT, "-r", "0-0", "-o", "-", "-w", "\n%{http_code}", url, NULL};
    SmmBuf b = {0};
    int code = run_curl(argv, &b);
    smm_buf_free(&b);
    return code != 404;
}

static long long content_length(const char *url)
{
    const char *argv[] = {"curl", revoke_flag(), "-sIL", "--max-time", "20", "-A", SMM_USER_AGENT, url, NULL};
    SmmBuf b = {0};
    int code = 0;
    long long len = -1;
    if (smm_os_run(argv, &b, &code) == 0 && b.p) {
        const char *p = b.p;
        while (*p) {
            if (smm_starts_with_nocase(p, "content-length:")) len = atoll(p + 15);
            const char *nl = strchr(p, '\n');
            if (!nl) break;
            p = nl + 1;
        }
    }
    smm_buf_free(&b);
    return len;
}

int smm_http_download(const char *url, const char *dest, SmmProgress *p, const char *label, int pct_from, int pct_to)
{
    int handled = 0;
    int r = smm_demo_download(url, dest, &handled);
    if (handled) return r;
    if (bad_url(url)) return -1;
    char part[SMM_PATH], dir[SMM_PATH];
    smm_fmt(part, sizeof part, "%s.part", dest);
    smm_dirname(dest, dir, sizeof dir);
    if (dir[0]) smm_os_mkdir_p(dir);
    smm_os_remove(part);
    long long total = content_length(url);
    const char *argv[] = {"curl", revoke_flag(), "-sSL", "--fail", "--max-time", "300", "-A", SMM_USER_AGENT, "-o", part, url, NULL};
    SmmProc *proc = smm_os_spawn(argv);
    if (!proc) return curl_missing();
    int code = 0, timed_out = 0;
    long long start = smm_os_ms();
    while (smm_os_proc_running(proc, &code)) {
        long long sz = smm_os_size(part);
        if (sz < 0) sz = 0;
        int pct = total > 0 ? pct_from + (int)(sz * (pct_to - pct_from) / total) : pct_from;
        if (pct > pct_to) pct = pct_to;
        smm_progress(p, pct, "Downloading %s... %lldKB", label, sz / 1024);
        smm_os_sleep(120);
        if (smm_os_ms() - start > 310000) {
            timed_out = 1;
            break;
        }
    }
    smm_os_proc_free(proc);
    if (code != 0 || timed_out) {
        smm_os_remove(part);
        smm_set_error("Download of %s failed (curl exit %d)", label, code);
        return -1;
    }
    smm_os_remove(dest);
    if (smm_os_rename(part, dest) != 0) {
        smm_set_error("Couldn't save %s", label);
        return -1;
    }
    return 0;
}
