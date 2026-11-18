#include "internal.h"
#include <stdlib.h>

static int find_child_ci(const char *dir, const char *name, char *out, size_t n)
{
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return 0;
    int found = 0;
    for (int i = 0; i < l.n && !found; i++)
        if (l.v[i].is_dir && smm_strcasecmp(l.v[i].name, name) == 0) {
            smm_join(out, n, dir, l.v[i].name);
            found = 1;
        }
    smm_dirlist_free(&l);
    return found;
}

static int cmp_installed(const void *a, const void *b)
{
    return smm_strcasecmp(((const SmmInstalled *)a)->name, ((const SmmInstalled *)b)->name);
}

static void add_installed(SmmInstalledList *l, const SmmInstalled *m)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 32;
        l->v = smm_xrealloc(l->v, (size_t)l->cap * sizeof(SmmInstalled));
    }
    l->v[l->n++] = *m;
}

static void scan_flat(const char *dir, SmmInstalledList *out)
{
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return;
    for (int i = 0; i < l.n; i++) {
        if (l.v[i].is_dir) continue;
        const char *fn = l.v[i].name;
        SmmInstalled m;
        memset(&m, 0, sizeof m);
        smm_join(m.path, sizeof m.path, dir, fn);
        m.size_bytes = l.v[i].size;
        if (smm_ends_with_nocase(fn, ".dll")) {
            smm_copy(m.name, sizeof m.name, fn);
            m.name[strlen(m.name) - 4] = 0;
            m.enabled = 1;
        } else if (smm_ends_with_nocase(fn, ".dll.disabled")) {
            smm_copy(m.name, sizeof m.name, fn);
            m.name[strlen(m.name) - 13] = 0;
            m.enabled = 0;
        } else {
            continue;
        }
        add_installed(out, &m);
    }
    smm_dirlist_free(&l);
}

static void scan_folders(const char *dir, SmmInstalledList *out)
{
    SmmDirList l = {0};
    if (smm_os_list(dir, &l) != 0) return;
    for (int i = 0; i < l.n; i++) {
        if (!l.v[i].is_dir || smm_ends_with_nocase(l.v[i].name, ".delete")) continue;
        char sub[SMM_PATH];
        smm_join(sub, sizeof sub, dir, l.v[i].name);
        int active = smm_os_count_ext(sub, ".dll", 1);
        int disabled = smm_os_count_ext(sub, ".dll.disabled", 1);
        if (active + disabled == 0) continue;
        SmmInstalled m;
        memset(&m, 0, sizeof m);
        smm_copy(m.name, sizeof m.name, l.v[i].name);
        smm_copy(m.path, sizeof m.path, sub);
        m.enabled = active > 0;
        m.is_folder = 1;
        m.size_bytes = smm_os_tree_size(sub);
        add_installed(out, &m);
    }
    smm_dirlist_free(&l);
}

int smm_installed_scan(SmmInstalledList *out)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir) || !smm_os_is_dir(dir)) return 0;
    scan_flat(dir, out);
    scan_folders(dir, out);
    char bep[SMM_PATH];
    if (smm_detect_loader() == SMM_LOADER_BEPINEX && smm_bepinex_dir(bep, sizeof bep)) {
        char patchers[SMM_PATH];
        smm_join(patchers, sizeof patchers, bep, "patchers");
        if (smm_os_is_dir(patchers)) {
            scan_flat(patchers, out);
            scan_folders(patchers, out);
        }
    }
    SmmBuf b = {0};
    char vp[SMM_PATH], sb[SMM_PATH], sd[SMM_PATH];
    SmmMap vers = {0};
    if (smm_sb_dir(sb, sizeof sb)) {
        smm_join(sd, sizeof sd, sb, "settings");
        smm_join(vp, sizeof vp, sd, "mod_versions.json");
        if (smm_file_read(vp, &b) == 0) {
            SmmJson *j = smm_json_parse(b.p, b.n);
            if (j && j->type == SMM_J_OBJ)
                for (int i = 0; i < j->n; i++)
                    if (j->kids[i].type == SMM_J_STR) smm_map_set(&vers, j->kids[i].key, j->kids[i].str);
            smm_json_free(j);
        }
        smm_buf_free(&b);
    }
    for (int i = 0; i < out->n; i++) {
        const char *v = smm_map_get(&vers, out->v[i].name);
        smm_copy(out->v[i].known_version, sizeof out->v[i].known_version, v ? v : "");
    }
    smm_map_free(&vers);
    if (out->n > 1) qsort(out->v, (size_t)out->n, sizeof(SmmInstalled), cmp_installed);
    return out->n;
}

static void temp_file(const char *name, char *out, size_t n)
{
    char t[SMM_PATH];
    smm_os_temp_dir(t, sizeof t);
    smm_os_mkdir_p(t);
    smm_join(out, n, t, name);
}

int smm_mod_install_dll(const char *url, const char *dll_name, SmmProgress *p)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return smm_op_fail(p, "No game path set.");
    smm_os_mkdir_p(dir);
    char fn[200], dest[SMM_PATH], tmp[SMM_PATH];
    smm_fmt(fn, sizeof fn, "%s.dll", dll_name);
    smm_join(dest, sizeof dest, dir, fn);
    temp_file(fn, tmp, sizeof tmp);
    smm_progress(p, 0, "Downloading %s...", dll_name);
    if (smm_http_download(url, tmp, p, dll_name, 0, 90) != 0) return -1;
    smm_progress(p, 95, "Installing %s...", dll_name);
    smm_os_remove(dest);
    if (smm_os_move_tree(tmp, dest) != 0) return smm_op_fail(p, "Couldn't install %s", dll_name);
    smm_progress(p, 100, "%s installed.", dll_name);
    return 0;
}

static const char *const chains[][3] = {
    {"BepInEx", "plugins", NULL},
    {"BepInEx", "patchers", NULL},
    {"MelonLoader", "Mods", NULL},
    {"BepInEx", NULL, NULL},
    {"MelonLoader", NULL, NULL},
    {"Mods", NULL, NULL},
    {"Plugins", NULL, NULL},
};

static void unwrap_known_root(const char *root, char *out, size_t n)
{
    smm_copy(out, n, root);
    char cur[SMM_PATH];
    for (size_t c = 0; c < sizeof chains / sizeof chains[0]; c++) {
        smm_copy(cur, sizeof cur, root);
        int matched = 1;
        for (int k = 0; k < 3 && chains[c][k]; k++) {
            char next[SMM_PATH];
            if (!find_child_ci(cur, chains[c][k], next, sizeof next)) {
                matched = 0;
                break;
            }
            smm_copy(cur, sizeof cur, next);
        }
        if (matched) {
            smm_copy(out, n, cur);
            break;
        }
    }
    if (strcmp(out, root) != 0) {
        SmmDirList l = {0};
        smm_os_list(out, &l);
        if (l.n == 1 && l.v[0].is_dir) {
            char sub[SMM_PATH];
            smm_join(sub, sizeof sub, out, l.v[0].name);
            smm_copy(out, n, sub);
        }
        smm_dirlist_free(&l);
    }
}

int smm_mod_install_zip_file(const char *zip, const char *folder_name, SmmProgress *p)
{
    int loader = smm_detect_loader();
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return smm_op_fail(p, "No game path set.");
    smm_os_mkdir_p(dir);
    char staging[SMM_PATH], stage_name[64];
    smm_fmt(stage_name, sizeof stage_name, "smm_pkg_%lld_%d", smm_os_now(), rand() % 100000);
    temp_file(stage_name, staging, sizeof staging);
    smm_progress(p, 95, "Extracting %s...", folder_name);
    SmmZip z;
    if (smm_zip_open_file(&z, zip) != 0) {
        smm_os_remove(zip);
        return -1;
    }
    int rc = smm_zip_extract_all(&z, staging);
    smm_zip_close(&z);
    if (rc != 0) {
        smm_os_remove(zip);
        smm_os_remove_tree(staging);
        return smm_op_fail(p, "Couldn't unpack %s", folder_name);
    }
    char content[SMM_PATH];
    unwrap_known_root(staging, content, sizeof content);
    if (loader == SMM_LOADER_MELON) {
        SmmStrs files = {0};
        smm_os_walk_files(content, &files);
        for (int i = 0; i < files.n; i++) {
            if (!smm_ends_with_nocase(files.v[i], ".dll")) continue;
            char dest[SMM_PATH];
            smm_join(dest, sizeof dest, dir, smm_basename(files.v[i]));
            smm_os_copy(files.v[i], dest);
        }
        smm_strs_free(&files);
    } else {
        char dest[SMM_PATH];
        smm_join(dest, sizeof dest, dir, folder_name);
        smm_os_remove_tree(dest);
        if (smm_os_move_tree(content, dest) != 0) rc = -1;
    }
    smm_os_remove(zip);
    smm_os_remove_tree(staging);
    if (rc != 0) return smm_op_fail(p, "Couldn't install %s", folder_name);
    smm_progress(p, 100, "%s installed.", folder_name);
    return 0;
}

int smm_mod_install_thunderstore(const char *url, const char *folder_name, SmmProgress *p)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return smm_op_fail(p, "No game path set.");
    smm_os_mkdir_p(dir);
    char zipname[220], zip[SMM_PATH];
    smm_fmt(zipname, sizeof zipname, "%s.zip", folder_name);
    temp_file(zipname, zip, sizeof zip);
    smm_progress(p, 0, "Downloading %s...", folder_name);
    if (smm_http_download(url, zip, p, folder_name, 0, 90) != 0) return -1;
    return smm_mod_install_zip_file(zip, folder_name, p);
}

int smm_mod_install_local(const char *dll_path)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) {
        smm_set_error("No game path set.");
        return -1;
    }
    smm_os_mkdir_p(dir);
    char stem[200], fn[220], dest[SMM_PATH];
    smm_stem(dll_path, stem, sizeof stem);
    smm_fmt(fn, sizeof fn, "%s.dll", stem);
    smm_join(dest, sizeof dest, dir, fn);
    return smm_os_copy(dll_path, dest);
}

static int trash(const char *path)
{
    char t[SMM_PATH];
    smm_fmt(t, sizeof t, "%s.delete", path);
    smm_os_remove_tree(t);
    return smm_os_rename(path, t);
}

int smm_mod_uninstall(const char *name)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return -1;
    char folder[SMM_PATH];
    smm_join(folder, sizeof folder, dir, name);
    if (smm_os_is_dir(folder)) {
        int rc = trash(folder);
        smm_mod_version_delete(name);
        return rc;
    }
    char fn[220], dll[SMM_PATH], dis[SMM_PATH];
    smm_fmt(fn, sizeof fn, "%s.dll", name);
    smm_join(dll, sizeof dll, dir, fn);
    smm_fmt(dis, sizeof dis, "%s.disabled", dll);
    int rc = 0;
    if (smm_os_is_file(dll)) rc = trash(dll);
    else if (smm_os_is_file(dis)) rc = trash(dis);
    smm_mod_version_delete(name);
    return rc;
}

static int set_folder_enabled(const char *folder, int enabled)
{
    SmmStrs files = {0};
    smm_os_walk_files(folder, &files);
    for (int i = 0; i < files.n; i++) {
        const char *f = files.v[i];
        char target[SMM_PATH];
        if (enabled) {
            if (!smm_ends_with_nocase(f, ".dll.disabled")) continue;
            smm_copy(target, sizeof target, f);
            target[strlen(target) - 9] = 0;
        } else {
            if (!smm_ends_with_nocase(f, ".dll")) continue;
            smm_fmt(target, sizeof target, "%s.disabled", f);
        }
        smm_os_remove(target);
        smm_os_rename(f, target);
    }
    smm_strs_free(&files);
    return 0;
}

static int toggle(const char *name, int enable)
{
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return -1;
    char folder[SMM_PATH];
    smm_join(folder, sizeof folder, dir, name);
    if (smm_os_is_dir(folder)) return set_folder_enabled(folder, enable);
    char fn[220], dll[SMM_PATH], dis[SMM_PATH];
    smm_fmt(fn, sizeof fn, "%s.dll", name);
    smm_join(dll, sizeof dll, dir, fn);
    smm_fmt(dis, sizeof dis, "%s.disabled", dll);
    if (enable && smm_os_is_file(dis)) {
        smm_os_remove(dll);
        return smm_os_rename(dis, dll);
    }
    if (!enable && smm_os_is_file(dll)) {
        smm_os_remove(dis);
        return smm_os_rename(dll, dis);
    }
    return 0;
}

int smm_mod_enable(const char *name)
{
    return toggle(name, 1);
}

int smm_mod_disable(const char *name)
{
    return toggle(name, 0);
}

void smm_melon_key(const char *full, char *out, size_t n)
{
    const char *d = strchr(full, '-');
    smm_copy(out, n, d ? d + 1 : full);
}

void smm_mod_key(const SmmMod *m, char *out, size_t n)
{
    if (m->source == SMM_SRC_THUNDERSTORE) {
        if (smm_detect_loader() == SMM_LOADER_MELON) smm_melon_key(m->dll_name, out, n);
        else smm_copy(out, n, m->dll_name);
        return;
    }
    smm_copy(out, n, m->dll_name);
    size_t l = strlen(out);
    while (l >= 4) {
        const char *q = strstr(out, ".dll");
        if (!q) break;
        memmove((char *)q, q + 4, strlen(q + 4) + 1);
        l = strlen(out);
    }
}

int smm_mod_download_url(const SmmMod *m, char *out, size_t n)
{
    if (m->source == SMM_SRC_THUNDERSTORE || m->source == SMM_SRC_MONKEFORGE) {
        smm_copy(out, n, m->download_url);
        return m->download_url[0] != 0;
    }
    if (!m->repo_url[0] || !m->dll_name[0]) {
        out[0] = 0;
        return 0;
    }
    char repo[300];
    smm_copy(repo, sizeof repo, m->repo_url);
    size_t l = strlen(repo);
    while (l > 0 && repo[l - 1] == '/') repo[--l] = 0;
    smm_fmt(out, n, "%s/releases/latest/download/%s", repo, m->dll_name);
    return 1;
}

static int unsafe_component(const char *s)
{
    return !*s || strpbrk(s, "/\\") || !strcmp(s, ".") || !strcmp(s, "..");
}

int smm_mod_install(const SmmMod *m, SmmProgress *p)
{
    char url[600], key[200];
    if (!smm_mod_download_url(m, url, sizeof url)) return smm_op_fail(p, "%s has no download link.", m->name);
    if (unsafe_component(m->dll_name)) return smm_op_fail(p, "%s has an invalid file name.", m->name);
    smm_mod_key(m, key, sizeof key);
    int rc;
    if (m->source == SMM_SRC_THUNDERSTORE) rc = smm_mod_install_thunderstore(url, m->dll_name, p);
    else if (m->source == SMM_SRC_MONKEFORGE) rc = smm_mf_install(m, p);
    else rc = smm_mod_install_dll(url, key, p);
    if (rc != 0) return rc;
    if (m->version[0]) smm_mod_version_set(key, m->version);
    return 0;
}

int smm_mod_files_present(const SmmMod *m)
{
    return smm_mod_is_disabled(m) || smm_mod_is_installed(m, NULL);
}

int smm_mod_is_installed(const SmmMod *m, const SmmInstalledList *inst)
{
    (void)inst;
    if (m->source == SMM_SRC_MONKEFORGE) return smm_mf_is_installed(m);
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return 0;
    char p[SMM_PATH], fn[220];
    if (m->source == SMM_SRC_THUNDERSTORE) {
        if (smm_detect_loader() == SMM_LOADER_MELON) {
            char k[200];
            smm_melon_key(m->dll_name, k, sizeof k);
            smm_fmt(fn, sizeof fn, "%s.dll", k);
            smm_join(p, sizeof p, dir, fn);
            return smm_os_is_file(p);
        }
        smm_join(p, sizeof p, dir, m->dll_name);
        return smm_os_is_dir(p) && smm_os_count_ext(p, ".dll", 1) > 0;
    }
    smm_join(p, sizeof p, dir, m->dll_name);
    return smm_os_is_file(p);
}

int smm_mod_is_disabled(const SmmMod *m)
{
    if (m->source == SMM_SRC_MONKEFORGE) return smm_mf_is_disabled(m);
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) return 0;
    char p[SMM_PATH], fn[240];
    if (m->source == SMM_SRC_THUNDERSTORE) {
        if (smm_detect_loader() == SMM_LOADER_MELON) {
            char k[200];
            smm_melon_key(m->dll_name, k, sizeof k);
            smm_fmt(fn, sizeof fn, "%s.dll.disabled", k);
            smm_join(p, sizeof p, dir, fn);
            return smm_os_is_file(p);
        }
        smm_join(p, sizeof p, dir, m->dll_name);
        return smm_os_is_dir(p) && smm_os_count_ext(p, ".dll.disabled", 1) > 0;
    }
    smm_fmt(fn, sizeof fn, "%s.disabled", m->dll_name);
    smm_join(p, sizeof p, dir, fn);
    return smm_os_is_file(p);
}

void smm_apply_latest_versions(SmmInstalledList *inst, const SmmMods *latest)
{
    int melon = smm_detect_loader() == SMM_LOADER_MELON;
    for (int i = 0; i < inst->n; i++) {
        inst->v[i].latest_version[0] = 0;
        for (int k = 0; k < latest->n; k++) {
            if (!latest->v[k].version[0]) continue;
            char key[200];
            if (melon) smm_melon_key(latest->v[k].dll_name, key, sizeof key);
            else smm_copy(key, sizeof key, latest->v[k].dll_name);
            if (smm_strcasecmp(key, inst->v[i].name) == 0) {
                smm_copy(inst->v[i].latest_version, sizeof inst->v[i].latest_version, latest->v[k].version);
                break;
            }
        }
    }
}

void smm_strip_dep_version(const char *dep, char *out, size_t n)
{
    smm_copy(out, n, dep);
    char *d = strrchr(out, '-');
    if (d) *d = 0;
}

static const SmmMod *find_full(const SmmMods *all, const char *full)
{
    for (int i = 0; i < all->n; i++)
        if (smm_strcasecmp(all->v[i].dll_name, full) == 0) return &all->v[i];
    return NULL;
}

static void deps_walk(const SmmMod *m, const SmmMods *all, SmmStrs *visited, SmmMods *out)
{
    for (int i = 0; i < m->deps.n; i++) {
        const char *dep = m->deps.v[i];
        if (!strrchr(dep, '-')) continue;
        char full[200];
        smm_strip_dep_version(dep, full, sizeof full);
        if (smm_strs_find(visited, full, 1) >= 0) continue;
        smm_strs_add(visited, full);
        if (smm_stristr(full, "BepInExPack") || smm_stristr(full, "MelonLoader")) continue;
        const SmmMod *d = find_full(all, full);
        if (!d || !d->download_url[0]) continue;
        if (!smm_mod_is_installed(d, NULL) && !smm_mod_is_disabled(d)) {
            SmmMod copy = *d;
            memset(&copy.deps, 0, sizeof copy.deps);
            smm_strs_copy(&copy.deps, &d->deps);
            smm_mods_add(out, &copy);
        }
        deps_walk(d, all, visited, out);
    }
}

int smm_missing_deps(const SmmMod *m, const SmmMods *all, const SmmInstalledList *inst, SmmMods *out)
{
    (void)inst;
    SmmStrs visited = {0};
    deps_walk(m, all, &visited, out);
    smm_strs_free(&visited);
    return out->n;
}

int smm_dependents_installed(const SmmMod *m, const SmmMods *all, const SmmInstalledList *inst, SmmStrs *out)
{
    (void)inst;
    for (int i = 0; i < all->n; i++) {
        const SmmMod *o = &all->v[i];
        if (smm_strcasecmp(o->dll_name, m->dll_name) == 0) continue;
        int dep = 0;
        for (int k = 0; k < o->deps.n && !dep; k++) {
            char full[200];
            smm_strip_dep_version(o->deps.v[k], full, sizeof full);
            dep = smm_strcasecmp(full, m->dll_name) == 0;
        }
        if (dep && smm_mod_is_installed(o, NULL)) smm_strs_add(out, o->name);
    }
    return out->n;
}

int smm_find_conflicts(const SmmInstalledList *inst, SmmStrs *out)
{
    SmmMap owners = {0};
    SmmStrs order = {0};
    for (int i = 0; i < inst->n; i++) {
        const SmmInstalled *m = &inst->v[i];
        SmmStrs files = {0};
        if (m->is_folder) smm_os_walk_files(m->path, &files);
        else smm_strs_add(&files, m->path);
        for (int k = 0; k < files.n; k++) {
            const char *fn = smm_basename(files.v[k]);
            if (!smm_ends_with_nocase(fn, ".dll") && m->is_folder) continue;
            char lower[SMM_PATH];
            smm_copy(lower, sizeof lower, fn);
            for (char *c = lower; *c; c++)
                if (*c >= 'A' && *c <= 'Z') *c += 32;
            const char *cur = smm_map_get(&owners, lower);
            if (!cur) {
                smm_map_set(&owners, lower, m->name);
                smm_strs_add(&order, fn);
                continue;
            }
            char both[2048];
            smm_copy(both, sizeof both, cur);
            char needle[200];
            smm_fmt(needle, sizeof needle, "\n%s\n", m->name);
            char hay[2100];
            smm_fmt(hay, sizeof hay, "\n%s\n", both);
            if (!strstr(hay, needle)) {
                char merged[2200];
                smm_fmt(merged, sizeof merged, "%s\n%s", both, m->name);
                smm_map_set(&owners, lower, merged);
            }
        }
        smm_strs_free(&files);
    }
    for (int i = 0; i < order.n; i++) {
        char lower[SMM_PATH];
        smm_copy(lower, sizeof lower, order.v[i]);
        for (char *c = lower; *c; c++)
            if (*c >= 'A' && *c <= 'Z') *c += 32;
        const char *o = smm_map_get(&owners, lower);
        if (!o || !strchr(o, '\n')) continue;
        char line[2600], list[2200];
        smm_copy(list, sizeof list, o);
        SmmBuf b = {0};
        char *save = list;
        int first = 1;
        for (char *seg = save; seg;) {
            char *nl = strchr(seg, '\n');
            if (nl) *nl = 0;
            if (!first) smm_buf_append(&b, " and ", 5);
            smm_buf_append(&b, seg, strlen(seg));
            first = 0;
            seg = nl ? nl + 1 : NULL;
        }
        smm_bufs_cstr(&b);
        smm_fmt(line, sizeof line, "%s is present in both %s", order.v[i], b.p);
        smm_buf_free(&b);
        smm_strs_add(out, line);
    }
    smm_map_free(&owners);
    smm_strs_free(&order);
    return out->n;
}

static int mod_rank(const SmmMod *m)
{
    return m->featured ? 2 : m->verified ? 1 : 0;
}

static int cmp_mods(const void *a, const void *b)
{
    const SmmMod *x = a, *y = b;
    int rx = mod_rank(x), ry = mod_rank(y);
    if (rx != ry) return ry - rx;
    if (x->upvotes != y->upvotes) return y->upvotes - x->upvotes;
    return 0;
}

void smm_sort_mods(SmmMods *m)
{
    if (m->n < 2) return;
    SmmMod *tmp = smm_xmalloc((size_t)m->n * sizeof(SmmMod));
    int *idx = smm_xmalloc((size_t)m->n * sizeof(int));
    for (int i = 0; i < m->n; i++) idx[i] = i;
    for (int i = 1; i < m->n; i++) {
        int cur = idx[i], j = i - 1;
        while (j >= 0 && cmp_mods(&m->v[idx[j]], &m->v[cur]) > 0) {
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = cur;
    }
    for (int i = 0; i < m->n; i++) tmp[i] = m->v[idx[i]];
    memcpy(m->v, tmp, (size_t)m->n * sizeof(SmmMod));
    free(tmp);
    free(idx);
}

void smm_filter_mods(const SmmMods *src, const char *tab, SmmMods *out)
{
    for (int i = 0; i < src->n; i++) {
        const SmmMod *m = &src->v[i];
        int keep = !strcmp(tab, "featured") ? m->featured : !strcmp(tab, "verified") ? m->verified : !strcmp(tab, "unverified") ? !m->verified : 1;
        if (!keep) continue;
        SmmMod c = *m;
        memset(&c.deps, 0, sizeof c.deps);
        smm_strs_copy(&c.deps, &m->deps);
        smm_mods_add(out, &c);
    }
    smm_sort_mods(out);
}
