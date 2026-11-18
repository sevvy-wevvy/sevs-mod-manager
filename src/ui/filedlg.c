#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#if defined(_WIN32)
#include "../core/internal.h"

static void ps_escape(SmmBuf *b, const char *s)
{
    for (; *s; s++) {
        if (*s == '\'') smm_buf_append(b, "'", 1);
        smm_buf_append(b, s, 1);
    }
}

static int base64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}

static int base64_decode(const char *in, char *out, size_t n)
{
    size_t k = 0;
    unsigned acc = 0;
    int bits = 0;
    for (; *in; in++) {
        int v = base64_value(*in);
        if (v < 0) continue;
        acc = acc << 6 | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (k + 1 >= n) break;
            out[k++] = (char)(acc >> bits & 0xFF);
        }
    }
    out[k] = 0;
    return (int)k;
}

static int pick_windows(int save, const char *title, const char *desc, const char *ext, const char *def, char *out, size_t n)
{
    SmmBuf s = {0};
    smm_buf_printf(&s, "Add-Type -AssemblyName System.Windows.Forms; $o=New-Object System.Windows.Forms.Form; $o.TopMost=$true; "
                       "$d=New-Object System.Windows.Forms.%sFileDialog; $d.Title='", save ? "Save" : "Open");
    ps_escape(&s, title);
    smm_buf_append(&s, "'; $d.Filter='", 14);
    ps_escape(&s, desc);
    smm_buf_append(&s, "|*.", 3);
    ps_escape(&s, ext);
    smm_buf_append(&s, "'; ", 3);
    if (save && def) {
        smm_buf_append(&s, "$d.FileName='", 13);
        ps_escape(&s, def);
        smm_buf_append(&s, "'; ", 3);
    }
    const char *tail = "if($d.ShowDialog($o) -eq 'OK'){[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($d.FileName))}";
    smm_buf_append(&s, tail, strlen(tail));
    const char *argv[] = {"powershell", "-NoProfile", "-NonInteractive", "-STA", "-ExecutionPolicy", "Bypass", "-Command", s.p, NULL};
    SmmBuf res = {0};
    int code = -1;
    int rc = smm_os_run(argv, &res, &code);
    smm_buf_free(&s);
    out[0] = 0;
    if (rc == 0 && code == 0 && res.p) base64_decode(res.p, out, n);
    smm_buf_free(&res);
    return out[0] != 0;
}
#else
static int capture(const char *cmd, char *out, size_t n)
{
    out[0] = 0;
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    if (fgets(out, (int)n, f)) {
        size_t l = strlen(out);
        while (l && (out[l - 1] == '\n' || out[l - 1] == '\r')) out[--l] = 0;
    }
    pclose(f);
    return out[0] != 0;
}

static int have(const char *prog)
{
    char cmd[200];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", prog);
    return system(cmd) == 0;
}
#endif

#if !defined(_WIN32)
static void shell_quote(char *out, size_t n, const char *s)
{
    size_t k = 0;
    if (n < 3) {
        if (n) out[0] = 0;
        return;
    }
    out[k++] = '\'';
    for (; *s && k + 6 < n; s++) {
        if (*s == '\'') {
            memcpy(out + k, "'\\''", 4);
            k += 4;
        } else {
            out[k++] = *s;
        }
    }
    out[k++] = '\'';
    out[k] = 0;
}
#endif

#if defined(__APPLE__)
static void script_text(char *out, size_t n, const char *s)
{
    size_t k = 0;
    for (; *s && k + 3 < n; s++) {
        if (*s == '"' || *s == '\\') out[k++] = '\\';
        out[k++] = *s;
    }
    out[k] = 0;
}
#endif

static int pick(int save, const char *title, const char *desc, const char *ext, const char *def, char *out, size_t n)
{
    char cmd[4096];
#if defined(_WIN32)
    return pick_windows(save, title, desc, ext, def, out, n);
#elif defined(__APPLE__)
    char t[300], d[600], script[1200], q[2400];
    script_text(t, sizeof t, title);
    script_text(d, sizeof d, def ? def : "file");
    if (save) snprintf(script, sizeof script, "POSIX path of (choose file name with prompt \"%s\" default name \"%s\")", t, d);
    else snprintf(script, sizeof script, "POSIX path of (choose file with prompt \"%s\")", t);
    shell_quote(q, sizeof q, script);
    snprintf(cmd, sizeof(cmd), "osascript -e %s 2>/dev/null", q);
    (void)desc;
    (void)ext;
    return capture(cmd, out, n);
#else
    char qt[600], qd[1200], qf[600], qe[200];
    shell_quote(qt, sizeof qt, title);
    shell_quote(qd, sizeof qd, def ? def : "");
    if (have("zenity")) {
        char filter[400];
        snprintf(filter, sizeof filter, "%s | *.%s", desc, ext);
        shell_quote(qf, sizeof qf, filter);
        char ft[1300] = "";
        if (def) snprintf(ft, sizeof ft, "--filename=%s", qd);
        snprintf(cmd, sizeof(cmd), "zenity --file-selection %s --title=%s %s --file-filter=%s 2>/dev/null", save ? "--save --confirm-overwrite" : "", qt, ft, qf);
        return capture(cmd, out, n);
    }
    if (have("kdialog")) {
        char pat[200];
        snprintf(pat, sizeof pat, "*.%s", ext);
        shell_quote(qe, sizeof qe, pat);
        snprintf(cmd, sizeof(cmd), "kdialog %s %s %s --title %s 2>/dev/null", save ? "--getsavefilename" : "--getopenfilename", def ? qd : "'.'", qe, qt);
        return capture(cmd, out, n);
    }
    app_status("No file dialog found. Install zenity or kdialog.");
    return 0;
#endif
}

int file_pick_open(const char *title, const char *desc, const char *ext, char *out, size_t n)
{
    return pick(0, title, desc, ext, NULL, out, n);
}

int file_pick_save(const char *title, const char *def, const char *ext, char *out, size_t n)
{
    return pick(1, title, "Files", ext, def, out, n);
}
