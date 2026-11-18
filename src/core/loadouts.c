#include "internal.h"
#include <stdlib.h>

static void snapshot_dir(int number, char *out, size_t n)
{
    char sb[SMM_PATH], a[SMM_PATH], b[SMM_PATH], num[16];
    if (!smm_sb_dir(sb, sizeof sb)) {
        out[0] = 0;
        return;
    }
    smm_join(a, sizeof a, sb, "loadouts");
    smm_join(b, sizeof b, a, "config_snapshots");
    smm_fmt(num, sizeof num, "%d", number);
    smm_join(out, n, b, num);
}

static void save_snapshot(int number)
{
    char dir[SMM_PATH];
    snapshot_dir(number, dir, sizeof dir);
    smm_os_remove_tree(dir);
    smm_os_mkdir_p(dir);
    char game[SMM_PATH];
    if (!smm_game_dir(game, sizeof game)) return;
    if (smm_detect_loader() == SMM_LOADER_MELON) {
        char ud[SMM_PATH], src[SMM_PATH], dst[SMM_PATH];
        smm_join(ud, sizeof ud, game, "UserData");
        smm_join(src, sizeof src, ud, "MelonPreferences.cfg");
        smm_join(dst, sizeof dst, dir, "MelonPreferences.cfg");
        if (smm_os_is_file(src)) smm_os_copy(src, dst);
    } else {
        char bep[SMM_PATH], cfg[SMM_PATH];
        if (!smm_bepinex_dir(bep, sizeof bep)) return;
        smm_join(cfg, sizeof cfg, bep, "config");
        if (smm_os_is_dir(cfg)) smm_os_copy_tree(cfg, dir);
    }
}

static void restore_snapshot(int number)
{
    char dir[SMM_PATH];
    snapshot_dir(number, dir, sizeof dir);
    if (!smm_os_is_dir(dir)) return;
    char game[SMM_PATH];
    if (!smm_game_dir(game, sizeof game)) return;
    if (smm_detect_loader() == SMM_LOADER_MELON) {
        char src[SMM_PATH], ud[SMM_PATH], dst[SMM_PATH];
        smm_join(src, sizeof src, dir, "MelonPreferences.cfg");
        if (!smm_os_is_file(src)) return;
        smm_join(ud, sizeof ud, game, "UserData");
        smm_join(dst, sizeof dst, ud, "MelonPreferences.cfg");
        smm_os_copy(src, dst);
    } else {
        char bep[SMM_PATH], cfg[SMM_PATH];
        if (!smm_bepinex_dir(bep, sizeof bep)) return;
        smm_join(cfg, sizeof cfg, bep, "config");
        smm_os_mkdir_p(cfg);
        smm_os_copy_tree(dir, cfg);
    }
}

static SmmLoadout *add_loadout(SmmLoadouts *l)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->v = smm_xrealloc(l->v, (size_t)l->cap * sizeof(SmmLoadout));
    }
    SmmLoadout *x = &l->v[l->n++];
    memset(x, 0, sizeof *x);
    return x;
}

int smm_loadout_capture(SmmLoadouts *l)
{
    char game[SMM_PATH], dir[SMM_PATH];
    if (!smm_game_dir(game, sizeof game)) {
        smm_set_error("No game path set.");
        return -1;
    }
    smm_ensure_sb_folder();
    if (l->n >= SMM_MAX_LOADOUTS) {
        smm_set_error("Loadout cap is %d. Delete one first.", SMM_MAX_LOADOUTS);
        return -1;
    }
    if (!smm_mods_install_dir(dir, sizeof dir)) {
        smm_set_error("No game path set.");
        return -1;
    }
    int next = 1;
    for (int i = 0; i < l->n; i++)
        if (l->v[i].number >= next) next = l->v[i].number + 1;
    SmmLoadout *x = add_loadout(l);
    x->number = next;
    SmmDirList d = {0};
    if (smm_os_list(dir, &d) == 0) {
        for (int i = 0; i < d.n; i++) {
            if (d.v[i].is_dir) continue;
            char stem[SMM_PATH];
            const char *fn = d.v[i].name;
            if (smm_ends_with_nocase(fn, ".dll")) {
                smm_copy(stem, sizeof stem, fn);
                stem[strlen(stem) - 4] = 0;
                smm_strs_add(&x->enabled, stem);
            } else if (smm_ends_with_nocase(fn, ".dll.disabled")) {
                smm_copy(stem, sizeof stem, fn);
                stem[strlen(stem) - 13] = 0;
                smm_strs_add(&x->disabled, stem);
            }
        }
    }
    smm_dirlist_free(&d);
    smm_sort_strs_nocase(&x->enabled);
    smm_sort_strs_nocase(&x->disabled);
    int rc = smm_loadouts_save(l);
    save_snapshot(next);
    return rc;
}

static SmmLoadout *find_loadout(SmmLoadouts *l, int number)
{
    for (int i = 0; i < l->n; i++)
        if (l->v[i].number == number) return &l->v[i];
    return NULL;
}

int smm_loadout_apply(SmmLoadouts *l, int number)
{
    SmmLoadout *x = find_loadout(l, number);
    if (!x) return -1;
    char dir[SMM_PATH];
    if (!smm_mods_install_dir(dir, sizeof dir)) {
        smm_set_error("No game path set.");
        return -1;
    }
    SmmStrs en = {0}, dis = {0};
    for (int i = 0; i < x->enabled.n; i++) {
        const char *name = x->enabled.v[i];
        char fn[300], dll[SMM_PATH], off[SMM_PATH];
        smm_fmt(fn, sizeof fn, "%s.dll", name);
        smm_join(dll, sizeof dll, dir, fn);
        smm_fmt(off, sizeof off, "%s.disabled", dll);
        if (smm_os_is_file(off)) {
            smm_os_remove(dll);
            smm_os_rename(off, dll);
            smm_strs_add(&en, name);
        } else if (smm_os_is_file(dll)) {
            smm_strs_add(&en, name);
        } else {
            if (smm_strs_find(&x->missing, name, 0) < 0) smm_strs_add(&x->missing, name);
        }
    }
    for (int i = 0; i < x->disabled.n; i++) {
        const char *name = x->disabled.v[i];
        char fn[300], dll[SMM_PATH], off[SMM_PATH];
        smm_fmt(fn, sizeof fn, "%s.dll", name);
        smm_join(dll, sizeof dll, dir, fn);
        smm_fmt(off, sizeof off, "%s.disabled", dll);
        if (smm_os_is_file(dll)) {
            smm_os_remove(off);
            smm_os_rename(dll, off);
            smm_strs_add(&dis, name);
        } else if (smm_os_is_file(off)) {
            smm_strs_add(&dis, name);
        } else {
            if (smm_strs_find(&x->missing, name, 0) < 0) smm_strs_add(&x->missing, name);
        }
    }
    smm_strs_free(&x->enabled);
    smm_strs_free(&x->disabled);
    x->enabled = en;
    x->disabled = dis;
    int rc = smm_loadouts_save(l);
    restore_snapshot(number);
    return rc;
}

int smm_loadout_delete(SmmLoadouts *l, int number)
{
    int idx = -1;
    for (int i = 0; i < l->n; i++)
        if (l->v[i].number == number) idx = i;
    if (idx < 0) return -1;
    smm_strs_free(&l->v[idx].enabled);
    smm_strs_free(&l->v[idx].disabled);
    smm_strs_free(&l->v[idx].missing);
    memmove(l->v + idx, l->v + idx + 1, (size_t)(l->n - idx - 1) * sizeof(SmmLoadout));
    l->n--;
    int rc = smm_loadouts_save(l);
    char dir[SMM_PATH];
    snapshot_dir(number, dir, sizeof dir);
    smm_os_remove_tree(dir);
    return rc;
}

int smm_loadout_rename(SmmLoadouts *l, int number, const char *name)
{
    SmmLoadout *x = find_loadout(l, number);
    if (!x) return -1;
    smm_copy(x->name, sizeof x->name, name);
    smm_trim(x->name);
    return smm_loadouts_save(l);
}
