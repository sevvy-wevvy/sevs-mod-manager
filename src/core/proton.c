#include "internal.h"
#include <ctype.h>
#include <stdlib.h>

#define VDF_DEPTH 6

enum { TOK_END, TOK_STR, TOK_OPEN, TOK_CLOSE, TOK_BAD };

typedef struct {
    int kind;
    int quoted;
    size_t start, end;
} Tok;

typedef struct {
    int has_app;
    size_t open_end, close_at;
    int has_opt;
    size_t opt_start, opt_end;
    char *opt;
} AppHit;

static Tok next_tok(const char *s, size_t n, size_t *pos)
{
    Tok t = {TOK_END, 0, 0, 0};
    size_t i = *pos;
    for (;;) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
        if (i + 1 < n && s[i] == '/' && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        break;
    }
    if (i >= n) {
        *pos = i;
        return t;
    }
    t.start = i;
    if (s[i] == '{' || s[i] == '}') {
        t.kind = s[i] == '{' ? TOK_OPEN : TOK_CLOSE;
        t.end = ++i;
    } else if (s[i] == '"') {
        i++;
        while (i < n && s[i] != '"') i += s[i] == '\\' && i + 1 < n ? 2 : 1;
        if (i >= n) {
            t.kind = TOK_BAD;
            *pos = n;
            return t;
        }
        t.kind = TOK_STR;
        t.quoted = 1;
        t.end = ++i;
    } else {
        while (i < n && !isspace((unsigned char)s[i]) && s[i] != '{' && s[i] != '}' && s[i] != '"') i++;
        t.kind = TOK_STR;
        t.end = i;
    }
    *pos = i;
    return t;
}

static char *tok_text(const char *s, Tok t)
{
    size_t a = t.start, b = t.end;
    if (t.quoted) {
        a++;
        b--;
    }
    char *out = smm_xmalloc(b - a + 1);
    size_t k = 0;
    for (size_t i = a; i < b; i++) {
        if (t.quoted && s[i] == '\\' && i + 1 < b) {
            char c = s[++i];
            out[k++] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
        } else {
            out[k++] = s[i];
        }
    }
    out[k] = 0;
    return out;
}

static void quote_into(SmmBuf *b, const char *v)
{
    smm_buf_append(b, "\"", 1);
    for (; *v; v++) {
        switch (*v) {
        case '"': smm_buf_append(b, "\\\"", 2); break;
        case '\\': smm_buf_append(b, "\\\\", 2); break;
        case '\n': smm_buf_append(b, "\\n", 2); break;
        case '\t': smm_buf_append(b, "\\t", 2); break;
        case '\r': smm_buf_append(b, "\\r", 2); break;
        default: smm_buf_append(b, v, 1);
        }
    }
    smm_buf_append(b, "\"", 1);
}

static int find_app(const char *s, size_t n, const char *appid, AppHit *hit)
{
    const char *path[VDF_DEPTH] = {"UserLocalConfigStore", "Software", "Valve", "Steam", "apps", appid};
    memset(hit, 0, sizeof *hit);
    size_t pos = 0;
    int depth = 0, chain = 0, have_key = 0;
    Tok key = {0};
    for (;;) {
        Tok t = next_tok(s, n, &pos);
        if (t.kind == TOK_BAD) return -1;
        if (t.kind == TOK_END) return hit->has_app ? -1 : 0;
        if (t.kind == TOK_STR) {
            if (!have_key) {
                key = t;
                have_key = 1;
                continue;
            }
            have_key = 0;
            if (chain == VDF_DEPTH && depth == VDF_DEPTH && !hit->has_opt) {
                char *k = tok_text(s, key);
                if (smm_strcasecmp(k, "LaunchOptions") == 0) {
                    hit->has_opt = 1;
                    hit->opt_start = t.start;
                    hit->opt_end = t.end;
                    hit->opt = tok_text(s, t);
                }
                free(k);
            }
        } else if (t.kind == TOK_OPEN) {
            if (have_key && chain == depth && depth < VDF_DEPTH) {
                char *k = tok_text(s, key);
                if (smm_strcasecmp(k, path[depth]) == 0) chain++;
                free(k);
            }
            have_key = 0;
            depth++;
            if (chain == VDF_DEPTH && depth == VDF_DEPTH && !hit->has_app) {
                hit->has_app = 1;
                hit->open_end = t.end;
            }
        } else {
            have_key = 0;
            if (depth == 0) return hit->has_app ? -1 : 0;
            depth--;
            if (hit->has_app && depth == VDF_DEPTH - 1) {
                hit->close_at = t.start;
                return 1;
            }
            if (chain > depth) chain = depth;
        }
    }
}

static size_t span_lower(const char *s, size_t a, size_t b, char *buf, size_t cap)
{
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t len = b - a;
    if (len >= cap) len = cap - 1;
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)s[a + i]);
    buf[len] = 0;
    return len;
}

static int name_is(const char *s, size_t a, size_t b, const char *proxy)
{
    char buf[64];
    size_t len = span_lower(s, a, b, buf, sizeof buf);
    if (len > 4 && strcmp(buf + len - 4, ".dll") == 0) buf[len - 4] = 0;
    return strcmp(buf, proxy) == 0;
}

static int mode_is_native(const char *s, size_t a, size_t b)
{
    size_t c = a;
    while (c < b && s[c] != ',') c++;
    char buf[16];
    span_lower(s, a, c, buf, sizeof buf);
    return strcmp(buf, "n") == 0 || strcmp(buf, "native") == 0;
}

static void add_span(SmmBuf *out, char sep, const char *s, size_t a, size_t b)
{
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (a == b) return;
    if (out->n) smm_buf_append(out, &sep, 1);
    smm_buf_append(out, s + a, b - a);
}

static int fold_overrides(const char *v, const char *proxy, SmmBuf *kept)
{
    size_t len = strlen(v);
    int has_target = 0, native = 0;
    size_t a = 0;
    while (a <= len) {
        size_t e = a;
        while (e < len && v[e] != ';') e++;
        const char *eq = memchr(v + a, '=', e - a);
        if (!eq) {
            add_span(kept, ';', v, a, e);
        } else {
            size_t q = (size_t)(eq - v);
            SmmBuf names = {0};
            int hit = 0;
            size_t na = a;
            for (;;) {
                size_t ne = na;
                while (ne < q && v[ne] != ',') ne++;
                if (name_is(v, na, ne, proxy)) hit = 1;
                else add_span(&names, ',', v, na, ne);
                if (ne >= q) break;
                na = ne + 1;
            }
            if (!hit) {
                add_span(kept, ';', v, a, e);
            } else {
                has_target = 1;
                native = mode_is_native(v, q + 1, e);
                if (names.n) {
                    SmmBuf entry = {0};
                    smm_buf_append(&entry, names.p, names.n);
                    smm_buf_append(&entry, "=", 1);
                    size_t ma = q + 1, mb = e;
                    while (ma < mb && isspace((unsigned char)v[ma])) ma++;
                    while (mb > ma && isspace((unsigned char)v[mb - 1])) mb--;
                    smm_buf_append(&entry, v + ma, mb - ma);
                    add_span(kept, ';', entry.p, 0, entry.n);
                    smm_buf_free(&entry);
                }
            }
            smm_buf_free(&names);
        }
        a = e + 1;
    }
    return has_target && native;
}

static size_t quoted_end(const char *s, size_t from, char q)
{
    size_t j = from;
    while (s[j] && s[j] != q) j += q == '"' && s[j] == '\\' && s[j + 1] ? 2 : 1;
    return j;
}

static int copy_out(const SmmBuf *b, char *out, size_t n)
{
    if (b->n + 1 > n) {
        smm_set_error("The launch options are too long to edit.");
        return -1;
    }
    memcpy(out, b->p, b->n + 1);
    return 1;
}

int smm_proton_patch_launch_options(const char *old, const char *proxy, char *out, size_t n)
{
    size_t len = strlen(old);
    const char *cmd = strstr(old, "%command%");
    size_t limit = cmd ? (size_t)(cmd - old) : len;
    int found = 0;
    size_t as = 0, ae = 0, vs = 0, ve = 0;
    char quote = 0;
    size_t i = 0;
    while (i < limit) {
        char c = old[i];
        if (c == '"' || c == '\'') {
            size_t j = quoted_end(old, i + 1, c);
            if (!old[j]) break;
            i = j + 1;
            continue;
        }
        if (c == 'W' && (i == 0 || isspace((unsigned char)old[i - 1])) && strncmp(old + i, "WINEDLLOVERRIDES=", 17) == 0) {
            char q = old[i + 17];
            as = i;
            if (q == '"' || q == '\'') {
                size_t j = quoted_end(old, i + 18, q);
                if (!old[j]) {
                    smm_set_error("Couldn't read the WINEDLLOVERRIDES part of your Steam launch options. Fix or remove it, then try again.");
                    return -1;
                }
                vs = i + 18;
                ve = j;
                ae = j + 1;
                quote = q;
            } else {
                vs = i + 17;
                ve = vs;
                while (old[ve] && !isspace((unsigned char)old[ve])) ve++;
                ae = ve;
                quote = 0;
            }
            found = 1;
            i = ae;
            continue;
        }
        i++;
    }
    SmmBuf res = {0};
    int rc;
    char want[96];
    smm_fmt(want, sizeof want, "%s=n,b", proxy);
    if (found) {
        char *val = smm_xmalloc(ve - vs + 1);
        memcpy(val, old + vs, ve - vs);
        val[ve - vs] = 0;
        SmmBuf kept = {0};
        int done = fold_overrides(val, proxy, &kept);
        free(val);
        if (done) {
            smm_buf_free(&kept);
            smm_copy(out, n, old);
            return 0;
        }
        if (kept.n) smm_buf_append(&kept, ";", 1);
        smm_buf_append(&kept, want, strlen(want));
        char qc = quote == '\'' ? '\'' : '"';
        smm_buf_append(&res, old, as);
        smm_buf_append(&res, "WINEDLLOVERRIDES=", 17);
        smm_buf_append(&res, &qc, 1);
        smm_buf_append(&res, kept.p, kept.n);
        smm_buf_append(&res, &qc, 1);
        smm_buf_append(&res, old + ae, len - ae);
        smm_buf_free(&kept);
    } else {
        size_t a = 0, b = len;
        while (a < b && isspace((unsigned char)old[a])) a++;
        while (b > a && isspace((unsigned char)old[b - 1])) b--;
        smm_buf_printf(&res, "WINEDLLOVERRIDES=\"%s\" ", want);
        if (cmd) {
            smm_buf_append(&res, old + a, b - a);
        } else {
            smm_buf_append(&res, "%command%", 9);
            if (b > a) {
                smm_buf_append(&res, " ", 1);
                smm_buf_append(&res, old + a, b - a);
            }
        }
    }
    rc = copy_out(&res, out, n);
    smm_buf_free(&res);
    return rc;
}

int smm_proton_patch_vdf(const char *source, const char *appid, const char *proxy, SmmBuf *out)
{
    size_t n = strlen(source);
    AppHit hit;
    int rc = find_app(source, n, appid, &hit);
    if (rc < 0) {
        free(hit.opt);
        smm_set_error("Steam's localconfig.vdf didn't look right, so it was left alone.");
        return SMM_PATCH_BAD;
    }
    if (rc == 0) return SMM_PATCH_NO_APP;
    const char *old = hit.opt ? hit.opt : "";
    size_t cap = strlen(old) + 256;
    char *neu = smm_xmalloc(cap);
    int pr = smm_proton_patch_launch_options(old, proxy, neu, cap);
    free(hit.opt);
    if (pr <= 0) {
        free(neu);
        return pr < 0 ? SMM_PATCH_BAD : SMM_PATCH_SAME;
    }
    if (hit.has_opt) {
        smm_buf_append(out, source, hit.opt_start);
        quote_into(out, neu);
        smm_buf_append(out, source + hit.opt_end, n - hit.opt_end);
    } else {
        size_t ls = hit.close_at;
        while (ls > hit.open_end && source[ls - 1] != '\n') ls--;
        int own_line = ls > 0 && source[ls - 1] == '\n';
        for (size_t k = ls; k < hit.close_at; k++)
            if (source[k] != ' ' && source[k] != '\t') own_line = 0;
        if (own_line && ls >= hit.open_end) {
            smm_buf_append(out, source, ls);
            smm_buf_append(out, source + ls, hit.close_at - ls);
            smm_buf_append(out, "\t\"LaunchOptions\"\t\t", 18);
            quote_into(out, neu);
            smm_buf_append(out, "\n", 1);
            smm_buf_append(out, source + ls, n - ls);
        } else {
            smm_buf_append(out, source, hit.close_at);
            smm_buf_append(out, "\"LaunchOptions\"\t\t", 17);
            quote_into(out, neu);
            smm_buf_append(out, " ", 1);
            smm_buf_append(out, source + hit.close_at, n - hit.close_at);
        }
    }
    free(neu);
    return SMM_PATCH_CHANGED;
}

static int on_linux(void)
{
    return !smm_os_is_windows() && !smm_os_is_mac();
}

int smm_proton_game_appid(void)
{
    if (!on_linux() || !smm_is_linux_proton()) return 0;
    int id = smm_game_steam_appid();
    if (id > 0) return id;
    return smm_steam_appid_for_path(smm_settings.game_path);
}

static int steam_running(void)
{
    return smm_os_process_running("steam") || smm_os_process_running("steamwebhelper") || smm_os_process_running("com.valvesoftware.Steam");
}

static void config_files(SmmStrs *out)
{
    SmmStrs roots = {0};
    smm_steam_roots(&roots);
    for (int i = 0; i < roots.n; i++) {
        char ud[SMM_PATH];
        smm_join(ud, sizeof ud, roots.v[i], "userdata");
        SmmDirList l = {0};
        if (smm_os_list(ud, &l) != 0) continue;
        for (int k = 0; k < l.n; k++) {
            const char *nm = l.v[k].name;
            if (!l.v[k].is_dir || !isdigit((unsigned char)nm[0]) || strcmp(nm, "0") == 0) continue;
            char acc[SMM_PATH], cfg[SMM_PATH], f[SMM_PATH];
            smm_join(acc, sizeof acc, ud, nm);
            smm_join(cfg, sizeof cfg, acc, "config");
            smm_join(f, sizeof f, cfg, "localconfig.vdf");
            if (smm_os_is_file(f) && smm_strs_find(out, f, 0) < 0) smm_strs_add(out, f);
        }
        smm_dirlist_free(&l);
    }
    smm_strs_free(&roots);
}

static int demo_configured;

int smm_proton_status(int appid, const char *proxy)
{
    if (!on_linux()) return SMM_PROTON_NOT_LINUX;
    if (appid <= 0) return SMM_PROTON_NOT_STEAM_GAME;
    if (smm_demo_enabled()) return demo_configured ? SMM_PROTON_CONFIGURED : SMM_PROTON_NEEDS_CONFIG;
    char id[32];
    smm_fmt(id, sizeof id, "%d", appid);
    SmmStrs files = {0};
    config_files(&files);
    int need = 0, same = 0;
    for (int i = 0; i < files.n; i++) {
        SmmBuf src = {0}, res = {0};
        if (smm_file_read(files.v[i], &src) == 0) {
            int rc = smm_proton_patch_vdf(src.p, id, proxy, &res);
            if (rc == SMM_PATCH_CHANGED) need++;
            else if (rc == SMM_PATCH_SAME) same++;
        }
        smm_buf_free(&src);
        smm_buf_free(&res);
    }
    smm_strs_free(&files);
    if (need) return SMM_PROTON_NEEDS_CONFIG;
    return same ? SMM_PROTON_CONFIGURED : SMM_PROTON_UNKNOWN;
}

typedef struct {
    char path[SMM_PATH];
    SmmBuf data;
} Pending;

static int write_config(const char *path, const SmmBuf *data)
{
    char bak[SMM_PATH + 16];
    smm_fmt(bak, sizeof bak, "%s.smm-backup", path);
    if (!smm_os_exists(bak) && smm_os_copy(path, bak) != 0) return -1;
    char tmp[SMM_PATH + 16];
    smm_fmt(tmp, sizeof tmp, "%s.smm-tmp", path);
    if (smm_file_write(tmp, data->p, data->n) != 0) {
        smm_os_remove(tmp);
        return -1;
    }
    smm_os_copy_mode(path, tmp);
    if (smm_os_rename(tmp, path) != 0) {
        smm_os_remove(tmp);
        return -1;
    }
    return 0;
}

int smm_proton_configure(int appid, const char *proxy, int *accounts, int *updated, char *err, size_t err_n)
{
    int acc = 0, upd = 0;
    if (accounts) *accounts = 0;
    if (updated) *updated = 0;
    if (err && err_n) err[0] = 0;
    if (!on_linux() || appid <= 0) {
        if (err && err_n) smm_copy(err, err_n, "This only works for Steam games on Linux.");
        return SMM_PROTON_NO_CONFIG;
    }
    if (smm_demo_enabled()) {
        demo_configured = 1;
        if (accounts) *accounts = 1;
        if (updated) *updated = 1;
        return SMM_PROTON_OK;
    }
    char id[32];
    smm_fmt(id, sizeof id, "%d", appid);
    SmmStrs files = {0};
    config_files(&files);
    Pending *pend = files.n ? smm_xmalloc((size_t)files.n * sizeof *pend) : NULL;
    int np = 0, bad = 0;
    char bad_msg[256] = "";
    for (int i = 0; i < files.n; i++) {
        SmmBuf src = {0}, res = {0};
        if (smm_file_read(files.v[i], &src) != 0) {
            smm_buf_free(&src);
            continue;
        }
        int rc = smm_proton_patch_vdf(src.p, id, proxy, &res);
        smm_buf_free(&src);
        if (rc == SMM_PATCH_NO_APP) {
            smm_buf_free(&res);
            continue;
        }
        acc++;
        if (rc == SMM_PATCH_BAD) {
            if (!bad) smm_copy(bad_msg, sizeof bad_msg, smm_error());
            bad++;
            smm_buf_free(&res);
        } else if (rc == SMM_PATCH_CHANGED) {
            smm_copy(pend[np].path, sizeof pend[np].path, files.v[i]);
            pend[np].data = res;
            np++;
        } else {
            smm_buf_free(&res);
        }
    }
    int result;
    if (accounts) *accounts = acc;
    if (bad) {
        if (err && err_n) smm_copy(err, err_n, bad_msg);
        result = SMM_PROTON_PARSE_ERROR;
    } else if (acc == 0) {
        if (err && err_n) smm_copy(err, err_n, "Couldn't find this game in Steam's settings. Launch it once through Steam first.");
        result = SMM_PROTON_NO_CONFIG;
    } else if (np == 0) {
        result = SMM_PROTON_ALREADY;
    } else if (steam_running()) {
        result = SMM_PROTON_STEAM_RUNNING;
    } else {
        result = SMM_PROTON_OK;
        for (int i = 0; i < np; i++) {
            if (write_config(pend[i].path, &pend[i].data) == 0) {
                upd++;
            } else {
                result = SMM_PROTON_PARSE_ERROR;
                if (err && err_n) smm_fmt(err, err_n, "Couldn't write %s", pend[i].path);
            }
        }
    }
    if (updated) *updated = upd;
    for (int i = 0; i < np; i++) smm_buf_free(&pend[i].data);
    free(pend);
    smm_strs_free(&files);
    return result;
}
