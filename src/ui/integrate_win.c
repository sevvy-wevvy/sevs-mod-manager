#ifdef _WIN32
#include "integrate.h"
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../core/internal.h"

#define PROG_ID L"SevsModManager.smmpack"
#define CLASSES L"Software\\Classes\\"

static const GUID guid_shell_link = {0x00021401, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID guid_shell_link_w = {0x000214F9, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID guid_persist_file = {0x0000010B, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

static wchar_t *wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = smm_xmalloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static int set_value(const wchar_t *key, const wchar_t *name, const wchar_t *value)
{
    HKEY k;
    wchar_t path[512];
    swprintf(path, 512, CLASSES L"%ls", key);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return -1;
    LONG rc = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return rc == ERROR_SUCCESS ? 0 : -1;
}

static int shortcut_path(wchar_t *out)
{
    wchar_t dir[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, dir) != S_OK) return -1;
    swprintf(out, MAX_PATH + 64, L"%ls\\" L"Sevs Mod Manager.lnk", dir);
    return 0;
}

static int make_shortcut(const wchar_t *exe)
{
    wchar_t lnk[MAX_PATH + 64];
    if (shortcut_path(lnk) != 0) return -1;
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IShellLinkW *sl = NULL;
    int rc = -1;
    if (SUCCEEDED(CoCreateInstance(&guid_shell_link, NULL, CLSCTX_INPROC_SERVER, &guid_shell_link_w, (void **)&sl))) {
        wchar_t dir[MAX_PATH];
        wcsncpy(dir, exe, MAX_PATH - 1);
        dir[MAX_PATH - 1] = 0;
        wchar_t *slash = wcsrchr(dir, L'\\');
        if (slash) *slash = 0;
        sl->lpVtbl->SetPath(sl, exe);
        sl->lpVtbl->SetWorkingDirectory(sl, dir);
        sl->lpVtbl->SetIconLocation(sl, exe, 0);
        sl->lpVtbl->SetDescription(sl, L"Mod manager for Unity games");
        IPersistFile *pf = NULL;
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &guid_persist_file, (void **)&pf))) {
            if (SUCCEEDED(pf->lpVtbl->Save(pf, lnk, TRUE))) rc = 0;
            pf->lpVtbl->Release(pf);
        }
        sl->lpVtbl->Release(sl);
    }
    if (SUCCEEDED(init)) CoUninitialize();
    return rc;
}

int integ_os_register(const char *exe, const unsigned char *png, size_t png_len)
{
    (void)png;
    (void)png_len;
    wchar_t *we = wide(exe);
    size_t len = wcslen(we) + 32;
    wchar_t *icon = smm_xmalloc(len * sizeof(wchar_t));
    wchar_t *cmd = smm_xmalloc((len + 16) * sizeof(wchar_t));
    swprintf(icon, len, L"\"%ls\",0", we);
    swprintf(cmd, len + 16, L"\"%ls\" \"%%1\"", we);
    int rc = 0;
    rc |= set_value(L".smmpack", NULL, PROG_ID);
    rc |= set_value(L".smmpack", L"Content Type", L"application/x-smmpack");
    rc |= set_value(PROG_ID, NULL, L"Sev's Mod Manager Pack");
    rc |= set_value(PROG_ID, L"FriendlyTypeName", L"Sev's Mod Manager Pack");
    rc |= set_value(PROG_ID L"\\DefaultIcon", NULL, icon);
    rc |= set_value(PROG_ID L"\\shell\\open", NULL, L"Open with Sevs Mod Manager");
    rc |= set_value(PROG_ID L"\\shell\\open\\command", NULL, cmd);
    if (make_shortcut(we) != 0) rc |= -1;
    free(we);
    free(icon);
    free(cmd);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return rc ? -1 : 0;
}

int integ_os_unregister(void)
{
    RegDeleteTreeW(HKEY_CURRENT_USER, CLASSES L".smmpack");
    RegDeleteTreeW(HKEY_CURRENT_USER, CLASSES PROG_ID);
    wchar_t lnk[MAX_PATH + 64];
    if (shortcut_path(lnk) == 0) DeleteFileW(lnk);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}
#endif
