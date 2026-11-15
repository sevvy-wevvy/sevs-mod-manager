#include "internal.h"
#include <stdlib.h>

#define MF_SITE "https://monkeforge.org"
#define MF_PAGE_SIZE 100
#define MF_MAX_PAGES 20

static const char *loader_id(int kind)
{
    return kind == SMM_LOADER_MELON ? "melonloader" : "bepinex5";
}

int smm_mf_supported(void)
{
    char slug[64];
    smm_current_slug(slug, sizeof slug);
    return strcmp(slug, "gorillatag") == 0;
}

int smm_mf_active(void)
{
    return smm_mf_supported() && smm_strs_find(&smm_settings.monkeforge_games, smm_settings.game_name, 0) >= 0;
}

void smm_mf_set_active(int on)
{
    if (on && smm_mf_supported()) {
        if (smm_strs_find(&smm_settings.monkeforge_games, smm_settings.game_name, 0) < 0) smm_strs_add(&smm_settings.monkeforge_games, smm_settings.game_name);
    } else {
        smm_strs_remove(&smm_settings.monkeforge_games, smm_settings.game_name);
    }
}

static int has_loader(const SmmJson *mod, int loader)
{
    const SmmJson *l = smm_json_get(mod, "loaders");
    if (!l || l->type != SMM_J_ARR) return 1;
    for (int i = 0; i < l->n; i++)
        if (l->kids[i].type == SMM_J_STR && !strcmp(l->kids[i].str, loader_id(loader))) return 1;
    return 0;
}

int smm_mf_parse_list(const char *text, size_t n, int loader, SmmMods *out, long long *total)
{
    SmmJson *j = smm_json_parse(text, n);
    const SmmJson *items = smm_json_get(j, "items");
    if (!items || items->type != SMM_J_ARR) {
        smm_json_free(j);
        smm_set_error("Couldn't read the MonkeForge mod list");
        return -1;
    }
    if (total) *total = (long long)smm_json_num(j, "total", 0);
    int added = 0;
    for (int i = 0; i < items->n; i++) {
        const SmmJson *it = &items->kids[i];
        const char *guid = smm_json_str(it, "guid", "");
        if (!guid[0] || !has_loader(it, loader)) continue;
        const char *status = smm_json_str(it, "status", "approved");
        if (strcmp(status, "approved") != 0) continue;
        SmmMod m;
        memset(&m, 0, sizeof m);
        m.id = out->n + 1;
        smm_copy(m.name, sizeof m.name, smm_json_str(it, "name", guid));
        smm_copy(m.dll_name, sizeof m.dll_name, guid);
        smm_copy(m.author, sizeof m.author, smm_json_str(smm_json_get(it, "author"), "name", ""));
        smm_copy(m.description, sizeof m.description, smm_json_str(it, "description", ""));
        smm_copy(m.version, sizeof m.version, smm_json_str(it, "current_version", ""));
        const char *page = smm_json_str(it, "url", "");
        if (page[0]) smm_copy(m.repo_url, sizeof m.repo_url, page);
        else smm_fmt(m.repo_url, sizeof m.repo_url, "%s/view/%s", MF_SITE, smm_json_str(it, "id", guid));
        const char *logo = smm_json_str(it, "logo", "");
        if (logo[0] == '/') smm_fmt(m.icon_url, sizeof m.icon_url, "%s%s", MF_SITE, logo);
        else smm_copy(m.icon_url, sizeof m.icon_url, logo);
        smm_fmt(m.download_url, sizeof m.download_url, "%s/api/v1/mods/%s", MF_SITE, guid);
        m.upvotes = (int)smm_json_num(it, "downloads", 0);
        m.verified = 1;
        const char *choice = smm_json_str(it, "choice_category", "");
        m.featured = choice[0] != 0;
        m.source = SMM_SRC_MONKEFORGE;
        smm_mods_add(out, &m);
        added++;
    }
    smm_json_free(j);
    return added;
}

int smm_mf_list(SmmMods *out)
{
    int loader = smm_detect_loader();
    long long total = 0;
    for (int page = 1; page <= MF_MAX_PAGES; page++) {
        char url[256];
        smm_fmt(url, sizeof url, "%s/api/v1/mods?limit=%d&page=%d&sort=popular&loader=%s", MF_SITE, MF_PAGE_SIZE, page, loader_id(loader));
        SmmBuf b = {0};
        int code = smm_http_get(url, &b);
        if (code < 200 || code >= 300) {
            smm_buf_free(&b);
            if (code >= 0) smm_set_error("MonkeForge answered with HTTP %d", code);
            return -1;
        }
        int got = smm_mf_parse_list(b.p, b.n, loader, out, &total);
        smm_buf_free(&b);
        if (got < 0) return -1;
        if (got == 0 || out->n >= total) break;
    }
    return out->n;
}

int smm_mf_pick_file(const char *text, size_t n, int loader, SmmMfFile *out)
{
    SmmJson *j = smm_json_parse(text, n);
    const SmmJson *releases = smm_json_get(j, "releases");
    const SmmJson *rel = releases && releases->type == SMM_J_ARR ? smm_json_at(releases, 0) : NULL;
    const SmmJson *files = rel ? smm_json_get(rel, "downloads") : NULL;
    int found = 0;
    for (int i = 0; files && files->type == SMM_J_ARR && i < files->n && !found; i++) {
        const SmmJson *f = &files->kids[i];
        const char *l = smm_json_str(f, "loader", "bepinex5");
        const char *kind = smm_json_str(f, "kind", "");
        if (strcmp(l, loader_id(loader)) != 0) continue;
        if (strcmp(kind, "dll") != 0 && strcmp(kind, "zip") != 0) continue;
        if (!smm_json_str(f, "url", "")[0]) continue;
        memset(out, 0, sizeof *out);
        smm_copy(out->url, sizeof out->url, smm_json_str(f, "url", ""));
        smm_copy(out->filename, sizeof out->filename, smm_json_str(f, "filename", ""));
        smm_copy(out->sha256, sizeof out->sha256, smm_json_str(f, "sha256", ""));
        smm_copy(out->kind, sizeof out->kind, kind);
        smm_copy(out->version, sizeof out->version, smm_json_str(rel, "version", ""));
        found = 1;
    }
    if (!found) smm_set_error("This mod has no %s download.", loader == SMM_LOADER_MELON ? "MelonLoader" : "BepInEx");
    smm_json_free(j);
    return found;
}

static const unsigned sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static unsigned rotr(unsigned x, int n)
{
    return x >> n | x << (32 - n);
}

static void sha_block(unsigned h[8], const unsigned char *p)
{
    unsigned w[64];
    for (int i = 0; i < 16; i++) w[i] = (unsigned)p[i * 4] << 24 | (unsigned)p[i * 4 + 1] << 16 | (unsigned)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        unsigned s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    unsigned a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 64; i++) {
        unsigned t1 = k + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + sha_k[i] + w[i];
        unsigned t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}

int smm_sha256_file(const char *path, char *hex_out)
{
    FILE *f = smm_os_fopen(path, "rb");
    if (!f) return -1;
    unsigned h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    unsigned char buf[64];
    unsigned long long total = 0;
    size_t got;
    while ((got = fread(buf, 1, 64, f)) == 64) {
        sha_block(h, buf);
        total += 64;
    }
    fclose(f);
    total += got;
    buf[got++] = 0x80;
    if (got > 56) {
        memset(buf + got, 0, 64 - got);
        sha_block(h, buf);
        got = 0;
    }
    memset(buf + got, 0, 56 - got);
    unsigned long long bits = total * 8;
    for (int i = 0; i < 8; i++) buf[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    sha_block(h, buf);
    for (int i = 0; i < 8; i++) snprintf(hex_out + i * 8, 9, "%08x", h[i]);
    return 0;
}

static void lower(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s += 32;
}

int smm_mf_install(const SmmMod *m, SmmProgress *p)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return smm_op_fail(p, "No game path set.");
    int loader = smm_detect_loader();
    smm_progress(p, 0, "Looking up %s...", m->name);
    SmmBuf b = {0};
    int code = smm_http_get(m->download_url, &b);
    SmmMfFile file;
    int ok = code >= 200 && code < 300 && smm_mf_pick_file(b.p, b.n, loader, &file);
    smm_buf_free(&b);
    if (!ok) return smm_op_fail(p, "%s can't be installed: %s", m->name, code >= 200 && code < 300 ? smm_error() : "MonkeForge didn't answer.");
    char tmp[SMM_PATH], name[240];
    smm_os_temp_dir(tmp, sizeof tmp);
    smm_os_mkdir_p(tmp);
    smm_fmt(name, sizeof name, "mf_%s_%s", m->dll_name, smm_basename(file.filename));
    char path[SMM_PATH];
    smm_join(path, sizeof path, tmp, name);
    if (smm_http_download(file.url, path, p, m->name, 5, 85) != 0) return -1;
    if (file.sha256[0]) {
        char got[72], want[72];
        smm_copy(want, sizeof want, file.sha256);
        lower(want);
        if (smm_sha256_file(path, got) != 0 || strcmp(got, want) != 0) {
            smm_os_remove(path);
            return smm_op_fail(p, "The download for %s failed its checksum, nothing was installed.", m->name);
        }
    }
    if (!strcmp(file.kind, "zip")) return smm_mod_install_zip_file(path, m->dll_name, p);
    char fn[260], dest[SMM_PATH], folder[SMM_PATH];
    smm_os_mkdir_p(dir);
    smm_fmt(fn, sizeof fn, "%s.dll", m->dll_name);
    smm_join(dest, sizeof dest, dir, fn);
    smm_join(folder, sizeof folder, dir, m->dll_name);
    smm_progress(p, 95, "Installing %s...", m->name);
    smm_os_remove_tree(folder);
    smm_os_remove(dest);
    if (smm_os_move_tree(path, dest) != 0) return smm_op_fail(p, "Couldn't install %s", m->name);
    smm_progress(p, 100, "%s installed.", m->name);
    return 0;
}

int smm_mf_is_installed(const SmmMod *m)
{
    char dir[SMM_PATH], p[SMM_PATH], fn[260];
    if (!smm_mods_install_dir(dir, sizeof dir)) return 0;
    smm_fmt(fn, sizeof fn, "%s.dll", m->dll_name);
    smm_join(p, sizeof p, dir, fn);
    if (smm_os_is_file(p)) return 1;
    smm_join(p, sizeof p, dir, m->dll_name);
    return smm_os_is_dir(p) && smm_os_count_ext(p, ".dll", 1) > 0;
}

int smm_mf_is_disabled(const SmmMod *m)
{
    char dir[SMM_PATH], p[SMM_PATH], fn[270];
    if (!smm_mods_install_dir(dir, sizeof dir)) return 0;
    smm_fmt(fn, sizeof fn, "%s.dll.disabled", m->dll_name);
    smm_join(p, sizeof p, dir, fn);
    if (smm_os_is_file(p)) return 1;
    smm_join(p, sizeof p, dir, m->dll_name);
    return smm_os_is_dir(p) && smm_os_count_ext(p, ".dll.disabled", 1) > 0;
}
