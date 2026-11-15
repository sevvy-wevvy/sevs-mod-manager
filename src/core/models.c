#include "internal.h"
#include <stdlib.h>

void smm_mods_add(SmmMods *m, const SmmMod *mod)
{
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 32;
        m->v = smm_xrealloc(m->v, (size_t)m->cap * sizeof(SmmMod));
    }
    m->v[m->n++] = *mod;
}

void smm_mods_free(SmmMods *m)
{
    for (int i = 0; i < m->n; i++) smm_strs_free(&m->v[i].deps);
    free(m->v);
    memset(m, 0, sizeof *m);
}

void smm_installed_free(SmmInstalledList *l)
{
    free(l->v);
    memset(l, 0, sizeof *l);
}

int smm_installed_update_available(const SmmInstalled *m)
{
    return m->known_version[0] && m->latest_version[0] && strcmp(m->known_version, m->latest_version) != 0;
}

void smm_loadouts_free(SmmLoadouts *l)
{
    for (int i = 0; i < l->n; i++) {
        smm_strs_free(&l->v[i].enabled);
        smm_strs_free(&l->v[i].disabled);
        smm_strs_free(&l->v[i].missing);
    }
    free(l->v);
    memset(l, 0, sizeof *l);
}

void smm_loadout_display_name(const SmmLoadout *l, char *out, size_t n)
{
    int blank = 1;
    for (const char *p = l->name; *p; p++)
        if (*p != ' ' && *p != '\t') blank = 0;
    if (blank) smm_fmt(out, n, "Loadout %d", l->number);
    else smm_copy(out, n, l->name);
}

void smm_manifest_free(SmmManifest *m)
{
    smm_strs_free(&m->mods);
}

void smm_packs_free(SmmPacks *p)
{
    for (int i = 0; i < p->n; i++) smm_manifest_free(&p->v[i].manifest);
    free(p->v);
    memset(p, 0, sizeof *p);
}

void smm_pack_display_name(const SmmPack *p, char *out, size_t n)
{
    int blank = 1;
    for (const char *s = p->manifest.name; *s; s++)
        if (*s != ' ' && *s != '\t') blank = 0;
    if (blank) smm_stem(p->path, out, n);
    else smm_copy(out, n, p->manifest.name);
}

void smm_communities_free(SmmCommunities *c)
{
    free(c->v);
    memset(c, 0, sizeof *c);
}

void smm_config_file_free(SmmConfigFile *f)
{
    smm_strs_free(&f->header);
    for (int i = 0; i < f->n_sections; i++) {
        SmmConfigSection *s = &f->sections[i];
        for (int k = 0; k < s->n; k++) {
            smm_strs_free(&s->v[k].description);
            smm_strs_free(&s->v[k].acceptable);
        }
        free(s->v);
    }
    free(f->sections);
    memset(f, 0, sizeof *f);
}

void smm_config_refs_free(SmmConfigRefs *r)
{
    free(r->v);
    memset(r, 0, sizeof *r);
}

void smm_log_sessions_free(SmmLogSessions *l)
{
    free(l->v);
    memset(l, 0, sizeof *l);
}

void smm_steam_games_free(SmmSteamGames *g)
{
    free(g->v);
    memset(g, 0, sizeof *g);
}
