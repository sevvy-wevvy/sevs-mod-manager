#ifdef __APPLE__
#include "integrate.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../core/internal.h"

#define LSREGISTER "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
#define PLISTBUDDY "/usr/libexec/PlistBuddy"

static void app_dir(char *out, size_t n)
{
    smm_join3(out, n, smm_os_home(), "Applications", INTEG_APP_NAME ".app");
}

static void scratch_dir(char *out, size_t n)
{
    char t[SMM_PATH];
    smm_os_temp_dir(t, sizeof t);
    if (strpbrk(t, " \t\"'\\;")) smm_copy(t, sizeof t, "/tmp");
    smm_copy(out, n, t);
}

static int write_buf(const char *path, const SmmBuf *b)
{
    return integ_write(path, b->p, b->n);
}

static int build_applet(const char *app, const char *exe, const unsigned char *png, size_t png_len)
{
    char tmp[SMM_PATH], src[SMM_PATH], frag[SMM_PATH], plist[SMM_PATH], icns[SMM_PATH], old_icns[SMM_PATH], cmd[SMM_PATH + 32];
    scratch_dir(tmp, sizeof tmp);
    smm_join(src, sizeof src, tmp, "smm-applet.applescript");
    smm_join(frag, sizeof frag, tmp, "smm-applet-keys.plist");
    smm_join(plist, sizeof plist, app, "Contents/Info.plist");
    smm_join(icns, sizeof icns, app, "Contents/Resources/icon.icns");
    smm_join(old_icns, sizeof old_icns, app, "Contents/Resources/applet.icns");

    SmmBuf s = {0}, k = {0}, i = {0};
    integ_applet_source(&s, exe);
    integ_plist_fragment(&k);
    integ_icns(&i, png, png_len);
    int rc = write_buf(src, &s) == 0 && write_buf(frag, &k) == 0 ? 0 : -1;
    smm_buf_free(&s);
    smm_buf_free(&k);

    if (rc == 0) {
        smm_os_remove_tree(app);
        rc = integ_run_code("osacompile", "-o", app, src, NULL) == 0 && smm_os_is_dir(app) ? 0 : -1;
    }
    if (rc == 0) {
        smm_os_remove(old_icns);
        rc = write_buf(icns, &i);
    }
    smm_buf_free(&i);
    if (rc == 0) {
        static const char *const keys[] = {"CFBundleIconFile", "CFBundleIdentifier", "CFBundleName", "CFBundleDisplayName", "LSUIElement",
                                           "CFBundleDocumentTypes", "UTExportedTypeDeclarations"};
        for (size_t n = 0; n < sizeof keys / sizeof *keys; n++) {
            smm_fmt(cmd, sizeof cmd, "Delete :%s", keys[n]);
            integ_run_quiet(PLISTBUDDY, "-c", cmd, plist, NULL);
        }
        smm_fmt(cmd, sizeof cmd, "Merge %s", frag);
        rc = integ_run_code(PLISTBUDDY, "-c", cmd, plist, NULL) == 0 ? 0 : -1;
    }
    smm_os_remove(src);
    smm_os_remove(frag);
    if (rc == 0) {
        const char *argv[] = {"codesign", "--force", "--deep", "--sign", "-", app, NULL};
        int code;
        smm_os_run(argv, NULL, &code);
    }
    return rc;
}

static int build_symlink_bundle(const char *app, const char *exe, const unsigned char *png, size_t png_len)
{
    char plist[SMM_PATH], icns[SMM_PATH], bin[SMM_PATH];
    smm_join(plist, sizeof plist, app, "Contents/Info.plist");
    smm_join(icns, sizeof icns, app, "Contents/Resources/icon.icns");
    smm_join(bin, sizeof bin, app, "Contents/MacOS/smm");
    smm_os_remove_tree(app);
    SmmBuf p = {0}, i = {0};
    integ_plist_full(&p);
    integ_icns(&i, png, png_len);
    int rc = write_buf(plist, &p) == 0 && write_buf(icns, &i) == 0 ? 0 : -1;
    smm_buf_free(&p);
    smm_buf_free(&i);
    if (rc == 0) {
        char dir[SMM_PATH];
        smm_dirname(bin, dir, sizeof dir);
        smm_os_mkdir_p(dir);
        if (symlink(exe, bin) != 0) rc = -1;
    }
    return rc;
}

int integ_os_register(const char *exe, const unsigned char *png, size_t png_len)
{
    char app[SMM_PATH], parent[SMM_PATH];
    app_dir(app, sizeof app);
    smm_dirname(app, parent, sizeof parent);
    smm_os_mkdir_p(parent);
    if (build_applet(app, exe, png, png_len) != 0 && build_symlink_bundle(app, exe, png, png_len) != 0) return -1;
    integ_run_quiet(LSREGISTER, "-f", app, NULL, NULL);
    return 0;
}

int integ_os_unregister(void)
{
    char app[SMM_PATH];
    app_dir(app, sizeof app);
    integ_run_quiet(LSREGISTER, "-u", app, NULL, NULL);
    smm_os_remove_tree(app);
    return 0;
}
#endif
