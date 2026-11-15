#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui/integrate.h"

static int fails;

#define check(c, w) check_(!!(c), w)

static void check_(int ok, const char *what)
{
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static int count(const char *s, const char *needle)
{
    int n = 0;
    for (const char *p = s; (p = strstr(p, needle)); p += strlen(needle)) n++;
    return n;
}

static int unquote_after(const char *s, const char *marker, char *out)
{
    const char *p = strstr(s, marker);
    if (!p) return 0;
    p += strlen(marker);
    if (*p++ != '"') return 0;
    while (*p && *p != '\n') {
        if (*p == '\\') {
            p++;
            *out++ = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r' : *p;
            p++;
        } else if (*p == '"') {
            *out = 0;
            return p[1] == '\n';
        } else *out++ = *p++;
    }
    return 0;
}

static void test_script(const char *exe)
{
    SmmBuf b = {0};
    integ_applet_source(&b, exe);
    char got[2048];
    check(unquote_after(b.p, "set exe to ", got) && strcmp(got, exe) == 0, "exe literal round trips");
    check(strstr(b.p, "on open theFiles") && strstr(b.p, "on run") && strstr(b.p, "on launchSmm(args)"), "handlers present");
    check(strstr(b.p, "quoted form of (item i of args)"), "args are shell quoted");
    check(strstr(b.p, "do shell script cmd & \" >/dev/null 2>&1 &\""), "launch detached");
    check(strstr(b.p, "POSIX path of (item i of theFiles)"), "posix paths");
    check(count(b.p, "\n\tend try") == 1 && count(b.p, "\ttry\n") == 1, "try balanced");
    check(count(b.p, "\nend ") + (strncmp(b.p, "end ", 4) == 0) == 3, "three handlers closed");
    smm_buf_free(&b);
}

static void test_plist(void)
{
    SmmBuf f = {0}, full = {0};
    integ_plist_fragment(&f);
    integ_plist_full(&full);
    SmmBuf all[2] = {f, full};
    for (int i = 0; i < 2; i++) {
        const char *t = all[i].p;
        check(count(t, "<dict>") == count(t, "</dict>"), "dict balanced");
        check(count(t, "<array>") == count(t, "</array>"), "array balanced");
        check(strstr(t, "<key>CFBundleIdentifier</key><string>com.sevvywevvy.sevsmodmanager</string>"), "bundle id");
        check(strstr(t, "<key>CFBundleIconFile</key><string>icon</string>"), "icon file");
        check(strstr(t, "<string>smmpack</string>"), "extension");
        check(strstr(t, "UTExportedTypeDeclarations") && strstr(t, "CFBundleDocumentTypes"), "types");
        check(strncmp(t, "<?xml", 5) == 0 && strstr(t, "</plist>\n"), "plist framing");
    }
    check(strstr(f.p, "<key>LSUIElement</key><true/>") != NULL, "fragment hides dock icon");
    check(strstr(full.p, "<key>CFBundleExecutable</key><string>smm</string>") != NULL, "full has executable");
    smm_buf_free(&f);
    smm_buf_free(&full);
}

static void test_icns(void)
{
    unsigned char png[300];
    for (int i = 0; i < 300; i++) png[i] = (unsigned char)i;
    SmmBuf b = {0};
    integ_icns(&b, png, sizeof png);
    const unsigned char *p = (const unsigned char *)b.p;
    check(b.n == 316, "icns size");
    check(memcmp(p, "icns", 4) == 0 && memcmp(p + 8, "ic09", 4) == 0, "icns tags");
    check(((p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7]) == 316, "icns total length");
    check(((p[12] << 24) | (p[13] << 16) | (p[14] << 8) | p[15]) == 308, "icns chunk length");
    check(memcmp(p + 16, png, sizeof png) == 0, "icns payload");
    smm_buf_free(&b);
}

int main(void)
{
    test_script("/Users/sev/.local/bin/smm");
    test_script("/Users/o'brien/My Apps/smm \"x\" \\ $HOME `id`");
    test_plist();
    test_icns();
    if (!fails) printf("integrate tests passed\n");
    return fails ? 1 : 0;
}
