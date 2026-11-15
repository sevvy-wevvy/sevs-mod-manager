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

static void test_monkeforge(const char *work)
{
    char hex[72], f[SMM_PATH];
    smm_join(f, sizeof f, work, "sha.txt");
    smm_file_write(f, "abc", 3);
    CHECK(smm_sha256_file(f, hex) == 0);
    CHECK_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    char cmd[2400], want[80] = "";
    smm_join(f, sizeof f, g_fix_dir, "../goome.h");
    CHECK(smm_sha256_file(f, hex) == 0);
    smm_fmt(cmd, sizeof cmd, "sha256sum '%s' | cut -d' ' -f1", f);
    FILE *pp = popen(cmd, "r");
    if (pp) {
        if (fscanf(pp, "%64s", want) != 1) want[0] = 0;
        pclose(pp);
    }
    CHECK_STR(hex, want);

    CHECK(smm_mf_supported());
    CHECK(!smm_mf_active());
    smm_mf_set_active(1);
    CHECK(smm_mf_active());
    smm_state_save();
    smm_state_load();
    CHECK(smm_mf_active());
    smm_mf_set_active(0);
    CHECK(!smm_mf_active());
    char saved_path[SMM_PATH], saved_name[128];
    smm_copy(saved_path, sizeof saved_path, smm_settings.game_path);
    smm_copy(saved_name, sizeof saved_name, smm_settings.game_name);
    smm_settings.game_path[0] = 0;
    smm_copy(smm_settings.game_name, sizeof smm_settings.game_name, "Some Other Game");
    smm_mf_set_active(1);
    CHECK(!smm_mf_supported() && !smm_mf_active());
    smm_copy(smm_settings.game_path, sizeof smm_settings.game_path, saved_path);
    smm_copy(smm_settings.game_name, sizeof smm_settings.game_name, saved_name);

    char fx[SMM_PATH];
    SmmBuf b = {0};
    smm_join(fx, sizeof fx, g_fix_dir, "monkeforge_mods.json");
    CHECK(smm_file_read(fx, &b) == 0);
    SmmMods bep = {0}, melon = {0};
    long long total = 0;
    CHECK(smm_mf_parse_list(b.p, b.n, SMM_LOADER_BEPINEX, &bep, &total) == 22 && total == 22);
    CHECK(smm_mf_parse_list(b.p, b.n, SMM_LOADER_MELON, &melon, &total) == 2);
    smm_buf_free(&b);
    int feat = 0;
    for (int i = 0; i < bep.n; i++) {
        feat += bep.v[i].featured;
        CHECK(bep.v[i].source == SMM_SRC_MONKEFORGE && bep.v[i].verified && bep.v[i].version[0] && strstr(bep.v[i].download_url, "/api/v1/mods/"));
    }
    CHECK(feat == 5);
    CHECK_STR(bep.v[0].dll_name, "com.tonimacaroni.computerinterface");
    CHECK_STR(bep.v[0].author, "DecalFree");
    CHECK(strstr(bep.v[0].icon_url, "https://monkeforge.org/media/") == bep.v[0].icon_url);
    CHECK_STR(bep.v[0].repo_url, "https://monkeforge.org/view/computerinterface");
    CHECK(smm_mf_parse_list("{\"nope\":1}", 11, 0, &bep, NULL) < 0);
    smm_mods_free(&bep);
    smm_mods_free(&melon);

    smm_join(fx, sizeof fx, g_fix_dir, "monkeforge_detail.json");
    CHECK(smm_file_read(fx, &b) == 0);
    SmmMfFile file;
    CHECK(smm_mf_pick_file(b.p, b.n, SMM_LOADER_BEPINEX, &file) == 1);
    CHECK_STR(file.filename, "ComputerInterface-BepInEx.zip");
    CHECK_STR(file.kind, "zip");
    CHECK_STR(file.version, "2.1.5");
    CHECK_STR(file.sha256, "b0257bd45b9aa8e47eaeba1d76ea442873e0aa1f604bcd177c19a64d879f2897");
    CHECK(smm_mf_pick_file(b.p, b.n, SMM_LOADER_MELON, &file) == 1);
    CHECK_STR(file.filename, "ComputerInterface-MelonLoader.zip");
    CHECK(smm_mf_pick_file("{\"releases\":[]}", 15, 0, &file) == 0);
    smm_buf_free(&b);

    SmmMods all = {0};
    CHECK(smm_mf_list(&all) == 22);
    const SmmMod *zipmod = NULL, *dllmod = NULL;
    for (int i = 0; i < all.n; i++) {
        if (!strcmp(all.v[i].dll_name, "com.tonimacaroni.computerinterface")) zipmod = &all.v[i];
        if (!strcmp(all.v[i].dll_name, "com.kx.shopindex")) dllmod = &all.v[i];
    }
    CHECK(zipmod && dllmod);
    char dir[SMM_PATH], p[SMM_PATH], v[64];
    smm_mods_install_dir(dir, sizeof dir);
    CHECK(!smm_mod_is_installed(zipmod, NULL) && !smm_mod_is_installed(dllmod, NULL));
    CHECK(smm_mod_install(zipmod, NULL) == 0);
    CHECK(smm_mod_install(dllmod, NULL) == 0);
    CHECK(smm_mod_is_installed(zipmod, NULL) && smm_mod_is_installed(dllmod, NULL));
    smm_join3(p, sizeof p, dir, "com.tonimacaroni.computerinterface", "com.tonimacaroni.computerinterface.dll");
    CHECK(smm_os_is_file(p));
    smm_join(p, sizeof p, dir, "com.kx.shopindex.dll");
    CHECK(smm_os_is_file(p));
    smm_mod_version_get("com.kx.shopindex", v, sizeof v);
    CHECK_STR(v, "4.0.0");
    CHECK(smm_mod_disable("com.kx.shopindex") == 0 && smm_mod_is_disabled(dllmod) && !smm_mod_is_installed(dllmod, NULL));
    CHECK(smm_mod_disable("com.tonimacaroni.computerinterface") == 0 && smm_mod_is_disabled(zipmod));
    CHECK(smm_mod_enable("com.kx.shopindex") == 0 && smm_mod_enable("com.tonimacaroni.computerinterface") == 0);
    SmmInstalledList l = {0};
    smm_installed_scan(&l);
    int seen = 0;
    for (int i = 0; i < l.n; i++) seen += !strcmp(l.v[i].name, "com.kx.shopindex") || !strcmp(l.v[i].name, "com.tonimacaroni.computerinterface");
    CHECK(seen == 2);
    smm_apply_latest_versions(&l, &all);
    for (int i = 0; i < l.n; i++)
        if (!strcmp(l.v[i].name, "com.kx.shopindex")) CHECK_STR(l.v[i].latest_version, "4.0.0");
    smm_installed_free(&l);
    CHECK(smm_mod_uninstall("com.kx.shopindex") == 0 && smm_mod_uninstall("com.tonimacaroni.computerinterface") == 0);
    CHECK(!smm_mod_is_installed(zipmod, NULL) && !smm_mod_is_installed(dllmod, NULL));
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

typedef struct {
    int type, id;
    unsigned char *data;
    size_t n;
} PeRes;

static void put16(unsigned char *p, unsigned v)
{
    p[0] = v & 255;
    p[1] = v >> 8 & 255;
}

static void put32(unsigned char *p, unsigned v)
{
    put16(p, v & 0xFFFF);
    put16(p + 2, v >> 16);
}

static void build_pe(int pe64, const PeRes *res, int nres, SmmBuf *out)
{
    unsigned opt = pe64 ? 240 : 224;
    unsigned hdr = 0x40 + 24 + opt + 40;
    unsigned raw_off = (hdr + 0x1FF) & ~0x1FFu;
    int types[8], ntypes = 0;
    for (int i = 0; i < nres; i++) {
        int seen = 0;
        for (int t = 0; t < ntypes; t++) seen |= types[t] == res[i].type;
        if (!seen) types[ntypes++] = res[i].type;
    }
    for (int a = 0; a < ntypes; a++)
        for (int b = a + 1; b < ntypes; b++)
            if (types[b] < types[a]) {
                int t = types[a];
                types[a] = types[b];
                types[b] = t;
            }
    size_t cap = 4096;
    for (int i = 0; i < nres; i++) cap += res[i].n + 128;
    unsigned char *rs = calloc(1, cap);
    unsigned pos = 16 + ntypes * 8;
    put16(rs + 14, ntypes);
    unsigned data_pos = 0;
    unsigned type_dir[8];
    for (int t = 0; t < ntypes; t++) {
        put32(rs + 16 + t * 8, types[t]);
        put32(rs + 16 + t * 8 + 4, 0x80000000u | pos);
        type_dir[t] = pos;
        int cnt = 0;
        for (int i = 0; i < nres; i++) cnt += res[i].type == types[t];
        put16(rs + pos + 14, cnt);
        pos += 16 + cnt * 8;
    }
    unsigned entries_at = pos;
    unsigned lang_dirs = entries_at, leafs = entries_at + nres * 24, blobs = leafs + nres * 16;
    data_pos = blobs;
    int k = 0;
    for (int t = 0; t < ntypes; t++) {
        int slot = 0;
        for (int i = 0; i < nres; i++) {
            if (res[i].type != types[t]) continue;
            unsigned e = type_dir[t] + 16 + slot * 8;
            unsigned ld = lang_dirs + k * 24, leaf = leafs + k * 16;
            put32(rs + e, res[i].id);
            put32(rs + e + 4, 0x80000000u | ld);
            put16(rs + ld + 14, 1);
            put32(rs + ld + 16, 0x409);
            put32(rs + ld + 20, leaf);
            put32(rs + leaf, 0x1000 + data_pos);
            put32(rs + leaf + 4, (unsigned)res[i].n);
            memcpy(rs + data_pos, res[i].data, res[i].n);
            data_pos += (unsigned)res[i].n;
            data_pos = (data_pos + 3) & ~3u;
            slot++;
            k++;
        }
    }
    unsigned rawsize = (data_pos + 0x1FF) & ~0x1FFu;
    size_t total = raw_off + rawsize;
    unsigned char *f = calloc(1, total);
    f[0] = 'M';
    f[1] = 'Z';
    put32(f + 60, 0x40);
    unsigned char *nt = f + 0x40;
    memcpy(nt, "PE\0\0", 4);
    put16(nt + 4, pe64 ? 0x8664 : 0x14c);
    put16(nt + 6, 1);
    put16(nt + 20, opt);
    put16(nt + 24, pe64 ? 0x20b : 0x10b);
    unsigned dd = 24 + (pe64 ? 112 : 96);
    put32(nt + 24 + (pe64 ? 108 : 92), 16);
    put32(nt + dd + 16, 0x1000);
    put32(nt + dd + 20, data_pos);
    unsigned char *sec = nt + 24 + opt;
    memcpy(sec, ".rsrc", 5);
    put32(sec + 8, data_pos);
    put32(sec + 12, 0x1000);
    put32(sec + 16, rawsize);
    put32(sec + 20, raw_off);
    memcpy(f + raw_off, rs, data_pos);
    free(rs);
    smm_buf_append(out, f, total);
    free(f);
}

static unsigned char *make_dib(int w, int h, int bits, int alpha, size_t *n)
{
    unsigned ncol = bits <= 8 ? 1u << bits : 0;
    size_t stride = (((size_t)w * bits + 31) / 32) * 4, mstride = (((size_t)w + 31) / 32) * 4;
    *n = 40 + ncol * 4 + stride * h + mstride * h;
    unsigned char *d = calloc(1, *n);
    put32(d, 40);
    put32(d + 4, w);
    put32(d + 8, h * 2);
    put16(d + 12, 1);
    put16(d + 14, bits);
    unsigned char *pal = d + 40, *px = pal + ncol * 4, *mask = px + stride * h;
    for (unsigned i = 0; i < ncol; i++) {
        pal[i * 4] = (unsigned char)(i * 7);
        pal[i * 4 + 1] = (unsigned char)(255 - i * 3);
        pal[i * 4 + 2] = (unsigned char)(i * 5 + 1);
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned char *row = px + (size_t)y * stride;
            if (bits == 32) {
                row[x * 4] = 10;
                row[x * 4 + 1] = 20;
                row[x * 4 + 2] = 200;
                row[x * 4 + 3] = alpha ? 128 : 0;
            } else if (bits == 24) {
                row[x * 3] = 10;
                row[x * 3 + 1] = 20;
                row[x * 3 + 2] = 200;
            } else if (bits == 8) row[x] = (unsigned char)((x + y) & 3);
            else if (bits == 4) row[x / 2] |= (unsigned char)(((x + y) & 3) << (x % 2 ? 0 : 4));
            else if ((x + y) & 1) row[x / 8] |= (unsigned char)(0x80 >> (x % 8));
            if (x == 0) mask[(size_t)y * mstride] |= 0x80;
        }
    return d;
}

static unsigned char *make_png(int w, int h, size_t *n)
{
    size_t rawn = (size_t)(w * 4 + 1) * h;
    unsigned char *raw = calloc(1, rawn);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned char *p = raw + (size_t)y * (w * 4 + 1) + 1 + x * 4;
            p[0] = 255;
            p[1] = 0;
            p[2] = 0;
            p[3] = 255;
        }
    size_t z = 2 + 5 + rawn + 4;
    *n = 8 + 25 + 12 + z + 12;
    unsigned char *d = calloc(1, *n);
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    memcpy(d, sig, 8);
    unsigned char *c = d + 8;
    c[3] = 13;
    memcpy(c + 4, "IHDR", 4);
    c[10] = (unsigned char)(w >> 8);
    c[11] = (unsigned char)w;
    c[14] = (unsigned char)(h >> 8);
    c[15] = (unsigned char)h;
    c[16] = 8;
    c[17] = 6;
    c += 25;
    c[0] = (unsigned char)(z >> 24);
    c[1] = (unsigned char)(z >> 16);
    c[2] = (unsigned char)(z >> 8);
    c[3] = (unsigned char)z;
    memcpy(c + 4, "IDAT", 4);
    c[8] = 0x78;
    c[9] = 1;
    c[10] = 1;
    put16(c + 11, (unsigned)rawn);
    put16(c + 13, (unsigned)~rawn);
    memcpy(c + 15, raw, rawn);
    c += 12 + z;
    memcpy(c + 4, "IEND", 4);
    free(raw);
    return d;
}

static unsigned char *make_group(const int *sizes, const int *bits, const int *ids, int n, size_t *len)
{
    *len = 6 + n * 14;
    unsigned char *g = calloc(1, *len);
    put16(g + 2, 1);
    put16(g + 4, n);
    for (int i = 0; i < n; i++) {
        unsigned char *e = g + 6 + i * 14;
        e[0] = sizes[i] >= 256 ? 0 : (unsigned char)sizes[i];
        e[1] = e[0];
        put16(e + 4, 1);
        put16(e + 6, bits[i]);
        put16(e + 12, ids[i]);
    }
    return g;
}

static int pe_icon_mem(const SmmBuf *pe, const char *path, int want, unsigned **argb, int *w, int *h)
{
    CHECK(smm_file_write(path, pe->p, pe->n) == 0);
    return smm_pe_icon(path, want, argb, w, h);
}

static void test_peicon(const char *work)
{
    char path[SMM_PATH];
    smm_join(path, sizeof path, work, "fake.exe");
    int sizes[3] = {16, 32, 48}, ids[3] = {1, 2, 3};
    int variants[][2] = {{32, 1}, {32, 0}, {24, 0}, {8, 0}, {4, 0}, {1, 0}};
    for (int pe64 = 0; pe64 < 2; pe64++)
        for (unsigned v = 0; v < sizeof variants / sizeof variants[0]; v++) {
            int bits = variants[v][0], alpha = variants[v][1];
            size_t n1, n2, n3, gn;
            unsigned char *d1 = make_dib(16, 16, bits, alpha, &n1), *d2 = make_dib(32, 32, bits, alpha, &n2), *d3 = make_dib(48, 48, bits, alpha, &n3);
            int gb[3] = {bits, bits, bits};
            unsigned char *g = make_group(sizes, gb, ids, 3, &gn);
            PeRes res[4] = {{3, 1, d1, n1}, {3, 2, d2, n2}, {3, 3, d3, n3}, {14, 1, g, gn}};
            SmmBuf pe = {0};
            build_pe(pe64, res, 4, &pe);
            unsigned *px = NULL;
            int w = 0, h = 0;
            CHECK(pe_icon_mem(&pe, path, 32, &px, &w, &h) == 1);
            CHECK(w == 32 && h == 32);
            if (px && w == 32) {
                if (bits == 32 && alpha) CHECK(px[5 * 32 + 5] == 0x80640A05u && px[0] == 0x80640A05u);
                if (bits == 32 && !alpha) CHECK(px[5 * 32 + 5] == 0xFFC8140Au);
                if (bits == 24) CHECK(px[5 * 32 + 5] == 0xFFC8140Au);
                if (bits != 32 || !alpha) CHECK(px[0] == 0 && (px[1] >> 24) == 255);
            }
            free(px);
            px = NULL;
            CHECK(pe_icon_mem(&pe, path, 100, &px, &w, &h) == 1 && w == 48);
            free(px);
            px = NULL;
            CHECK(pe_icon_mem(&pe, path, 1, &px, &w, &h) == 1 && w == 16);
            free(px);
            smm_buf_free(&pe);
            free(d1);
            free(d2);
            free(d3);
            free(g);
        }
    size_t pn, dn, gn;
    unsigned char *png = make_png(64, 64, &pn), *dib = make_dib(16, 16, 32, 1, &dn);
    int ps[2] = {16, 64}, pb[2] = {32, 32}, pi[2] = {1, 2};
    unsigned char *g = make_group(ps, pb, pi, 2, &gn);
    PeRes res[3] = {{3, 1, dib, dn}, {3, 2, png, pn}, {14, 7, g, gn}};
    SmmBuf pe = {0};
    build_pe(1, res, 3, &pe);
    unsigned *px = NULL;
    int w = 0, h = 0;
    CHECK(pe_icon_mem(&pe, path, 64, &px, &w, &h) == 1 && w == 64 && h == 64);
    CHECK(px && px[10 * 64 + 10] == 0xFFFF0000u);
    free(px);
    px = NULL;
    CHECK(pe_icon_mem(&pe, path, 16, &px, &w, &h) == 1 && w == 16);
    free(px);
    px = NULL;
    smm_buf_free(&pe);
    PeRes nogroup[1] = {{3, 1, dib, dn}};
    build_pe(0, nogroup, 1, &pe);
    CHECK(pe_icon_mem(&pe, path, 32, &px, &w, &h) == 0);
    smm_buf_free(&pe);
    CHECK(smm_pe_icon(path, 32, NULL, &w, &h) == 0);
    CHECK(smm_pe_icon("/nonexistent/none.exe", 32, &px, &w, &h) == 0);
    SmmBuf junk = {0};
    smm_buf_append(&junk, "MZ not a real exe", 17);
    CHECK(pe_icon_mem(&junk, path, 32, &px, &w, &h) == 0);
    smm_buf_free(&junk);
    build_pe(1, res, 3, &pe);
    unsigned seed = 12345;
    for (int round = 0; round < 3000; round++) {
        SmmBuf m = {0};
        smm_buf_append(&m, pe.p, pe.n);
        seed = seed * 1103515245u + 12345u;
        int kind = round % 3;
        if (kind == 0) m.n = (seed >> 8) % pe.n;
        else
            for (int f = 0; f < 1 + (int)(seed >> 28); f++) {
                seed = seed * 1103515245u + 12345u;
                size_t at = (seed >> 8) % (pe.n < 2048 ? pe.n : 2048);
                if (kind == 2) at = (seed >> 8) % pe.n;
                m.p[at] = (char)(seed >> 3);
            }
        unsigned *fz = NULL;
        int fw, fh;
        if (pe_icon_mem(&m, path, 48, &fz, &fw, &fh) == 1) {
            CHECK(fw > 0 && fh > 0 && fw <= 1024 && fh <= 1024);
            free(fz);
        }
        smm_buf_free(&m);
    }
    smm_buf_free(&pe);
    free(png);
    free(dib);
    free(g);
    remove(path);
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
    test_monkeforge(work);
    test_install_local(work);
    test_loadouts();
    test_config();
    test_melon(work);
    test_logs(work);
    test_steam();
    test_theme();
    test_peicon(work);
    test_game();
    test_packs(work);
    test_queue();
    printf("%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
