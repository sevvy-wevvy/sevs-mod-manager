#include "internal.h"
#include <stdlib.h>

#define API_BASE "https://sevvy-wevvy.com/mods/sb/api.php"
#define LINK_BASE "https://3gv.org/link/"
#define VERSION_URL "https://raw.githubusercontent.com/sevvy-wevvy/sevs-mod-manager/refs/heads/main/ver.txt"

static char g_token[512];

void smm_sb_set_token(const char *token)
{
    smm_copy(g_token, sizeof g_token, token ? token : "");
}

const char *smm_sb_token(void)
{
    return g_token[0] ? g_token : NULL;
}

static void add_query(char *url, size_t n, const char *k, const char *v)
{
    char ek[256], ev[1024];
    smm_url_escape(k, ek, sizeof ek);
    smm_url_escape(v, ev, sizeof ev);
    size_t l = strlen(url);
    smm_fmt(url + l, n - l, "&%s=%s", ek, ev);
}

static int api_get(const char *action, const char *const *kv, int nkv, SmmBuf *out)
{
    char url[2048], ea[128];
    smm_url_escape(action, ea, sizeof ea);
    smm_fmt(url, sizeof url, "%s?action=%s", API_BASE, ea);
    for (int i = 0; i + 1 < nkv * 2; i += 2) add_query(url, sizeof url, kv[i], kv[i + 1]);
    if (g_token[0]) add_query(url, sizeof url, "token", g_token);
    int code = smm_http_get(url, out);
    return code >= 200 && code < 300 ? 0 : -1;
}

static int api_post(const char *action, SmmMap *fields, SmmBuf *out)
{
    smm_map_set(fields, "action", action);
    if (g_token[0]) smm_map_set(fields, "token", g_token);
    int code = smm_http_post_form(API_BASE, fields, out);
    return code >= 200 && code < 300 ? 0 : -1;
}

int smm_sb_register_game(const char *slug)
{
    const char *kv[] = {"slug", slug};
    SmmBuf b = {0};
    int id = 0;
    if (api_get("register_game", kv, 1, &b) == 0) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        id = (int)smm_json_num(j, "id", 0);
        smm_json_free(j);
    }
    smm_buf_free(&b);
    return id;
}

int smm_sb_list_mods(const char *tab, int page, int game_id, SmmMods *out)
{
    char pg[16], gid[16];
    smm_fmt(pg, sizeof pg, "%d", page);
    smm_fmt(gid, sizeof gid, "%d", game_id);
    const char *kv[6] = {"tab", tab, "page", pg, "game_id", gid};
    SmmBuf b = {0};
    if (api_get("list_mods", kv, game_id > 0 ? 3 : 2, &b) != 0) {
        smm_buf_free(&b);
        return -1;
    }
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    const SmmJson *arr = j && j->type == SMM_J_ARR ? j : smm_json_get(j, "mods");
    if (arr && arr->type == SMM_J_ARR) {
        for (int i = 0; i < arr->n; i++) {
            const SmmJson *o = &arr->kids[i];
            if (o->type != SMM_J_OBJ) continue;
            SmmMod m;
            memset(&m, 0, sizeof m);
            m.id = (int)smm_json_num(o, "id", 0);
            smm_copy(m.name, sizeof m.name, smm_json_str(o, "name", ""));
            smm_copy(m.dll_name, sizeof m.dll_name, smm_json_str(o, "dll_name", ""));
            smm_copy(m.repo_url, sizeof m.repo_url, smm_json_str(o, "repo_url", ""));
            smm_copy(m.description, sizeof m.description, smm_json_str(o, "description", ""));
            smm_copy(m.author, sizeof m.author, smm_json_str(o, "author_username", ""));
            m.upvotes = (int)smm_json_num(o, "upvotes", 0);
            m.verified = (int)smm_json_num(o, "is_verified", 0) == 1;
            m.featured = (int)smm_json_num(o, "is_featured", 0) == 1;
            m.source = SMM_SRC_API;
            smm_mods_add(out, &m);
        }
    }
    smm_json_free(j);
    return out->n;
}

int smm_sb_user_upvoted(int mod_id)
{
    if (!g_token[0]) return 0;
    char id[16];
    smm_fmt(id, sizeof id, "%d", mod_id);
    const char *kv[] = {"mod_id", id};
    SmmBuf b = {0};
    int up = 0;
    if (api_get("get_user_upvote", kv, 1, &b) == 0) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        up = smm_json_bool(j, "upvoted", 0);
        smm_json_free(j);
    }
    smm_buf_free(&b);
    return up;
}

int smm_sb_toggle_upvote(int mod_id, int *upvoted)
{
    SmmMap f = {0};
    char id[16];
    smm_fmt(id, sizeof id, "%d", mod_id);
    smm_map_set(&f, "mod_id", id);
    SmmBuf b = {0};
    int rc = api_post("toggle_upvote", &f, &b);
    if (rc == 0) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        if (upvoted) *upvoted = smm_json_bool(j, "upvoted", 0);
        smm_json_free(j);
    }
    smm_buf_free(&b);
    smm_map_free(&f);
    return rc;
}

int smm_sb_validate_token(const char *token, char *username, size_t n)
{
    SmmMap f = {0};
    smm_map_set(&f, "token", token);
    SmmBuf b = {0};
    int ok = 0;
    if (api_post("validate_token", &f, &b) == 0) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        if (smm_json_bool(j, "valid", 0)) {
            smm_copy(username, n, smm_json_str(j, "username", ""));
            ok = 1;
        }
        smm_json_free(j);
    }
    smm_buf_free(&b);
    smm_map_free(&f);
    return ok;
}

static int link_get(const char *query, const char *field, char *out, size_t n)
{
    char url[512];
    smm_fmt(url, sizeof url, "%s?%s", LINK_BASE, query);
    SmmBuf b = {0};
    int code = smm_http_get(url, &b);
    int ok = 0;
    if (code >= 200 && code < 300) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        const char *v = smm_json_str(j, field, NULL);
        if (v && *v) {
            smm_copy(out, n, v);
            ok = 1;
        }
        smm_json_free(j);
    }
    smm_buf_free(&b);
    return ok;
}

int smm_sb_login_code(char *out, size_t n)
{
    return link_get("action=generate", "code", out, n);
}

int smm_sb_poll_login(const char *code, char *token_out, size_t n)
{
    char ec[128], q[256];
    smm_url_escape(code, ec, sizeof ec);
    smm_fmt(q, sizeof q, "action=status&code=%s", ec);
    return link_get(q, "token", token_out, n);
}

int smm_sb_latest_tag(const char *owner, const char *repo, char *out, size_t n)
{
    char url[512];
    smm_fmt(url, sizeof url, "https://api.github.com/repos/%s/%s/releases/latest", owner, repo);
    SmmBuf b = {0};
    int code = smm_http_get(url, &b);
    int ok = 0;
    if (code >= 200 && code < 300) {
        SmmJson *j = smm_json_parse(b.p, b.n);
        const char *t = smm_json_str(j, "tag_name", NULL);
        if (t && *t) {
            smm_copy(out, n, t);
            ok = 1;
        }
        smm_json_free(j);
    }
    smm_buf_free(&b);
    return ok;
}

int smm_ts_list_packages(const char *community, SmmMods *out)
{
    char ec[256], url[512];
    smm_url_escape(community, ec, sizeof ec);
    smm_fmt(url, sizeof url, "https://thunderstore.io/c/%s/api/v1/package/", ec);
    SmmBuf b = {0};
    int code = smm_http_get(url, &b);
    if (code < 200 || code >= 300) {
        smm_buf_free(&b);
        if (code >= 0) smm_set_error("Thunderstore answered with HTTP %d", code);
        return -1;
    }
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    if (!j || j->type != SMM_J_ARR) {
        smm_json_free(j);
        smm_set_error("Couldn't read the Thunderstore package list");
        return -1;
    }
    int id = 1;
    for (int i = 0; i < j->n; i++) {
        const SmmJson *pkg = &j->kids[i];
        const SmmJson *vers = smm_json_get(pkg, "versions");
        if (!vers || vers->type != SMM_J_ARR || vers->n == 0) continue;
        const SmmJson *latest = &vers->kids[0];
        SmmMod m;
        memset(&m, 0, sizeof m);
        m.id = id++;
        smm_copy(m.name, sizeof m.name, smm_json_str(pkg, "name", ""));
        smm_copy(m.dll_name, sizeof m.dll_name, smm_json_str(pkg, "full_name", ""));
        smm_copy(m.author, sizeof m.author, smm_json_str(pkg, "owner", ""));
        smm_copy(m.description, sizeof m.description, smm_json_str(latest, "description", ""));
        smm_copy(m.repo_url, sizeof m.repo_url, smm_json_str(pkg, "package_url", ""));
        m.upvotes = (int)smm_json_num(pkg, "rating_score", 0);
        m.featured = smm_json_bool(pkg, "is_pinned", 0);
        m.verified = !smm_json_bool(pkg, "is_deprecated", 0);
        m.source = SMM_SRC_THUNDERSTORE;
        smm_copy(m.download_url, sizeof m.download_url, smm_json_str(latest, "download_url", ""));
        smm_copy(m.icon_url, sizeof m.icon_url, smm_json_str(latest, "icon", ""));
        smm_copy(m.version, sizeof m.version, smm_json_str(latest, "version_number", ""));
        const SmmJson *deps = smm_json_get(latest, "dependencies");
        if (deps && deps->type == SMM_J_ARR)
            for (int k = 0; k < deps->n; k++)
                if (deps->kids[k].type == SMM_J_STR) smm_strs_add(&m.deps, deps->kids[k].str);
        smm_mods_add(out, &m);
    }
    smm_json_free(j);
    return out->n;
}

static int cmp_comm(const void *a, const void *b)
{
    return smm_strcasecmp(((const SmmCommunity *)a)->name, ((const SmmCommunity *)b)->name);
}

int smm_ts_list_communities(SmmCommunities *out)
{
    SmmBuf b = {0};
    int code = smm_http_get("https://thunderstore.io/api/cyberstorm/community/?page_size=1000", &b);
    if (code < 200 || code >= 300) {
        smm_buf_free(&b);
        return -1;
    }
    SmmJson *j = smm_json_parse(b.p, b.n);
    smm_buf_free(&b);
    const SmmJson *res = smm_json_get(j, "results");
    if (res && res->type == SMM_J_ARR) {
        for (int i = 0; i < res->n; i++) {
            const SmmJson *c = &res->kids[i];
            if (out->n == out->cap) {
                out->cap = out->cap ? out->cap * 2 : 64;
                out->v = smm_xrealloc(out->v, (size_t)out->cap * sizeof(SmmCommunity));
            }
            smm_copy(out->v[out->n].name, sizeof out->v[out->n].name, smm_json_str(c, "name", ""));
            smm_copy(out->v[out->n].identifier, sizeof out->v[out->n].identifier, smm_json_str(c, "identifier", ""));
            out->n++;
        }
    }
    smm_json_free(j);
    if (out->n > 1) qsort(out->v, (size_t)out->n, sizeof(SmmCommunity), cmp_comm);
    return out->n;
}

int smm_ts_guess_community(const char *game_name, char *out, size_t n)
{
    SmmCommunities c = {0};
    smm_ts_list_communities(&c);
    char target[256], a[256], b[256];
    smm_derive_slug(game_name, target, sizeof target);
    int found = 0;
    for (int i = 0; i < c.n && !found; i++) {
        smm_derive_slug(c.v[i].name, a, sizeof a);
        smm_derive_slug(c.v[i].identifier, b, sizeof b);
        if (!strcmp(a, target) || !strcmp(b, target)) {
            smm_copy(out, n, c.v[i].identifier);
            found = 1;
        }
    }
    smm_communities_free(&c);
    return found;
}

int smm_update_latest_version(void)
{
    char url[512];
    smm_fmt(url, sizeof url, "%s?date=%lld", VERSION_URL, smm_os_now());
    SmmBuf b = {0};
    int code = smm_http_get(url, &b);
    int v = -1;
    if (code >= 200 && code < 300 && b.p) {
        char *end;
        long x = strtol(b.p, &end, 10);
        while (*end == ' ' || *end == '\n' || *end == '\r') end++;
        if (end != b.p && !*end) v = (int)x;
    }
    smm_buf_free(&b);
    return v;
}
