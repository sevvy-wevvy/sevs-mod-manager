#define _DEFAULT_SOURCE
#include "internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

void *smm_xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fputs("out of memory\n", stderr);
        abort();
    }
    return p;
}

void *smm_xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fputs("out of memory\n", stderr);
        abort();
    }
    return q;
}

char *smm_xstrdup(const char *s)
{
    size_t n = strlen(s ? s : "");
    char *r = smm_xmalloc(n + 1);
    memcpy(r, s ? s : "", n);
    r[n] = 0;
    return r;
}

void smm_copy(char *dst, size_t n, const char *src)
{
    if (!n) return;
    if (!src) src = "";
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memmove(dst, src, l);
    dst[l] = 0;
}

int smm_fmt(char *dst, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, n, fmt, ap);
    va_end(ap);
    if (r < 0 || (size_t)r >= n) {
        if (n) dst[n - 1] = 0;
        return -1;
    }
    return r;
}

void smm_join(char *out, size_t n, const char *a, const char *b)
{
    size_t l = strlen(a);
    int sep = l > 0 && a[l - 1] != '/' && a[l - 1] != '\\';
    smm_fmt(out, n, "%s%s%s", a, sep ? (SMM_SEP == '/' ? "/" : "\\") : "", b);
}

void smm_join3(char *out, size_t n, const char *a, const char *b, const char *c)
{
    char tmp[SMM_PATH];
    smm_join(tmp, sizeof tmp, a, b);
    smm_join(out, n, tmp, c);
}

int smm_starts_with(const char *s, const char *p)
{
    return strncmp(s, p, strlen(p)) == 0;
}

int smm_ends_with(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

int smm_strcasecmp(const char *a, const char *b)
{
    while (*a && *b) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x != y) return x - y;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int smm_starts_with_nocase(const char *s, const char *p)
{
    while (*p) {
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*p)) return 0;
        s++;
        p++;
    }
    return 1;
}

int smm_ends_with_nocase(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && smm_strcasecmp(s + a - b, suf) == 0;
}

char *smm_stristr(const char *h, const char *n)
{
    if (!*n) return (char *)h;
    for (; *h; h++)
        if (smm_starts_with_nocase(h, n)) return (char *)h;
    return NULL;
}

void smm_trim(char *s)
{
    size_t n = strlen(s), a = 0;
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    while (a < n && isspace((unsigned char)s[a])) a++;
    memmove(s, s + a, n - a);
    s[n - a] = 0;
}

const char *smm_basename(const char *path)
{
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

void smm_dirname(const char *path, char *out, size_t n)
{
    const char *b = smm_basename(path);
    size_t l = (size_t)(b - path);
    int drive_root = l == 3 && path[1] == ':';
    if (l > 1 && !drive_root) l--;
    if (l >= n) l = n - 1;
    memcpy(out, path, l);
    out[l] = 0;
}

void smm_stem(const char *path, char *out, size_t n)
{
    smm_copy(out, n, smm_basename(path));
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
}

static int cmp_nocase(const void *a, const void *b)
{
    return smm_strcasecmp(*(char *const *)a, *(char *const *)b);
}

void smm_sort_strs_nocase(SmmStrs *s)
{
    if (s->n > 1) qsort(s->v, (size_t)s->n, sizeof(char *), cmp_nocase);
}

void smm_strs_add(SmmStrs *s, const char *str)
{
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = smm_xrealloc(s->v, (size_t)s->cap * sizeof(char *));
    }
    s->v[s->n++] = smm_xstrdup(str);
}

void smm_strs_free(SmmStrs *s)
{
    for (int i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

int smm_strs_find(const SmmStrs *s, const char *str, int nocase)
{
    for (int i = 0; i < s->n; i++)
        if (nocase ? smm_strcasecmp(s->v[i], str) == 0 : strcmp(s->v[i], str) == 0) return i;
    return -1;
}

int smm_strs_remove(SmmStrs *s, const char *str)
{
    int i = smm_strs_find(s, str, 0);
    if (i < 0) return 0;
    free(s->v[i]);
    memmove(s->v + i, s->v + i + 1, (size_t)(s->n - i - 1) * sizeof(char *));
    s->n--;
    return 1;
}

void smm_strs_copy(SmmStrs *dst, const SmmStrs *src)
{
    memset(dst, 0, sizeof *dst);
    for (int i = 0; i < src->n; i++) smm_strs_add(dst, src->v[i]);
}

static int map_find(const SmmMap *m, const char *key)
{
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->k[i], key) == 0) return i;
    return -1;
}

const char *smm_map_get(const SmmMap *m, const char *key)
{
    int i = map_find(m, key);
    return i < 0 ? NULL : m->v[i];
}

void smm_map_set(SmmMap *m, const char *key, const char *val)
{
    int i = map_find(m, key);
    if (i >= 0) {
        free(m->v[i]);
        m->v[i] = smm_xstrdup(val);
        return;
    }
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 8;
        m->k = smm_xrealloc(m->k, (size_t)m->cap * sizeof(char *));
        m->v = smm_xrealloc(m->v, (size_t)m->cap * sizeof(char *));
    }
    m->k[m->n] = smm_xstrdup(key);
    m->v[m->n++] = smm_xstrdup(val);
}

int smm_map_del(SmmMap *m, const char *key)
{
    int i = map_find(m, key);
    if (i < 0) return 0;
    free(m->k[i]);
    free(m->v[i]);
    memmove(m->k + i, m->k + i + 1, (size_t)(m->n - i - 1) * sizeof(char *));
    memmove(m->v + i, m->v + i + 1, (size_t)(m->n - i - 1) * sizeof(char *));
    m->n--;
    return 1;
}

void smm_map_free(SmmMap *m)
{
    for (int i = 0; i < m->n; i++) {
        free(m->k[i]);
        free(m->v[i]);
    }
    free(m->k);
    free(m->v);
    memset(m, 0, sizeof *m);
}

void smm_buf_append(SmmBuf *b, const void *data, size_t n)
{
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 256;
        while (c < b->n + n + 1) c *= 2;
        b->p = smm_xrealloc(b->p, c);
        b->cap = c;
    }
    memcpy(b->p + b->n, data, n);
    b->n += n;
    b->p[b->n] = 0;
}

void smm_buf_free(SmmBuf *b)
{
    free(b->p);
    memset(b, 0, sizeof *b);
}

void smm_buf_printf(SmmBuf *b, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (r < 0) return;
    if ((size_t)r < sizeof tmp) {
        smm_buf_append(b, tmp, (size_t)r);
        return;
    }
    char *big = smm_xmalloc((size_t)r + 1);
    va_start(ap, fmt);
    vsnprintf(big, (size_t)r + 1, fmt, ap);
    va_end(ap);
    smm_buf_append(b, big, (size_t)r);
    free(big);
}

void smm_bufs_cstr(SmmBuf *b)
{
    if (!b->p) smm_buf_append(b, "", 0);
}

void smm_json_escape(SmmBuf *b, const char *s)
{
    smm_buf_append(b, "\"", 1);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') smm_buf_append(b, "\\\"", 2);
        else if (c == '\\') smm_buf_append(b, "\\\\", 2);
        else if (c == '\n') smm_buf_append(b, "\\n", 2);
        else if (c == '\r') smm_buf_append(b, "\\r", 2);
        else if (c == '\t') smm_buf_append(b, "\\t", 2);
        else if (c < 0x20) smm_buf_printf(b, "\\u%04x", c);
        else smm_buf_append(b, s, 1);
    }
    smm_buf_append(b, "\"", 1);
}

static SMM_TLS char g_err[512];

const char *smm_error(void)
{
    return g_err;
}

void smm_set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_err, sizeof g_err, fmt, ap);
    va_end(ap);
}

int smm_file_read(const char *path, SmmBuf *out)
{
    FILE *f = smm_os_fopen(path, "rb");
    if (!f) {
        smm_set_error("Couldn't open %s", path);
        return -1;
    }
    char tmp[65536];
    size_t r;
    while ((r = fread(tmp, 1, sizeof tmp, f)) > 0) smm_buf_append(out, tmp, r);
    int bad = ferror(f);
    fclose(f);
    if (bad) {
        smm_set_error("Couldn't read %s", path);
        return -1;
    }
    smm_bufs_cstr(out);
    return 0;
}

int smm_file_write(const char *path, const void *data, size_t n)
{
    FILE *f = smm_os_fopen(path, "wb");
    if (!f) {
        smm_set_error("Couldn't write %s", path);
        return -1;
    }
    int ok = n == 0 || fwrite(data, 1, n, f) == n;
    if (fclose(f) != 0) ok = 0;
    if (!ok) smm_set_error("Couldn't write %s", path);
    return ok ? 0 : -1;
}

int smm_file_write_atomic(const char *path, const void *data, size_t n)
{
    char tmp[SMM_PATH];
    smm_fmt(tmp, sizeof tmp, "%s.tmp", path);
    if (smm_file_write(tmp, data, n) != 0) return -1;
    if (smm_os_rename(tmp, path) != 0) {
        smm_os_remove(tmp);
        smm_set_error("Couldn't replace %s", path);
        return -1;
    }
    return 0;
}

int smm_split_lines(const char *text, size_t n, SmmStrs *out)
{
    size_t a = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || text[i] == '\n') {
            if (i == n && a == i) break;
            size_t e = i;
            if (e > a && text[e - 1] == '\r') e--;
            char *line = smm_xmalloc(e - a + 1);
            memcpy(line, text + a, e - a);
            line[e - a] = 0;
            smm_strs_add(out, line);
            free(line);
            a = i + 1;
        }
    }
    return 0;
}

int smm_read_lines(const char *path, SmmStrs *out)
{
    SmmBuf b = {0};
    if (smm_file_read(path, &b) != 0) return -1;
    const char *text = b.p;
    size_t len = b.n;
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
        text += 3;
        len -= 3;
    }
    smm_split_lines(text, len, out);
    smm_buf_free(&b);
    return 0;
}

int smm_iso_time(long long t, char *out, size_t n)
{
    time_t tt = (time_t)t;
    struct tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &tt);
#else
    gmtime_r(&tt, &tmv);
#endif
    return (int)strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv) > 0 ? 0 : -1;
}

int smm_parse_iso(const char *s, long long *out)
{
    int y, mo, d, h = 0, mi = 0, se = 0;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) < 3) return -1;
    long long days = 0;
    int yy = y - (mo <= 2);
    int era = (yy >= 0 ? yy : yy - 399) / 400;
    int yoe = yy - era * 400;
    int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    days = (long long)era * 146097 + doe - 719468;
    *out = days * 86400 + h * 3600 + mi * 60 + se;
    return 0;
}

void smm_cmdline_quote(const char *const *argv, SmmBuf *out)
{
    for (int i = 0; argv[i]; i++) {
        if (i) smm_buf_append(out, " ", 1);
        smm_buf_append(out, "\"", 1);
        int slashes = 0;
        for (const char *p = argv[i]; *p; p++) {
            if (*p == '\\') {
                slashes++;
                continue;
            }
            if (*p == '"') {
                for (int k = 0; k < slashes * 2 + 1; k++) smm_buf_append(out, "\\", 1);
            } else {
                for (int k = 0; k < slashes; k++) smm_buf_append(out, "\\", 1);
            }
            slashes = 0;
            smm_buf_append(out, p, 1);
        }
        for (int k = 0; k < slashes * 2; k++) smm_buf_append(out, "\\", 1);
        smm_buf_append(out, "\"", 1);
    }
    smm_bufs_cstr(out);
}
