#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#if defined(_WIN32)
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

static int capture(const char *cmd, char *out, size_t n)
{
    out[0] = 0;
    FILE *f = POPEN(cmd, "r");
    if (!f) return 0;
    if (fgets(out, (int)n, f)) {
        size_t l = strlen(out);
        while (l && (out[l - 1] == '\n' || out[l - 1] == '\r')) out[--l] = 0;
    }
    PCLOSE(f);
    return out[0] != 0;
}

#if !defined(_WIN32)
static int have(const char *prog)
{
    char cmd[200];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", prog);
    return system(cmd) == 0;
}
#endif

static int pick(int save, const char *title, const char *desc, const char *ext, const char *def, char *out, size_t n)
{
    char cmd[1200];
#if defined(_WIN32)
    snprintf(cmd, sizeof(cmd),
             "powershell -NoProfile -STA -Command \"Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.%sFileDialog; "
             "$d.Title='%s'; $d.Filter='%s|*.%s'; %s if($d.ShowDialog() -eq 'OK'){[Console]::Out.WriteLine($d.FileName)}\"",
             save ? "Save" : "Open", title, desc, ext, save && def ? "$d.FileName='x';" : "");
    (void)def;
    return capture(cmd, out, n);
#elif defined(__APPLE__)
    if (save) snprintf(cmd, sizeof(cmd), "osascript -e 'POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' 2>/dev/null", title, def ? def : "file");
    else snprintf(cmd, sizeof(cmd), "osascript -e 'POSIX path of (choose file with prompt \"%s\")' 2>/dev/null", title);
    (void)desc;
    (void)ext;
    return capture(cmd, out, n);
#else
    if (have("zenity")) {
        snprintf(cmd, sizeof(cmd), "zenity --file-selection %s --title='%s' %s%s%s --file-filter='%s | *.%s' 2>/dev/null", save ? "--save --confirm-overwrite" : "", title,
                 def ? "--filename='" : "", def ? def : "", def ? "'" : "", desc, ext);
        return capture(cmd, out, n);
    }
    if (have("kdialog")) {
        snprintf(cmd, sizeof(cmd), "kdialog %s %s '*.%s' --title '%s' 2>/dev/null", save ? "--getsavefilename" : "--getopenfilename", def ? def : ".", ext, title);
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
