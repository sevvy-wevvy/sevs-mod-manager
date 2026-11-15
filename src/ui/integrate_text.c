#include "integrate.h"
#include <string.h>
#include "../core/internal.h"

#define UTI "com.sevvywevvy.smmpack"
#define BUNDLE_ID "com.sevvywevvy.sevsmodmanager"

static void script_string(SmmBuf *b, const char *s)
{
    smm_buf_append(b, "\"", 1);
    for (; *s; s++) {
        if (*s == '\\' || *s == '"') smm_buf_append(b, "\\", 1);
        if (*s == '\n') smm_buf_append(b, "\\n", 2);
        else if (*s == '\r') smm_buf_append(b, "\\r", 2);
        else if (*s == '\t') smm_buf_append(b, "\\t", 2);
        else smm_buf_append(b, s, 1);
    }
    smm_buf_append(b, "\"", 1);
}

void integ_applet_source(SmmBuf *b, const char *exe)
{
    smm_buf_printf(b,
                   "on run\n"
                   "\tlaunchSmm({})\n"
                   "end run\n"
                   "\n"
                   "on open theFiles\n"
                   "\tset args to {}\n"
                   "\trepeat with i from 1 to count of theFiles\n"
                   "\t\tset end of args to POSIX path of (item i of theFiles)\n"
                   "\tend repeat\n"
                   "\tlaunchSmm(args)\n"
                   "end open\n"
                   "\n"
                   "on launchSmm(args)\n"
                   "\tset exe to ");
    script_string(b, exe);
    smm_buf_printf(b,
                   "\n"
                   "\ttry\n"
                   "\t\tdo shell script \"test -x \" & quoted form of exe\n"
                   "\ton error\n"
                   "\t\treturn\n"
                   "\tend try\n"
                   "\tset cmd to quoted form of exe\n"
                   "\trepeat with i from 1 to count of args\n"
                   "\t\tset cmd to cmd & \" \" & quoted form of (item i of args)\n"
                   "\tend repeat\n"
                   "\tdo shell script cmd & \" >/dev/null 2>&1 &\"\n"
                   "end launchSmm\n");
}

void integ_plist_types(SmmBuf *b)
{
    smm_buf_printf(b,
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
                   "  </array>\n");
}

static void plist_open(SmmBuf *b)
{
    smm_buf_printf(b,
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                   "<plist version=\"1.0\">\n"
                   "<dict>\n"
                   "  <key>CFBundleName</key><string>" INTEG_APP_NAME "</string>\n"
                   "  <key>CFBundleDisplayName</key><string>" INTEG_APP_NAME "</string>\n"
                   "  <key>CFBundleIdentifier</key><string>" BUNDLE_ID "</string>\n"
                   "  <key>CFBundleIconFile</key><string>icon</string>\n");
}

static void plist_close(SmmBuf *b)
{
    smm_buf_printf(b, "</dict>\n</plist>\n");
}

void integ_plist_fragment(SmmBuf *b)
{
    plist_open(b);
    smm_buf_printf(b, "  <key>LSUIElement</key><true/>\n");
    integ_plist_types(b);
    plist_close(b);
}

void integ_plist_full(SmmBuf *b)
{
    plist_open(b);
    smm_buf_printf(b,
                   "  <key>CFBundleExecutable</key><string>smm</string>\n"
                   "  <key>CFBundlePackageType</key><string>APPL</string>\n"
                   "  <key>CFBundleVersion</key><string>" SMM_VERSION_TEXT "</string>\n"
                   "  <key>CFBundleShortVersionString</key><string>" SMM_VERSION_TEXT "</string>\n"
                   "  <key>NSHighResolutionCapable</key><true/>\n"
                   "  <key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>\n");
    integ_plist_types(b);
    plist_close(b);
}

static void put_be32(unsigned char *p, size_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void integ_icns(SmmBuf *b, const unsigned char *png, size_t n)
{
    unsigned char head[16];
    memcpy(head, "icns", 4);
    put_be32(head + 4, n + 16);
    memcpy(head + 8, "ic09", 4);
    put_be32(head + 12, n + 8);
    smm_buf_append(b, head, sizeof head);
    smm_buf_append(b, png, n);
}
