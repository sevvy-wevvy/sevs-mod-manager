#include "../src/core/internal.h"
#include <stdlib.h>
#include <unistd.h>

static const char *g_fix_dir;
static int g_checks, g_fails;

#define CHECK(c) do { g_checks++; if (!(c)) { g_fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(a, b) do { g_checks++; if (strcmp((a), (b)) != 0) { g_fails++; printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } } while (0)

static void test_util(void)
{
    SmmStrs s = {0};
    smm_strs_add(&s, "one");
    smm_strs_add(&s, "Two");
    CHECK(s.n == 2);
    CHECK(smm_strs_find(&s, "two", 1) == 1);
    CHECK(smm_strs_find(&s, "two", 0) < 0);
    CHECK(smm_strs_remove(&s, "one") == 1 && s.n == 1);
    smm_strs_free(&s);
    SmmMap m = {0};
    smm_map_set(&m, "a", "1");
    smm_map_set(&m, "a", "2");
    CHECK(m.n == 1);
    CHECK_STR(smm_map_get(&m, "a"), "2");
    CHECK(smm_map_del(&m, "a") && m.n == 0);
    smm_map_free(&m);
    char out[64];
    smm_join(out, sizeof out, "/a/b", "c.txt");
    CHECK_STR(out, "/a/b/c.txt");
    smm_stem("/x/y/Foo Bar.dll", out, sizeof out);
    CHECK_STR(out, "Foo Bar");
    smm_derive_slug("Gorilla Tag!", out, sizeof out);
    CHECK_STR(out, "gorillatag");
    long long t = 0;
    CHECK(smm_parse_iso("2026-11-15T10:20:30Z", &t) == 0);
    char iso[40];
    smm_iso_time(t, iso, sizeof iso);
    CHECK_STR(iso, "2026-11-15T10:20:30Z");
}

static void test_json(void)
{
    const char *txt = "{\"a\":[1,2,{\"b\":\"x\\u00e9\\n\"}],\"t\":true,\"n\":null,\"f\":-1.5e2,\"s\":\"\\ud83d\\ude00\"}";
    SmmJson *j = smm_json_parse(txt, strlen(txt));
    CHECK(j != NULL);
    const SmmJson *a = smm_json_get(j, "a");
    CHECK(a && a->n == 3);
    CHECK_STR(smm_json_str(smm_json_at(a, 2), "b", ""), "x\xc3\xa9\n");
    CHECK(smm_json_bool(j, "t", 0) == 1);
    CHECK(smm_json_num(j, "f", 0) == -150.0);
    CHECK_STR(smm_json_str(j, "s", ""), "\xf0\x9f\x98\x80");
    smm_json_free(j);
    CHECK(smm_json_parse("{\"a\":", 5) == NULL);
    CHECK(smm_json_parse("[1,2", 4) == NULL);
    SmmBuf b = {0};
    smm_json_escape(&b, "q\"\\\n\x01");
    CHECK_STR(b.p, "\"q\\\"\\\\\\n\\u0001\"");
    smm_buf_free(&b);
}

static void test_zip(const char *work)
{
    char zpath[SMM_PATH], big[SMM_PATH];
    smm_join(zpath, sizeof zpath, work, "t.zip");
    SmmBuf text = {0};
    for (int i = 0; i < 5000; i++) smm_buf_printf(&text, "line %d of some quite repetitive text %d\n", i, i % 17);
    SmmZipWriter w;
    CHECK(smm_zipw_open(&w, zpath) == 0);
    CHECK(smm_zipw_add_mem(&w, "a/b.txt", text.p, text.n) == 0);
    CHECK(smm_zipw_add_mem(&w, "tiny.txt", "hi", 2) == 0);
    CHECK(smm_zipw_add_mem(&w, "empty.txt", "", 0) == 0);
    CHECK(smm_zipw_close(&w) == 0);
    CHECK(smm_os_size(zpath) < (long long)text.n);
    SmmZip z;
    CHECK(smm_zip_open_file(&z, zpath) == 0);
    CHECK(z.n == 3);
    int i = smm_zip_find(&z, "a/b.txt");
    CHECK(i >= 0);
    SmmBuf out = {0};
    CHECK(smm_zip_extract_mem(&z, i, &out) == 0 && out.n == text.n && memcmp(out.p, text.p, text.n) == 0);
    smm_buf_free(&out);
    smm_zip_close(&z);
    char cmd[2200];
    smm_fmt(cmd, sizeof cmd, "unzip -tq '%s' >/dev/null", zpath);
    CHECK(system(cmd) == 0);
    smm_join(big, sizeof big, work, "ext.zip");
    smm_fmt(cmd, sizeof cmd, "cd '%s' && rm -rf zsrc ext.zip && mkdir -p zsrc/d1/d2 && cp %s/../src/core/zip.c zsrc/d1/ && cp %s/../goome.h zsrc/d1/d2/g.h && echo hello > zsrc/h.txt && zip -qr ext.zip zsrc", work, g_fix_dir, g_fix_dir);
    CHECK(system(cmd) == 0);
    CHECK(smm_zip_open_file(&z, big) == 0);
    char ex[SMM_PATH];
    smm_join(ex, sizeof ex, work, "zex");
    smm_os_remove_tree(ex);
    CHECK(smm_zip_extract_all(&z, ex) == 0);
    smm_zip_close(&z);
    char a[SMM_PATH], b[SMM_PATH];
    smm_join(a, sizeof a, ex, "zsrc/d1/d2/g.h");
    CHECK(smm_os_size(a) > 100000);
    smm_fmt(cmd, sizeof cmd, "cmp '%s/zsrc/d1/d2/g.h' '%s/../goome.h' && cmp '%s/zsrc/d1/zip.c' '%s/../src/core/zip.c'", ex, g_fix_dir, ex, g_fix_dir);
    CHECK(system(cmd) == 0);
    (void)b;
    smm_buf_free(&text);
}

static void test_state(void)
{
    smm_state_load();
    CHECK(smm_settings.game_name[0] != 0);
    smm_map_set(&smm_settings.loader_overrides, "X", "1");
    smm_strs_add(&smm_settings.hidden_presets, "REPO");
    smm_copy(smm_settings.custom_accent, sizeof smm_settings.custom_accent, "#AABBCC");
    smm_settings.theme = SMM_THEME_CUSTOM;
    CHECK(smm_state_save() == 0);
    smm_state_load();
    CHECK_STR(smm_settings.custom_accent, "#AABBCC");
    CHECK(smm_settings.theme == SMM_THEME_CUSTOM);
    CHECK(smm_strs_find(&smm_settings.hidden_presets, "REPO", 0) >= 0);
    CHECK_STR(smm_map_get(&smm_settings.loader_overrides, "X"), "1");
    CHECK_STR(smm_map_get(&smm_settings.thunderstore_communities, "Gorilla Tag"), "gorilla-tag");
    smm_settings.theme = SMM_THEME_BLACK;
    smm_strs_remove(&smm_settings.hidden_presets, "REPO");
    smm_map_del(&smm_settings.loader_overrides, "X");
    smm_state_save();
    char p[SMM_PATH];
    smm_state_path(p, sizeof p);
    SmmBuf b = {0};
    smm_file_read(p, &b);
    CHECK(strstr(b.p, "\"GamePath\"") && strstr(b.p, "\"ThunderstoreCommunities\"") && strstr(b.p, "\"HasSeenTutorial\""));
    smm_buf_free(&b);
}

static void test_paths(void)
{
    char d[SMM_PATH];
    CHECK(smm_game_dir(d, sizeof d));
    CHECK(strstr(d, "Gorilla Tag") != NULL);
    CHECK(smm_mods_install_dir(d, sizeof d) && smm_ends_with(d, "plugins"));
    CHECK(smm_detect_loader() == SMM_LOADER_BEPINEX);
    CHECK(smm_exe_is_x64(smm_settings.game_path) == 1);
    CHECK(smm_has_sb_folder());
    char slug[64];
    smm_current_slug(slug, sizeof slug);
    CHECK_STR(slug, "gorillatag");
    CHECK(smm_loader_installed(SMM_LOADER_BEPINEX, "/nonexistent") == 0);
    smm_dc_load_settings();
    CHECK_STR(smm_dc_get_string("Theme1", ""), "#FF5E7E");
    CHECK(smm_dc_get_bool("ShowTutorial", 1) == 0);
    CHECK(smm_dc_get_float("VolumeBoost", 0) > 0.34);
    smm_dc_set_bool("NewFlag", 1);
    smm_dc_set_string("Name", "a\"b");
    smm_dc_load_settings();
    CHECK(smm_dc_get_bool("NewFlag", 0) == 1);
    CHECK_STR(smm_dc_get_string("Name", ""), "a\"b");
    CHECK(smm_dc_get_bool("ShowTutorial", 1) == 0);
}

static void test_installed(void)
{
    SmmInstalledList l = {0};
    smm_installed_scan(&l);
    CHECK(l.n == 13);
    int folders = 0, disabled = 0;
    for (int i = 0; i < l.n; i++) {
        folders += l.v[i].is_folder;
        disabled += !l.v[i].enabled;
    }
    CHECK(folders == 9);
    CHECK(disabled == 3);
    for (int i = 1; i < l.n; i++) CHECK(smm_strcasecmp(l.v[i - 1].name, l.v[i].name) <= 0);
    SmmMods ts = {0};
    CHECK(smm_ts_list_packages("gorilla-tag", &ts) == 40);
    smm_apply_latest_versions(&l, &ts);
    int updates = 0;
    for (int i = 0; i < l.n; i++) updates += smm_installed_update_available(&l.v[i]);
    CHECK(updates == 3);
    SmmStrs conf = {0};
    CHECK(smm_find_conflicts(&l, &conf) == 0);
    smm_strs_free(&conf);
    smm_mods_free(&ts);
    smm_installed_free(&l);
}

static void test_thunderstore(void)
{
    SmmMods all = {0};
    CHECK(smm_ts_list_packages("gorilla-tag", &all) == 40);
    int feat = 0, unver = 0;
    for (int i = 0; i < all.n; i++) {
        feat += all.v[i].featured;
        unver += !all.v[i].verified;
        CHECK(all.v[i].download_url[0] != 0 && all.v[i].version[0] != 0);
        CHECK(all.v[i].source == SMM_SRC_THUNDERSTORE);
    }
    CHECK(feat == 6 && unver == 5);
    SmmMods f = {0};
    smm_filter_mods(&all, "featured", &f);
    CHECK(f.n == feat);
    SmmMods u = {0};
    smm_filter_mods(&all, "unverified", &u);
    CHECK(u.n == unver);
    SmmMods s = {0};
    smm_filter_mods(&all, "all", &s);
    CHECK(s.n == 40 && s.v[0].featured);
    for (int i = 1; i < s.n; i++) {
        int ra = s.v[i - 1].featured ? 2 : s.v[i - 1].verified ? 1 : 0, rb = s.v[i].featured ? 2 : s.v[i].verified ? 1 : 0;
        CHECK(ra >= rb);
        if (ra == rb) CHECK(s.v[i - 1].upvotes >= s.v[i].upvotes);
    }
    const SmmMod *ci = NULL;
    for (int i = 0; i < all.n; i++)
        if (!strcmp(all.v[i].name, "ComputerInterface")) ci = &all.v[i];
    CHECK(ci && ci->deps.n == 2);
    SmmMods miss = {0};
    SmmMod hats;
    memset(&hats, 0, sizeof hats);
    for (int i = 0; i < all.n; i++)
        if (!strcmp(all.v[i].name, "GorillaHats")) hats = all.v[i];
    CHECK(smm_missing_deps(&hats, &all, NULL, &miss) == 0);
    SmmMod lobby;
    memset(&lobby, 0, sizeof lobby);
    for (int i = 0; i < all.n; i++)
        if (!strcmp(all.v[i].name, "ModdedLobbyBrowser")) lobby = all.v[i];
    CHECK(smm_mod_uninstall("Auros-PhotonUtility") == 0);
    CHECK(smm_missing_deps(&lobby, &all, NULL, &miss) == 1);
    CHECK_STR(miss.v[0].dll_name, "Auros-PhotonUtility");
    smm_mods_free(&miss);
    SmmMod cyc;
    memset(&cyc, 0, sizeof cyc);
    for (int i = 0; i < all.n; i++)
        if (!strcmp(all.v[i].name, "MonkeBackgroundTweaks")) cyc = all.v[i];
    CHECK(smm_missing_deps(&cyc, &all, NULL, &miss) == 0);
    smm_mods_free(&miss);
    SmmStrs dependents = {0};
    for (int i = 0; i < all.n; i++)
        if (!strcmp(all.v[i].name, "Utilla")) smm_dependents_installed(&all.v[i], &all, NULL, &dependents);
    CHECK(dependents.n >= 2);
    smm_strs_free(&dependents);
    SmmCommunities c = {0};
    CHECK(smm_ts_list_communities(&c) == 20);
    char id[64];
    CHECK(smm_ts_guess_community("Gorilla Tag", id, sizeof id) && !strcmp(id, "gorilla-tag"));
    CHECK(!smm_ts_guess_community("Nonexistent Game", id, sizeof id));
    smm_communities_free(&c);
    smm_mods_free(&f);
    smm_mods_free(&u);
    smm_mods_free(&s);
    smm_mods_free(&all);
}

static void test_sb(void)
{
    SmmMods m = {0};
    CHECK(smm_sb_list_mods("all", 1, 1, &m) == 20);
    CHECK(m.v[0].source == SMM_SRC_API && m.v[0].dll_name[0]);
    SmmMods f = {0};
    CHECK(smm_sb_list_mods("featured", 1, 1, &f) == 4);
    SmmMods p2 = {0};
    CHECK(smm_sb_list_mods("all", 2, 1, &p2) == 0);
    CHECK(smm_sb_register_game("gorillatag") == 1);
    char code[64], tok[64], user[64];
    CHECK(smm_sb_login_code(code, sizeof code));
    CHECK(!smm_sb_poll_login(code, tok, sizeof tok));
    CHECK(smm_sb_validate_token("x", user, sizeof user) && !strcmp(user, "sevvy"));
    char url[600];
    CHECK(smm_mod_download_url(&m.v[0], url, sizeof url));
    CHECK(strstr(url, "/releases/latest/download/") != NULL);
    CHECK(smm_http_url_exists("https://example.com/x.dll"));
    CHECK(smm_update_latest_version() == 2);
    smm_mods_free(&m);
    smm_mods_free(&f);
    smm_mods_free(&p2);
}

static void test_install(void)
{
    SmmMods all = {0};
    smm_ts_list_packages("gorilla-tag", &all);
    const SmmMod *hats = NULL, *lights = NULL;
    for (int i = 0; i < all.n; i++) {
        if (!strcmp(all.v[i].name, "GorillaHats")) hats = &all.v[i];
        if (!strcmp(all.v[i].name, "GorillaLights")) lights = &all.v[i];
    }
    CHECK(hats && lights);
    CHECK(!smm_mod_is_installed(hats, NULL));
    CHECK(smm_mod_install(hats, NULL) == 0);
    CHECK(smm_mod_is_installed(hats, NULL));
    char dir[SMM_PATH], p[SMM_PATH], v[64];
    smm_mods_install_dir(dir, sizeof dir);
    smm_join3(p, sizeof p, dir, "Quinn-GorillaHats", "GorillaHats.dll");
    CHECK(smm_os_is_file(p));
    smm_mod_version_get("Quinn-GorillaHats", v, sizeof v);
    CHECK_STR(v, "3.0.0");
    CHECK(smm_mod_disable("Quinn-GorillaHats") == 0);
    CHECK(smm_mod_is_disabled(hats) && !smm_mod_is_installed(hats, NULL));
    CHECK(smm_mod_enable("Quinn-GorillaHats") == 0);
    CHECK(smm_mod_is_installed(hats, NULL) && !smm_mod_is_disabled(hats));
    CHECK(smm_mod_uninstall("Quinn-GorillaHats") == 0);
    CHECK(!smm_mod_is_installed(hats, NULL));
    char trash[SMM_PATH];
    smm_join(trash, sizeof trash, dir, "Quinn-GorillaHats.delete");
    CHECK(smm_os_is_dir(trash));
    smm_mod_version_get("Quinn-GorillaHats", v, sizeof v);
    CHECK_STR(v, "");
    SmmMods sb = {0};
    smm_sb_list_mods("all", 1, 1, &sb);
    CHECK(smm_mod_install(&sb.v[0], NULL) == 0);
    SmmInstalledList l = {0};
    smm_installed_scan(&l);
    int found = 0;
    for (int i = 0; i < l.n; i++)
        if (!strcmp(l.v[i].name, "BepInExPack_GorillaTag")) found = 1;
    CHECK(l.n == 13);
    (void)found;
    CHECK(smm_mod_uninstall(l.v[0].name) == 0);
    smm_installed_free(&l);
    smm_mods_free(&sb);
    smm_mods_free(&all);
}

static void test_install_local(const char *work)
{
    char f[SMM_PATH], dir[SMM_PATH], d[SMM_PATH];
    smm_join(f, sizeof f, work, "Local Mod.dll");
    smm_file_write(f, "MZ", 2);
    CHECK(smm_mod_install_local(f) == 0);
    smm_mods_install_dir(dir, sizeof dir);
    smm_join(d, sizeof d, dir, "Local Mod.dll");
    CHECK(smm_os_is_file(d));
    CHECK(smm_mod_disable("Local Mod") == 0);
    CHECK(smm_mod_uninstall("Local Mod") == 0);
}

static void test_loadouts(void)
{
    SmmLoadouts l = {0};
    CHECK(smm_loadouts_load(&l) == 3);
    CHECK_STR(l.v[0].name, "Daily driver");
    CHECK(l.v[1].missing.n == 1);
    char dn[64];
    smm_loadout_display_name(&l.v[2], dn, sizeof dn);
    CHECK_STR(dn, "Loadout 3");
    CHECK(smm_loadout_capture(&l) == 0);
    CHECK(l.n == 4 && l.v[3].number == 4);
    CHECK(l.v[3].enabled.n == 1 || l.v[3].enabled.n == 2);
    char dir[SMM_PATH], f[SMM_PATH];
    smm_mods_install_dir(dir, sizeof dir);
    smm_join(f, sizeof f, dir, "WristClock.dll");
    CHECK(smm_os_is_file(f));
    CHECK(smm_mod_disable("WristClock") == 0);
    CHECK(smm_loadout_apply(&l, 4) == 0);
    CHECK(smm_os_is_file(f));
    CHECK(smm_loadout_apply(&l, 3) == 0);
    char off[SMM_PATH];
    smm_fmt(off, sizeof off, "%s.disabled", f);
    CHECK(smm_os_is_file(off) && !smm_os_is_file(f));
    CHECK(smm_loadout_apply(&l, 2) == 0);
    CHECK(smm_os_is_file(f));
    CHECK(l.v[1].missing.n == 1);
    CHECK(smm_loadout_rename(&l, 4, "  Saved  ") == 0);
    CHECK_STR(l.v[3].name, "Saved");
    CHECK(smm_loadout_delete(&l, 4) == 0 && l.n == 3);
    smm_loadouts_free(&l);
    SmmLoadouts again = {0};
    CHECK(smm_loadouts_load(&again) == 3);
    smm_loadouts_free(&again);
    SmmLoadouts full = {0};
    for (int i = 0; i < SMM_MAX_LOADOUTS; i++) {
        SmmLoadout *x;
        full.v = smm_xrealloc(full.v, (size_t)(full.n + 1) * sizeof(SmmLoadout));
        x = &full.v[full.n++];
        memset(x, 0, sizeof *x);
        x->number = i + 1;
    }
    CHECK(smm_loadout_capture(&full) != 0);
    smm_loadouts_free(&full);
}

static void test_packs(const char *work)
{
    SmmPacks p = {0};
    CHECK(smm_packs_list(&p) == 3);
    char dn[128];
    smm_pack_display_name(&p.v[0], dn, sizeof dn);
    CHECK_STR(dn, "Cosmetics only");
    CHECK(p.v[0].manifest.mods.n == 2);
    CHECK(p.v[0].manifest.loader_kind == 0);
    CHECK_STR(p.v[0].manifest.game_slug, "gorillatag");
    SmmStrs ent = {0};
    CHECK(smm_pack_entries(p.v[0].path, &ent) == 0 && ent.n == 4);
    smm_strs_free(&ent);
    CHECK(!smm_setup_matches_saved_pack());
    char packpath[SMM_PATH];
    smm_copy(packpath, sizeof packpath, p.v[0].path);
    smm_packs_free(&p);
    CHECK(smm_pack_apply(packpath, NULL) == 0);
    SmmInstalledList l = {0};
    smm_installed_scan(&l);
    CHECK(l.n == 2);
    CHECK(smm_setup_matches_saved_pack());
    smm_installed_free(&l);
    char dir[SMM_PATH], f[SMM_PATH];
    smm_mods_install_dir(dir, sizeof dir);
    smm_join3(f, sizeof f, dir, "legoandmars-Utilla", "Utilla.dll");
    CHECK(smm_os_is_file(f));
    char game[SMM_PATH], wh[SMM_PATH];
    smm_game_dir(game, sizeof game);
    smm_join(wh, sizeof wh, game, "winhttp.dll");
    CHECK(smm_os_is_file(wh));
    CHECK(smm_loader_mods_enabled(SMM_LOADER_BEPINEX, game));
    CHECK(smm_loader_toggle_mods(SMM_LOADER_BEPINEX, game) == 0 && !smm_loader_mods_enabled(SMM_LOADER_BEPINEX, game));
    CHECK(smm_loader_toggle_mods(SMM_LOADER_BEPINEX, game) == 0 && smm_loader_mods_enabled(SMM_LOADER_BEPINEX, game));
    char saved[SMM_PATH];
    CHECK(smm_pack_save("Test: pack/1", "me", NULL, saved, sizeof saved, NULL) == 0);
    CHECK(strstr(smm_basename(saved), "Test_ pack_1.smmpack") != NULL);
    SmmManifest man;
    CHECK(smm_pack_read_manifest(saved, &man) == 0);
    CHECK_STR(man.name, "Test: pack/1");
    CHECK(man.mods.n == 2);
    smm_manifest_free(&man);
    char cmd[2200];
    smm_fmt(cmd, sizeof cmd, "unzip -tq '%s' >/dev/null", saved);
    CHECK(system(cmd) == 0);
    SmmStrs only = {0};
    smm_strs_add(&only, f);
    char sel[SMM_PATH];
    CHECK(smm_pack_save("Selective", "me", &only, sel, sizeof sel, NULL) == 0);
    SmmStrs ent2 = {0};
    smm_pack_entries(sel, &ent2);
    CHECK(ent2.n == 1 && strstr(ent2.v[0], "BepInEx/plugins/legoandmars-Utilla/Utilla.dll"));
    smm_strs_free(&ent2);
    smm_strs_free(&only);
    char ex[SMM_PATH], imp[SMM_PATH];
    smm_join(ex, sizeof ex, work, "exported.smmpack");
    CHECK(smm_pack_export(sel, ex) == 0);
    CHECK(smm_pack_import(ex, imp, sizeof imp) == 0);
    CHECK(smm_pack_import(ex, imp, sizeof imp) == 0);
    SmmPacks p2 = {0};
    CHECK(smm_packs_list(&p2) == 3 + 1 + 1 + 2);
    smm_packs_free(&p2);
    CHECK(smm_pack_delete(saved) == 0);
    CHECK(smm_pack_blank_setup(NULL) == 0);
    SmmInstalledList l2 = {0};
    smm_installed_scan(&l2);
    CHECK(l2.n == 0);
    smm_installed_free(&l2);
    SmmStrs roots = {0};
    smm_pack_default_roots(&roots);
    CHECK(roots.n == 2);
    smm_strs_free(&roots);
}

static void test_config(void)
{
    SmmConfigRefs r = {0};
    CHECK(smm_config_list(&r) == 4);
    CHECK_STR(r.v[0].display_name, "ComputerInterface");
    SmmConfigFile f;
    CHECK(smm_config_parse(r.v[0].path, &f) == 0);
    CHECK(f.header.n == 2 && f.n_sections == 2);
    SmmConfigEntry *e = &f.sections[0].v[1];
    CHECK_STR(e->key, "Scale");
    CHECK_STR(e->setting_type, "Single");
    CHECK(e->has_range && e->range_min == 0.5 && e->range_max == 2.0);
    CHECK(e->has_default && !strcmp(e->default_value, "1"));
    CHECK(f.sections[0].v[0].acceptable.n == 2);
    CHECK(f.sections[0].v[0].description.n == 1);
    smm_copy(e->value, sizeof e->value, "1.75");
    CHECK(smm_config_save(&f) == 0);
    SmmConfigFile g;
    CHECK(smm_config_parse(r.v[0].path, &g) == 0);
    CHECK_STR(g.sections[0].v[1].value, "1.75");
    CHECK(g.sections[1].n == 2 && g.sections[1].v[1].range_max == 1000.0);
    CHECK(g.sections[0].v[0].acceptable.n == 2);
    smm_config_file_free(&f);
    smm_config_file_free(&g);
    smm_config_refs_free(&r);
}

static void test_melon(const char *work)
{
    char game[SMM_PATH], ud[SMM_PATH], cfg[SMM_PATH];
    smm_game_dir(game, sizeof game);
    smm_map_set(&smm_settings.loader_overrides, smm_settings.game_name, "1");
    CHECK(smm_detect_loader() == SMM_LOADER_MELON);
    smm_join(ud, sizeof ud, game, "UserData");
    smm_os_mkdir_p(ud);
    smm_join(cfg, sizeof cfg, ud, "MelonPreferences.cfg");
    const char *t = "[Alpha]\n# Volume of things\nVolume = 3\nName = \"hi \\\"there\\\"\"\nOn = true\nF = 1.5\n\n[Beta]\nList = [1, 2]\n";
    smm_file_write(cfg, t, strlen(t));
    SmmConfigRefs r = {0};
    CHECK(smm_config_list(&r) == 2);
    CHECK_STR(r.v[0].path, "melon:Alpha");
    SmmConfigFile f;
    CHECK(smm_config_parse(r.v[0].path, &f) == 0);
    CHECK(f.n_sections == 1 && f.sections[0].n == 4);
    CHECK_STR(f.sections[0].v[0].setting_type, "Int32");
    CHECK_STR(f.sections[0].v[1].value, "hi \"there\"");
    CHECK_STR(f.sections[0].v[2].setting_type, "Boolean");
    CHECK_STR(f.sections[0].v[3].setting_type, "Single");
    CHECK(f.sections[0].v[0].description.n == 1);
    smm_copy(f.sections[0].v[0].value, 512, "9");
    CHECK(smm_config_save(&f) == 0);
    SmmBuf b = {0};
    smm_file_read(cfg, &b);
    CHECK(strstr(b.p, "Volume = 9") && strstr(b.p, "[Beta]") && strstr(b.p, "Name = \"hi \\\"there\\\"\""));
    smm_buf_free(&b);
    smm_config_file_free(&f);
    smm_config_refs_free(&r);
    char lp[SMM_PATH];
    CHECK(smm_log_path(lp, sizeof lp) && strstr(lp, "Latest.log"));
    char dmod[SMM_PATH];
    smm_mods_install_dir(dmod, sizeof dmod);
    CHECK(smm_ends_with(dmod, "Mods"));
    SmmPackRoot roots[8];
    CHECK(smm_pack_roots(roots, 8) == 4);
    smm_map_del(&smm_settings.loader_overrides, smm_settings.game_name);
    CHECK(smm_detect_loader() == SMM_LOADER_BEPINEX);
    smm_os_remove_tree(ud);
    (void)work;
}

static void test_logs(const char *work)
{
    char lp[SMM_PATH];
    CHECK(smm_log_path(lp, sizeof lp) && strstr(lp, "LogOutput.log"));
    SmmStrs lines = {0};
    CHECK(smm_log_read_lines(lp, &lines) == 0 && lines.n > 40);
    SmmInstalledList dummy = {0};
    (void)dummy;
    SmmCrashSuspect s;
    memset(&s, 0, sizeof s);
    SmmPacks none = {0};
    (void)none;
    char game[SMM_PATH], pl[SMM_PATH];
    smm_mods_install_dir(pl, sizeof pl);
    smm_game_dir(game, sizeof game);
    char dir[SMM_PATH];
    smm_join(dir, sizeof dir, pl, "Fchb1239-GorillaFriends");
    smm_os_mkdir_p(dir);
    smm_join(dir, sizeof dir, dir, "GorillaFriends.dll");
    smm_file_write(dir, "MZ", 2);
    CHECK(smm_crash_analyze(&lines, &s) == 1);
    CHECK(strstr(s.mod_name, "GorillaFriends") != NULL);
    CHECK(smm_log_line_level("[Error  :   Unity] NullReferenceException") == 2);
    CHECK(smm_log_line_level("[Warning:x] something") == 1);
    CHECK(smm_log_line_level("[Info   :x] fine") == 0);
    long long off = 0;
    SmmStrs got = {0};
    smm_log_read_from(lp, &off, &got);
    CHECK(got.n == lines.n && off > 0);
    smm_strs_free(&got);
    FILE *f = fopen(lp, "ab");
    fputs("[Info   :x] more\n[Info   :x] partial", f);
    fclose(f);
    smm_log_read_from(lp, &off, &got);
    CHECK(got.n == 1 && !strcmp(got.v[0], "[Info   :x] more"));
    smm_strs_free(&got);
    SmmLogSessions hist = {0};
    CHECK(smm_log_history_list(&hist) == 2);
    CHECK(hist.v[0].timestamp > hist.v[1].timestamp);
    CHECK(strstr(hist.v[0].display_name, "Nov 14, 2026") != NULL);
    smm_log_sessions_free(&hist);
    for (int i = 0; i < 12; i++) {
        smm_log_history_archive(1763000000 + i * 1000, &lines);
        char p[SMM_PATH];
        smm_log_history_dir(p, sizeof p);
        SmmDirList dl = {0};
        smm_os_list(p, &dl);
        if (dl.n > 10) {
            CHECK(dl.n <= 11);
        }
        smm_dirlist_free(&dl);
    }
    SmmLogSessions h2 = {0};
    CHECK(smm_log_history_list(&h2) <= 11);
    smm_log_sessions_free(&h2);
    char zp[SMM_PATH], cmd[2200];
    smm_join(zp, sizeof zp, work, "bug.zip");
    CHECK(smm_bug_report_export(zp, &lines) == 0);
    smm_fmt(cmd, sizeof cmd, "unzip -tq '%s' >/dev/null && unzip -p '%s' installed-mods.txt | grep -q GorillaFriends && unzip -p '%s' system-info.txt | grep -q 'Mod loader: BepInEx'", zp, zp, zp);
    CHECK(system(cmd) == 0);
    smm_strs_free(&lines);
    smm_os_remove_tree(dir);
}

static void test_steam(void)
{
    SmmSteamGames g = {0};
    CHECK(smm_steam_list_installed(&g) == 12);
    CHECK_STR(g.v[0].name, "BONELAB");
    smm_steam_games_free(&g);
    char p[SMM_PATH];
    CHECK(smm_steam_find_game_path(1229490, "ULTRAKILL.exe", p, sizeof p));
    CHECK(smm_ends_with(p, "ULTRAKILL.exe"));
    CHECK(!smm_steam_find_game_path(1229490, "Nope.exe", p, sizeof p));
    CHECK(!smm_steam_find_game_path(999, "x.exe", p, sizeof p));
    SmmStrs c = {0};
    CHECK(smm_steam_exe_candidates(1966720, &c) == 1);
    smm_strs_free(&c);
    smm_copy(smm_settings.game_name, sizeof smm_settings.game_name, "ULTRAKILL");
    CHECK(smm_autodetect_game(p, sizeof p));
    smm_copy(smm_settings.game_name, sizeof smm_settings.game_name, "Gorilla Tag");
    CHECK(smm_preset_count() == 6 && smm_preset_find("REPO")->steam_app_id == 3241660);
    CHECK(smm_current_preset()->steam_app_id == 1533390);
}

static void test_theme(void)
{
    SmmTheme t;
    smm_theme_apply(SMM_THEME_BLACK, NULL, NULL, NULL, &t);
    CHECK(t.background.r == 20 && t.accent.r == 124);
    SmmRgb h = smm_rgb_highlight(&t);
    CHECK(h.r == 84);
    smm_theme_apply(SMM_THEME_WHITE, NULL, NULL, NULL, &t);
    h = smm_rgb_highlight(&t);
    CHECK(h.r == 216);
    smm_theme_apply(SMM_THEME_CUSTOM, "#102030", "#FF8800", NULL, &t);
    CHECK(t.background.g == 0x20 && t.surface.r == 0x10 + 10 && t.accent.r == 255 && t.border.b == 0x30 + 35);
    smm_theme_apply(SMM_THEME_SYNCED_SB, NULL, NULL, "#FF5E7E", &t);
    CHECK(t.accent.g == 0x5E && t.background.r == 20);
    smm_theme_apply(SMM_THEME_R2MODMAN, NULL, NULL, NULL, &t);
    CHECK(t.accent.b == 242);
    char hex[8];
    smm_rgb_to_hex(t.accent, hex);
    CHECK_STR(hex, "#5865F2");
    SmmRgb c;
    CHECK(!smm_rgb_parse_hex("12345", &c) && !smm_rgb_parse_hex("GGGGGG", &c) && smm_rgb_parse_hex("0a0B0c", &c) && c.g == 11);
    SmmTheme cur;
    smm_theme_current(&cur);
    CHECK(cur.accent.b == 242);
}

static volatile int g_done_rc = -99;

static void on_done(int rc, void *arg)
{
    (void)arg;
    g_done_rc = rc;
}

static int slow_ok(SmmProgress *p, void *arg)
{
    (void)arg;
    for (int i = 0; i <= 4; i++) {
        smm_progress(p, i * 25, "step %d", i);
        smm_sleep_ms(20);
    }
    return 0;
}

static int slow_fail(SmmProgress *p, void *arg)
{
    (void)arg;
    return smm_op_fail(p, "it broke");
}

static void wait_idle(void)
{
    for (int i = 0; i < 500 && smm_queue_busy(); i++) smm_sleep_ms(10);
}

static void test_queue(void)
{
    unsigned c0 = smm_queue_changed();
    int a = smm_queue_run("slow", slow_ok, NULL, on_done);
    int b = smm_queue_run("fail", slow_fail, NULL, NULL);
    CHECK(a > 0 && b > a);
    wait_idle();
    CHECK(smm_queue_changed() != c0);
    SmmOp ops[8];
    int n = smm_queue_snapshot(ops, 8);
    CHECK(n >= 2);
    CHECK(ops[0].id == b && ops[0].status == SMM_OP_FAILED && !strcmp(ops[0].error, "it broke"));
    CHECK(ops[1].id == a && ops[1].status == SMM_OP_DONE && ops[1].percent == 100);
    CHECK(g_done_rc == 0);
    SmmMods all = {0};
    smm_ts_list_packages("gorilla-tag", &all);
    const SmmMod *m = NULL;
    for (int i = 0; i < all.n; i++)
        if (!strcmp(all.v[i].name, "FPSCounter")) m = &all.v[i];
    CHECK(smm_queue_install_mod(m) > 0);
    CHECK(smm_queue_install_loader(SMM_LOADER_BEPINEX) > 0);
    CHECK(smm_queue_blank_setup() > 0);
    wait_idle();
    n = smm_queue_snapshot(ops, 8);
    CHECK(n >= 5);
    for (int i = 0; i < 3; i++) CHECK(ops[i].status == SMM_OP_DONE);
    SmmInstalledList inst = {0};
    smm_installed_scan(&inst);
    CHECK(inst.n == 0);
    smm_installed_free(&inst);
    smm_mods_free(&all);
}

static void test_game(void)
{
    CHECK(!smm_game_running());
    CHECK(smm_game_launch() == 0 && smm_game_running());
    CHECK(smm_game_stop() == 0 && !smm_game_running());
    char hint[200];
    smm_launch_option_hint(SMM_LOADER_BEPINEX, hint, sizeof hint);
    CHECK_STR(hint, "WINEDLLOVERRIDES=\"winhttp=n,b\" %command%");
    CHECK_STR(smm_loader_dll_override(SMM_LOADER_MELON), "version=n,b");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *work = argc > 1 ? argv[1] : "test-out";
    char demo[SMM_PATH];
    smm_join(demo, sizeof demo, work, "demo");
    smm_os_setenv("SMM_DEMO_DIR", demo);
    smm_os_mkdir_p(work);
    g_fix_dir = argc > 2 ? argv[2] : "fixtures";
    test_util();
    test_json();
    test_zip(work);
    if (smm_demo_init(argc > 2 ? argv[2] : "fixtures") != 0) {
        printf("demo init failed: %s\n", smm_error());
        return 2;
    }
    CHECK(smm_demo_enabled());
    test_state();
    test_paths();
    test_installed();
    test_thunderstore();
    test_sb();
    test_install();
    test_install_local(work);
    test_loadouts();
    test_config();
    test_melon(work);
    test_logs(work);
    test_steam();
    test_theme();
    test_game();
    test_packs(work);
    test_queue();
    printf("%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
