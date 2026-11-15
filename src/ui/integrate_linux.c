#if !defined(_WIN32) && !defined(__APPLE__)
#include "integrate.h"
#include <stdlib.h>
#include <string.h>
#include "../core/internal.h"

static const char mime_xml[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
    "  <mime-type type=\"" INTEG_PACK_MIME "\">\n"
    "    <comment>" INTEG_PACK_NAME "</comment>\n"
    "    <icon name=\"application-x-smmpack\"/>\n"
    "    <sub-class-of type=\"application/zip\"/>\n"
    "    <glob pattern=\"*.smmpack\"/>\n"
    "  </mime-type>\n"
    "</mime-info>\n";

static void data_home(char *out, size_t n)
{
    const char *x = getenv("XDG_DATA_HOME");
    if (x && *x == '/') smm_copy(out, n, x);
    else smm_join3(out, n, smm_os_home(), ".local", "share");
}

static void paths(char *desktop, char *mime, char *app_icon, char *mime_icon, char *share, size_t n)
{
    char base[SMM_PATH];
    data_home(base, sizeof base);
    smm_copy(share, n, base);
    smm_join3(desktop, n, base, "applications", INTEG_APP_ID ".desktop");
    smm_join3(mime, n, base, "mime/packages", INTEG_APP_ID ".xml");
    smm_join3(app_icon, n, base, "icons/hicolor/512x512/apps", INTEG_APP_ID ".png");
    smm_join3(mime_icon, n, base, "icons/hicolor/512x512/mimetypes", "application-x-smmpack.png");
}

static void quote_exec(SmmBuf *b, const char *exe)
{
    if (!strpbrk(exe, " \t\"'\\><~|&;$*?#()`%")) {
        smm_buf_append(b, exe, strlen(exe));
        return;
    }
    smm_buf_append(b, "\"", 1);
    for (const char *p = exe; *p; p++) {
        if (*p == '\\') smm_buf_append(b, "\\\\\\\\", 4);
        else if (*p == '"' || *p == '`' || *p == '$') {
            smm_buf_append(b, "\\", 1);
            smm_buf_append(b, p, 1);
        } else if (*p == '%') smm_buf_append(b, "%%", 2);
        else smm_buf_append(b, p, 1);
    }
    smm_buf_append(b, "\"", 1);
}

static void refresh(const char *share)
{
    char dir[SMM_PATH];
    smm_join(dir, sizeof dir, share, "mime");
    integ_run_quiet("update-mime-database", dir, NULL, NULL, NULL);
    smm_join(dir, sizeof dir, share, "applications");
    integ_run_quiet("update-desktop-database", dir, NULL, NULL, NULL);
    smm_join(dir, sizeof dir, share, "icons/hicolor");
    integ_run_quiet("gtk-update-icon-cache", "-f", "-t", dir, NULL);
}

int integ_os_register(const char *exe, const unsigned char *png, size_t png_len)
{
    char desktop[SMM_PATH], mime[SMM_PATH], app_icon[SMM_PATH], mime_icon[SMM_PATH], share[SMM_PATH];
    paths(desktop, mime, app_icon, mime_icon, share, SMM_PATH);
    SmmBuf d = {0};
    smm_buf_printf(&d,
                   "[Desktop Entry]\nType=Application\nName=" INTEG_APP_NAME "\nComment=Mod manager for Unity games\nExec=");
    quote_exec(&d, exe);
    smm_buf_printf(&d,
                   " %%f\nIcon=" INTEG_APP_ID "\nStartupWMClass=" INTEG_APP_ID "\nMimeType=" INTEG_PACK_MIME
                   ";\nCategories=Game;Utility;\nTerminal=false\n");
    int rc = integ_write(desktop, d.p, d.n);
    smm_buf_free(&d);
    if (rc == 0) rc = integ_write(mime, mime_xml, sizeof mime_xml - 1);
    if (rc == 0) rc = integ_write(app_icon, png, png_len);
    if (rc == 0) rc = integ_write(mime_icon, png, png_len);
    if (rc != 0) return -1;
    refresh(share);
    integ_run_quiet("xdg-mime", "default", INTEG_APP_ID ".desktop", INTEG_PACK_MIME, NULL);
    return 0;
}

int integ_os_unregister(void)
{
    char desktop[SMM_PATH], mime[SMM_PATH], app_icon[SMM_PATH], mime_icon[SMM_PATH], share[SMM_PATH];
    paths(desktop, mime, app_icon, mime_icon, share, SMM_PATH);
    smm_os_remove(desktop);
    smm_os_remove(mime);
    smm_os_remove(app_icon);
    smm_os_remove(mime_icon);
    refresh(share);
    return 0;
}
#endif
