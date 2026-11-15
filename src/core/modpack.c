#include "internal.h"
#include <stdlib.h>

#define MANIFEST_ENTRY "smmpack.json"

int smm_pack_roots(SmmPackRoot *out, int max)
{
    char game[SMM_PATH];
    int n = 0;
    if (!smm_game_dir(game, sizeof game)) return 0;
    if (smm_detect_loader() == SMM_LOADER_MELON) {
        const char *names[] = {"Mods", "UserData", "Plugins", "MelonLoader"};
        for (int i = 0; i < 4 && n < max; i++) {
            smm_copy(out[n].name, sizeof out[n].name, names[i]);
            smm_fmt(out[n].prefix, sizeof out[n].prefix, "%s/", names[i]);
            smm_join(out[n].abs_path, sizeof out[n].abs_path, game, names[i]);
            n++;
        }
    } else if (max > 0) {
        smm_copy(out[0].name, sizeof out[0].name, "BepInEx");
        smm_copy(out[0].prefix, sizeof out[0].prefix, "BepInEx/");
        smm_join(out[0].abs_path, sizeof out[0].abs_path, game, "BepInEx");
        n = 1;
    }
    return n;
}

void smm_pack_default_roots(SmmStrs *out)
{
    if (smm_detect_loader() == SMM_LOADER_MELON) {
        smm_strs_add(out, "Mods");
        smm_strs_add(out, "UserData");
    } else {
        smm_strs_add(out, "plugins");
        smm_strs_add(out, "config");
    }
}

int smm_pack_read_manifest(const char *path, SmmManifest *out)
{
    memset(out, 0, sizeof *out);
    out->loader_kind = -1;
    SmmZip z;
    if (smm_zip_open_file(&z, path) != 0) return -1;
    int i = smm_zip_find(&z, MANIFEST_ENTRY);
    SmmBuf b = {0};
    int rc = -1;
    if (i >= 0 && smm_zip_extract_mem(&z, i, &b) == 0) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        if (j && j->type == SMM_J_OBJ) {
            smm_copy(out->name, sizeof out->name, smm_json_str(j, "Name", ""));
            smm_copy(out->author, sizeof out->author, smm_json_str(j, "Author", ""));
            smm_copy(out->game_slug, sizeof out->game_slug, smm_json_str(j, "GameSlug", ""));
            smm_copy(out->created_utc, sizeof out->created_utc, smm_json_str(j, "CreatedUtc", ""));
            const SmmJson *lk = smm_json_get(j, "LoaderKind");
            if (lk && lk->type == SMM_J_NUM) out->loader_kind = (int)lk->num;
            const SmmJson *mods = smm_json_get(j, "Mods");
            if (mods && mods->type == SMM_J_ARR)
                for (int k = 0; k < mods->n; k++)
                    if (mods->kids[k].type == SMM_J_STR) smm_strs_add(&out->mods, mods->kids[k].str);
            rc = 0;
        }
        smm_json_free(j);
    }
    smm_buf_free(&b);
    smm_zip_close(&z);
    return rc;
}

static int cmp_packs(const void *a, const void *b)
{
    char x[256], y[256];
    smm_pack_display_name(a, x, sizeof x);
    smm_pack_display_name(b, y, sizeof y);
    return smm_strcasecmp(x, y);
}

int smm_packs_list(SmmPacks *out)
{
    char dir[SMM_PATH];
    smm_packs_dir(dir, sizeof dir);
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return 0;
    for (int i = 0; i < l.n; i++) {
        if (l.v[i].is_dir || !smm_ends_with_nocase(l.v[i].name, ".smmpack")) continue;
        char f[SMM_PATH];
        smm_join(f, sizeof f, dir, l.v[i].name);
        SmmManifest m;
        if (smm_pack_read_manifest(f, &m) != 0) continue;
        if (out->n == out->cap) {
            out->cap = out->cap ? out->cap * 2 : 8;
            out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmPack));
        }
        SmmPack *p = &out->v[out->n++];
        smm_copy(p->path, sizeof p->path, f);
        p->manifest = m;
    }
    smm_dirlist_free(&l);
    if (out->n > 1) qsort(out->v, (size_t)out->n, sizeof(SmmPack), cmp_packs);
    return out->n;
}

int smm_pack_entries(const char *path, SmmStrs *out)
{
    SmmZip z;
    if (smm_zip_open_file(&z, path) != 0) return -1;
    for (int i = 0; i < z.n; i++) {
        size_t l = strlen(z.e[i].name);
        if (l == 0 || z.e[i].name[l - 1] == '/' || !strcmp(z.e[i].name, MANIFEST_ENTRY)) continue;
        smm_strs_add(out, z.e[i].name);
    }
    smm_zip_close(&z);
    smm_sort_strs_nocase(out);
    return 0;
}

void smm_current_mod_names(SmmStrs *out)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir) || !smm_os_is_dir(dir)) return;
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return;
    for (int i = 0; i < l.n; i++) {
        if (!l.v[i].is_dir) {
            if (!smm_ends_with_nocase(l.v[i].name, ".dll")) continue;
            char s[SMM_PATH];
            smm_copy(s, sizeof s, l.v[i].name);
            s[strlen(s) - 4] = 0;
            smm_strs_add(out, s);
        }
    }
    for (int i = 0; i < l.n; i++) {
        if (!l.v[i].is_dir) continue;
        char sub[SMM_PATH];
        smm_join(sub, sizeof sub, dir, l.v[i].name);
        if (smm_os_count_ext(sub, ".dll", 1) > 0) smm_strs_add(out, l.v[i].name);
    }
    smm_dirlist_free(&l);
}

int smm_setup_matches_saved_pack(void)
{
    SmmStrs cur = {0};
    smm_current_mod_names(&cur);
    SmmPacks packs = {0};
    smm_packs_list(&packs);
    int match = 0;
    for (int i = 0; i < packs.n && !match; i++) {
        const SmmStrs *m = &packs.v[i].manifest.mods;
        int same = 1;
        for (int k = 0; k < cur.n && same; k++)
            if (smm_strs_find(m, cur.v[k], 1) < 0) same = 0;
        for (int k = 0; k < m->n && same; k++)
            if (smm_strs_find(&cur, m->v[k], 1) < 0) same = 0;
        match = same;
    }
    smm_packs_free(&packs);
    smm_strs_free(&cur);
    return match;
}

static void remove_roots(void)
{
    SmmPackRoot roots[8];
    int n = smm_pack_roots(roots, 8);
    for (int i = 0; i < n; i++) smm_os_remove_tree(roots[i].abs_path);
}


int smm_pack_blank_setup(SmmProgress *p)
{
    char game[SMM_PATH];
    if (!smm_game_dir(game, sizeof game)) return smm_op_fail(p, "No game path set.");
    int loader = smm_detect_loader();
    smm_progress(p, 0, "Removing current setup...");
    remove_roots();
    return smm_loader_install(loader, game, p);
}

static void safe_file_name(const char *name, char *out, size_t n)
{
    size_t k = 0;
    for (const char *p = name; *p && k + 1 < n; p++) {
        unsigned char c = (unsigned char)*p;
        out[k++] = (c < 32 || strchr("<>:\"/\\|?*", c)) ? '_' : (char)c;
    }
    out[k] = 0;
    if (!out[0]) smm_copy(out, n, "Modpack");
}

static void write_manifest(SmmBuf *b, const char *name, const char *author, const SmmStrs *mods)
{
    char slug[96], when[40];
    smm_current_slug(slug, sizeof slug);
    smm_iso_time(smm_os_now(), when, sizeof when);
    smm_buf_append(b, "{\n  \"Name\": ", 12);
    smm_json_escape(b, name);
    smm_buf_append(b, ",\n  \"Author\": ", 14);
    smm_json_escape(b, author);
    smm_buf_append(b, ",\n  \"GameSlug\": ", 16);
    smm_json_escape(b, slug);
    smm_buf_printf(b, ",\n  \"CreatedUtc\": \"%s\",\n  \"Mods\": [", when);
    for (int i = 0; i < mods->n; i++) {
        smm_buf_append(b, i ? ",\n    " : "\n    ", i ? 6 : 5);
        smm_json_escape(b, mods->v[i]);
    }
    smm_buf_printf(b, "%s],\n  \"LoaderKind\": %d\n}", mods->n ? "\n  " : "", smm_detect_loader());
}

static int under_root(const char *file, const SmmPackRoot *r)
{
    size_t l = strlen(r->abs_path);
    return strncmp(file, r->abs_path, l) == 0 && (file[l] == '/' || file[l] == '\\');
}

static int write_pack(const char *name, const char *author, const SmmStrs *files, char *dest_out, size_t n, SmmProgress *p)
{
    SmmPackRoot roots[8];
    int nr = smm_pack_roots(roots, 8);
    if (nr == 0) return smm_op_fail(p, "No mod loader folder found.");
    char dir[SMM_PATH], safe[256], fn[300], dest[SMM_PATH];
    smm_packs_dir(dir, sizeof dir);
    smm_os_mkdir_p(dir);
    safe_file_name(name, safe, sizeof safe);
    smm_fmt(fn, sizeof fn, "%s.smmpack", safe);
    smm_join(dest, sizeof dest, dir, fn);
    smm_os_remove(dest);
    SmmStrs mods = {0};
    smm_current_mod_names(&mods);
    SmmBuf mf = {0};
    write_manifest(&mf, name, author, &mods);
    smm_strs_free(&mods);
    smm_progress(p, 0, "Scanning files...");
    SmmZipWriter w;
    if (smm_zipw_open(&w, dest) != 0) {
        smm_buf_free(&mf);
        return -1;
    }
    int rc = smm_zipw_add_mem(&w, MANIFEST_ENTRY, mf.p, mf.n);
    smm_buf_free(&mf);
    for (int i = 0; i < files->n && rc == 0; i++) {
        const char *f = files->v[i];
        const SmmPackRoot *root = NULL;
        for (int r = 0; r < nr; r++)
            if (under_root(f, &roots[r])) root = &roots[r];
        if (!root) continue;
        char rel[SMM_PATH], entry[SMM_PATH];
        smm_copy(rel, sizeof rel, f + strlen(root->abs_path) + 1);
        for (char *c = rel; *c; c++)
            if (*c == '\\') *c = '/';
        smm_fmt(entry, sizeof entry, "%s%s", root->prefix, rel);
        rc = smm_zipw_add_file(&w, entry, f);
        smm_progress(p, files->n ? (i + 1) * 100 / files->n : 100, "Zipping %s...", smm_basename(f));
    }
    int rc2 = smm_zipw_close(&w);
    if (rc != 0 || rc2 != 0) {
        smm_os_remove(dest);
        return -1;
    }
    if (dest_out) smm_copy(dest_out, n, dest);
    smm_progress(p, 100, "Modpack saved.");
    return 0;
}

int smm_pack_files_for_export(SmmStrs *abs_paths)
{
    SmmPackRoot roots[8];
    int nr = smm_pack_roots(roots, 8);
    for (int i = 0; i < nr; i++) smm_os_walk_files(roots[i].abs_path, abs_paths);
    smm_sort_strs_nocase(abs_paths);
    return abs_paths->n;
}

int smm_pack_save(const char *name, const char *author, const SmmStrs *only, char *dest_out, size_t n, SmmProgress *p)
{
    if (only) {
        SmmStrs keep = {0};
        for (int i = 0; i < only->n; i++)
            if (smm_os_is_file(only->v[i])) smm_strs_add(&keep, only->v[i]);
        int rc = write_pack(name, author, &keep, dest_out, n, p);
        smm_strs_free(&keep);
        return rc;
    }
    SmmStrs all = {0};
    smm_pack_files_for_export(&all);
    int rc = write_pack(name, author, &all, dest_out, n, p);
    smm_strs_free(&all);
    return rc;
}

static int safe_rel(const char *rel)
{
    if (!*rel || rel[0] == '/' || rel[0] == '\\' || strchr(rel, ':')) return 0;
    for (const char *p = rel; *p; p++)
        if ((p == rel || p[-1] == '/' || p[-1] == '\\') && p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\\' || !p[2])) return 0;
    return 1;
}

int smm_pack_apply(const char *pack_path, SmmProgress *p)
{
    char game[SMM_PATH];
    if (!smm_game_dir(game, sizeof game)) return smm_op_fail(p, "No game path set.");
    int loader = smm_detect_loader();
    SmmPackRoot roots[8];
    int nr = smm_pack_roots(roots, 8);
    if (nr == 0) return smm_op_fail(p, "No mod loader folder found.");
    SmmZip z;
    if (smm_zip_open_file(&z, pack_path) != 0) return -1;
    smm_progress(p, 0, "Removing current setup...");
    remove_roots();
    if (smm_loader_install(loader, game, p) != 0) {
        smm_zip_close(&z);
        return -1;
    }
    int total = 0;
    for (int i = 0; i < z.n; i++) {
        size_t l = strlen(z.e[i].name);
        if (l && z.e[i].name[l - 1] != '/') total++;
    }
    int done = 0, rc = 0;
    for (int i = 0; i < z.n && rc == 0; i++) {
        const char *name = z.e[i].name;
        size_t l = strlen(name);
        if (!l || name[l - 1] == '/') continue;
        for (int r = 0; r < nr; r++) {
            size_t pl = strlen(roots[r].prefix);
            if (!smm_starts_with_nocase(name, roots[r].prefix) || l == pl) continue;
            const char *rel = name + pl;
            if (!safe_rel(rel)) break;
            char dest[SMM_PATH];
            smm_join(dest, sizeof dest, roots[r].abs_path, rel);
            if (smm_zip_extract_file(&z, i, dest) != 0) rc = -1;
            break;
        }
        done++;
        smm_progress(p, 40 + (total ? done * 60 / total : 60), "Extracting %s...", smm_basename(name));
    }
    smm_zip_close(&z);
    if (rc != 0) return smm_op_fail(p, "Couldn't unpack the modpack.");
    smm_progress(p, 100, "Modpack applied.");
    return 0;
}

int smm_pack_delete(const char *pack_path)
{
    return smm_os_remove(pack_path);
}

int smm_pack_export(const char *pack_path, const char *dest)
{
    return smm_os_copy(pack_path, dest);
}

int smm_pack_import(const char *src, char *dest_out, size_t n)
{
    char dir[SMM_PATH], dest[SMM_PATH];
    smm_packs_dir(dir, sizeof dir);
    smm_os_mkdir_p(dir);
    smm_join(dest, sizeof dest, dir, smm_basename(src));
    if (smm_os_exists(dest)) {
        char stem[256], fn[300];
        smm_stem(src, stem, sizeof stem);
        smm_fmt(fn, sizeof fn, "%s_%06lld.smmpack", stem, smm_os_now() % 1000000);
        smm_join(dest, sizeof dest, dir, fn);
    }
    if (smm_os_copy(src, dest) != 0) {
        smm_set_error("Couldn't import %s", smm_basename(src));
        return -1;
    }
    if (dest_out) smm_copy(dest_out, n, dest);
    return 0;
}
