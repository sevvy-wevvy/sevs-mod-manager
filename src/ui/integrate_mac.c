#ifdef __APPLE__
#include "integrate.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../core/internal.h"

#define LSREGISTER "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
#define BUNDLE_ID "com.sevvywevvy.sevsmodmanager"
#define UTI "com.sevvywevvy.smmpack"

static const char plist[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    "<plist version=\"1.0\">\n"
    "<dict>\n"
    "  <key>CFBundleName</key><string>" INTEG_APP_NAME "</string>\n"
    "  <key>CFBundleDisplayName</key><string>" INTEG_APP_NAME "</string>\n"
    "  <key>CFBundleIdentifier</key><string>" BUNDLE_ID "</string>\n"
    "  <key>CFBundleExecutable</key><string>smm</string>\n"
    "  <key>CFBundleIconFile</key><string>icon</string>\n"
    "  <key>CFBundlePackageType</key><string>APPL</string>\n"
    "  <key>CFBundleVersion</key><string>" SMM_VERSION_TEXT "</string>\n"
    "  <key>CFBundleShortVersionString</key><string>" SMM_VERSION_TEXT "</string>\n"
    "  <key>NSHighResolutionCapable</key><true/>\n"
    "  <key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>\n"
    "  <key>CFBundleDocumentTypes</key>\n"
    "  <array>\n"
    "    <dict>\n"
    "      <key>CFBundleTypeName</key><string>" INTEG_PACK_NAME "</string>\n"
    "      <key>CFBundleTypeRole</key><string>Editor</string>\n"
    "      <key>LSHandlerRank</key><string>Owner</string>\n"
    "      <key>CFBundleTypeIconFile</key><string>icon</string>\n"
    "      <key>LSItemContentTypes</key><array><string>" UTI "</string></array>\n"
    "      <key>CFBundleTypeExtensions</key><array><string>smmpack</string></array>\n"
    "    </dict>\n"
    "  </array>\n"
    "  <key>UTExportedTypeDeclarations</key>\n"
    "  <array>\n"
    "    <dict>\n"
    "      <key>UTTypeIdentifier</key><string>" UTI "</string>\n"
    "      <key>UTTypeDescription</key><string>" INTEG_PACK_NAME "</string>\n"
    "      <key>UTTypeIconFile</key><string>icon</string>\n"
    "      <key>UTTypeConformsTo</key><array><string>public.data</string><string>public.archive</string></array>\n"
    "      <key>UTTypeTagSpecification</key>\n"
    "      <dict>\n"
    "        <key>public.filename-extension</key><array><string>smmpack</string></array>\n"
    "        <key>public.mime-type</key><string>" INTEG_PACK_MIME "</string>\n"
    "      </dict>\n"
    "    </dict>\n"
    "  </array>\n"
    "</dict>\n"
    "</plist>\n";

static void put_be32(unsigned char *p, size_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int write_icns(const char *path, const unsigned char *png, size_t n)
{
    unsigned char *b = smm_xmalloc(n + 16);
    memcpy(b, "icns", 4);
    put_be32(b + 4, n + 16);
    memcpy(b + 8, "ic09", 4);
    put_be32(b + 12, n + 8);
    memcpy(b + 16, png, n);
    int rc = integ_write(path, b, n + 16);
    free(b);
    return rc;
}

static void app_dir(char *out, size_t n)
{
    smm_join3(out, n, smm_os_home(), "Applications", INTEG_APP_NAME ".app");
}

int integ_os_register(const char *exe, const unsigned char *png, size_t png_len)
{
    char app[SMM_PATH], plist_path[SMM_PATH], icns[SMM_PATH], bin[SMM_PATH];
    app_dir(app, sizeof app);
    smm_join(plist_path, sizeof plist_path, app, "Contents/Info.plist");
    smm_join(icns, sizeof icns, app, "Contents/Resources/icon.icns");
    smm_join(bin, sizeof bin, app, "Contents/MacOS/smm");
    if (integ_write(plist_path, plist, sizeof plist - 1) != 0) return -1;
    if (write_icns(icns, png, png_len) != 0) return -1;
    smm_os_remove(bin);
    if (symlink(exe, bin) != 0) return -1;
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
