#include "internal.h"
#include <stdlib.h>

static SmmConfigSection *add_section(SmmConfigFile *f, const char *name)
{
    if (f->n_sections == f->cap_sections) {
        f->cap_sections = f->cap_sections ? f->cap_sections * 2 : 8;
        f->sections = smm_xrealloc(f->sections, (size_t)f->cap_sections * sizeof(SmmConfigSection));
    }
    SmmConfigSection *s = &f->sections[f->n_sections++];
    memset(s, 0, sizeof *s);
    smm_copy(s->name, sizeof s->name, name);
    return s;
}

static SmmConfigEntry *add_entry(SmmConfigSection *s)
{
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = smm_xrealloc(s->v, (size_t)s->cap * sizeof(SmmConfigEntry));
    }
    SmmConfigEntry *e = &s->v[s->n++];
    memset(e, 0, sizeof *e);
    return e;
}

static const char *after(const char *t, const char *prefix)
{
    if (!smm_starts_with(t, prefix)) return NULL;
    return t + strlen(prefix);
}

static void trim_copy(char *dst, size_t n, const char *src)
{
    smm_copy(dst, n, src);
    smm_trim(dst);
}

typedef struct {
    SmmStrs desc;
    char type[40];
    int has_type;
    int has_default;
    char def[256];
    int has_range;
    double min, max;
    int has_values;
    SmmStrs values;
} Pending;

static void pending_clear(Pending *p)
{
    smm_strs_free(&p->desc);
    smm_strs_free(&p->values);
    memset(p, 0, sizeof *p);
}

static void split_values(SmmStrs *out, const char *text)
{
    char buf[1024];
    smm_copy(buf, sizeof buf, text);
    char *s = buf;
    while (s) {
        char *c = strchr(s, ',');
        if (c) *c = 0;
        smm_trim(s);
        if (*s) smm_strs_add(out, s);
        s = c ? c + 1 : NULL;
    }
}

int smm_config_parse_bepinex(const char *path, SmmConfigFile *file)
{
    SmmStrs lines = {0};
    if (smm_read_lines(path, &lines) != 0) return -1;
    memset(file, 0, sizeof *file);
    smm_copy(file->path, sizeof file->path, path);
    smm_stem(path, file->display_name, sizeof file->display_name);
    SmmConfigSection *section = NULL;
    int saw_section = 0;
    Pending pend;
    memset(&pend, 0, sizeof pend);
    for (int i = 0; i < lines.n; i++) {
        char t[2048];
        smm_copy(t, sizeof t, lines.v[i]);
        smm_trim(t);
        size_t tl = strlen(t);
        if (tl == 0) continue;
        if (t[0] == '[' && t[tl - 1] == ']') {
            t[tl - 1] = 0;
            section = add_section(file, t + 1);
            saw_section = 1;
            pending_clear(&pend);
            continue;
        }
        const char *r;
        if (smm_starts_with(t, "## ")) {
            if (!saw_section) smm_strs_add(&file->header, t + 3);
            else smm_strs_add(&pend.desc, t + 3);
            continue;
        }
        if ((r = after(t, "# Setting type:"))) {
            trim_copy(pend.type, sizeof pend.type, r);
            pend.has_type = 1;
            continue;
        }
        if ((r = after(t, "# Default value:"))) {
            trim_copy(pend.def, sizeof pend.def, r);
            pend.has_default = 1;
            continue;
        }
        if ((r = after(t, "# Acceptable value range:"))) {
            char tmp[256], a[64], b[64], c[64], d[64], extra[64];
            trim_copy(tmp, sizeof tmp, r);
            if (sscanf(tmp, "%63s %63s %63s %63s %63s", a, b, c, d, extra) == 4) {
                char *e1, *e2;
                double mn = strtod(b, &e1), mx = strtod(d, &e2);
                if (!*e1 && !*e2) {
                    pend.has_range = 1;
                    pend.min = mn;
                    pend.max = mx;
                }
            }
            continue;
        }
        if ((r = after(t, "# Acceptable values:"))) {
            pend.has_values = 1;
            split_values(&pend.values, r);
            continue;
        }
        if (t[0] == '#') continue;
        char *eq = strchr(t, '=');
        if (!eq || !section) continue;
        *eq = 0;
        SmmConfigEntry *e = add_entry(section);
        trim_copy(e->key, sizeof e->key, t);
        trim_copy(e->value, sizeof e->value, eq + 1);
        e->description = pend.desc;
        memset(&pend.desc, 0, sizeof pend.desc);
        smm_copy(e->setting_type, sizeof e->setting_type, pend.has_type ? pend.type : "String");
        e->has_default = pend.has_default;
        smm_copy(e->default_value, sizeof e->default_value, pend.def);
        e->has_range = pend.has_range;
        e->range_min = pend.min;
        e->range_max = pend.max;
        if (pend.has_values) {
            e->acceptable = pend.values;
            memset(&pend.values, 0, sizeof pend.values);
        }
        pending_clear(&pend);
    }
    pending_clear(&pend);
    smm_strs_free(&lines);
    return 0;
}

static int save_bepinex(const SmmConfigFile *f)
{
    SmmBuf b = {0};
    for (int i = 0; i < f->header.n; i++) smm_buf_printf(&b, "## %s\n", f->header.v[i]);
    if (f->header.n) smm_buf_append(&b, "\n", 1);
    for (int s = 0; s < f->n_sections; s++) {
        const SmmConfigSection *sec = &f->sections[s];
        smm_buf_printf(&b, "[%s]\n\n", sec->name);
        for (int k = 0; k < sec->n; k++) {
            const SmmConfigEntry *e = &sec->v[k];
            for (int d = 0; d < e->description.n; d++) smm_buf_printf(&b, "## %s\n", e->description.v[d]);
            smm_buf_printf(&b, "# Setting type: %s\n", e->setting_type);
            if (e->has_default) smm_buf_printf(&b, "# Default value: %s\n", e->default_value);
            if (e->has_range) smm_buf_printf(&b, "# Acceptable value range: From %g to %g\n", e->range_min, e->range_max);
            if (e->acceptable.n) {
                smm_buf_append(&b, "# Acceptable values: ", 21);
                for (int v = 0; v < e->acceptable.n; v++) smm_buf_printf(&b, "%s%s", v ? ", " : "", e->acceptable.v[v]);
                smm_buf_append(&b, "\n", 1);
            }
            smm_buf_printf(&b, "%s = %s\n\n", e->key, e->value);
        }
    }
    smm_bufs_cstr(&b);
    int rc = smm_file_write(f->path, b.p, b.n);
    smm_buf_free(&b);
    return rc;
}

static int melon_cfg_path(char *out, size_t n)
{
    char g[SMM_PATH], ud[SMM_PATH];
    if (!smm_game_dir(g, sizeof g)) return 0;
    smm_join(ud, sizeof ud, g, "UserData");
    smm_join(out, n, ud, "MelonPreferences.cfg");
    return 1;
}

static void parse_toml_value(const char *raw, char *out, size_t n, char *type, size_t tn)
{
    size_t l = strlen(raw);
    char *end;
    if (!strcmp(raw, "true") || !strcmp(raw, "false")) {
        smm_copy(type, tn, "Boolean");
        smm_copy(out, n, raw);
    } else if (l >= 2 && raw[0] == '"' && raw[l - 1] == '"') {
        smm_copy(type, tn, "String");
        size_t k = 0;
        for (size_t i = 1; i + 1 < l && k + 1 < n; i++) {
            if (raw[i] == '\\' && i + 2 < l && (raw[i + 1] == '"' || raw[i + 1] == '\\')) i++;
            out[k++] = raw[i];
        }
        out[k] = 0;
    } else if (raw[0] == '[' && raw[l - 1] == ']') {
        smm_copy(type, tn, "Array");
        smm_copy(out, n, raw);
    } else {
        strtol(raw, &end, 10);
        if (*raw && !*end) smm_copy(type, tn, "Int32");
        else {
            strtod(raw, &end);
            smm_copy(type, tn, *raw && !*end ? "Single" : "String");
        }
        smm_copy(out, n, raw);
    }
}

static int parse_melon_all(const char *path, SmmConfigFile *file)
{
    SmmStrs lines = {0};
    memset(file, 0, sizeof *file);
    if (smm_read_lines(path, &lines) != 0) return -1;
    SmmConfigSection *sec = NULL;
    SmmStrs desc = {0};
    for (int i = 0; i < lines.n; i++) {
        char t[2048];
        smm_copy(t, sizeof t, lines.v[i]);
        smm_trim(t);
        size_t tl = strlen(t);
        if (!tl) continue;
        if (t[0] == '[' && t[tl - 1] == ']') {
            t[tl - 1] = 0;
            sec = add_section(file, t + 1);
            smm_strs_free(&desc);
            continue;
        }
        if (t[0] == '#') {
            char c[2048];
            trim_copy(c, sizeof c, t + 1);
            smm_strs_add(&desc, c);
            continue;
        }
        char *eq = strchr(t, '=');
        if (!eq || !sec) continue;
        *eq = 0;
        SmmConfigEntry *e = add_entry(sec);
        trim_copy(e->key, sizeof e->key, t);
        char raw[1024];
        trim_copy(raw, sizeof raw, eq + 1);
        parse_toml_value(raw, e->value, sizeof e->value, e->setting_type, sizeof e->setting_type);
        e->description = desc;
        memset(&desc, 0, sizeof desc);
    }
    smm_strs_free(&desc);
    smm_strs_free(&lines);
    return 0;
}

static void format_toml(const SmmConfigEntry *e, SmmBuf *b)
{
    const char *t = e->setting_type;
    if (!strcmp(t, "Boolean")) {
        smm_buf_append(b, smm_strcasecmp(e->value, "true") == 0 ? "true" : "false", smm_strcasecmp(e->value, "true") == 0 ? 4 : 5);
    } else if (!strcmp(t, "Int32") || !strcmp(t, "Single") || !strcmp(t, "Array")) {
        smm_buf_append(b, e->value, strlen(e->value));
    } else {
        smm_buf_append(b, "\"", 1);
        for (const char *p = e->value; *p; p++) {
            if (*p == '\\' || *p == '"') smm_buf_append(b, "\\", 1);
            smm_buf_append(b, p, 1);
        }
        smm_buf_append(b, "\"", 1);
    }
}

static int save_melon(const SmmConfigFile *f)
{
    char path[SMM_PATH];
    if (!melon_cfg_path(path, sizeof path) || f->n_sections == 0) return -1;
    SmmConfigFile all;
    memset(&all, 0, sizeof all);
    if (smm_os_is_file(path)) parse_melon_all(path, &all);
    const SmmConfigSection *edited = &f->sections[0];
    int idx = -1;
    for (int i = 0; i < all.n_sections; i++)
        if (!strcmp(all.sections[i].name, edited->name)) idx = i;
    SmmBuf b = {0};
    int count = all.n_sections + (idx < 0 ? 1 : 0);
    for (int s = 0; s < count; s++) {
        const SmmConfigSection *sec = idx >= 0 && s == idx ? edited : s < all.n_sections ? &all.sections[s] : edited;
        smm_buf_printf(&b, "[%s]\n", sec->name);
        for (int k = 0; k < sec->n; k++) {
            const SmmConfigEntry *e = &sec->v[k];
            if (e->description.n) {
                smm_buf_append(&b, "# ", 2);
                for (int d = 0; d < e->description.n; d++) smm_buf_printf(&b, "%s%s", d ? " " : "", e->description.v[d]);
                smm_buf_append(&b, "\n", 1);
            }
            smm_buf_printf(&b, "%s = ", e->key);
            format_toml(e, &b);
            smm_buf_append(&b, "\n", 1);
        }
        smm_buf_append(&b, "\n", 1);
    }
    smm_bufs_cstr(&b);
    char dir[SMM_PATH];
    smm_dirname(path, dir, sizeof dir);
    smm_os_mkdir_p(dir);
    int rc = smm_file_write(path, b.p, b.n);
    smm_buf_free(&b);
    smm_config_file_free(&all);
    return rc;
}

static void add_ref(SmmConfigRefs *r, const char *name, const char *path)
{
    if (r->n == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 16;
        r->v = smm_xrealloc(r->v, (size_t)r->cap * sizeof(SmmConfigRef));
    }
    smm_copy(r->v[r->n].display_name, sizeof r->v[r->n].display_name, name);
    smm_copy(r->v[r->n].path, sizeof r->v[r->n].path, path);
    r->n++;
}

static int cmp_refs(const void *a, const void *b)
{
    return smm_strcasecmp(((const SmmConfigRef *)a)->display_name, ((const SmmConfigRef *)b)->display_name);
}

int smm_config_list(SmmConfigRefs *out)
{
    if (smm_detect_loader() == SMM_LOADER_MELON) {
        char p[SMM_PATH];
        if (!melon_cfg_path(p, sizeof p) || !smm_os_is_file(p)) return 0;
        SmmConfigFile f;
        if (parse_melon_all(p, &f) != 0) return 0;
        for (int i = 0; i < f.n_sections; i++) {
            char virt[300];
            smm_fmt(virt, sizeof virt, "melon:%s", f.sections[i].name);
            add_ref(out, f.sections[i].name, virt);
        }
        smm_config_file_free(&f);
    } else {
        char bep[SMM_PATH], cfg[SMM_PATH];
        if (!smm_bepinex_dir(bep, sizeof bep)) return 0;
        smm_join(cfg, sizeof cfg, bep, "config");
        SmmDirList l = {0};
        if (smm_os_list(cfg, &l) != 0) return 0;
        for (int i = 0; i < l.n; i++) {
            if (l.v[i].is_dir || !smm_ends_with_nocase(l.v[i].name, ".cfg")) continue;
            char f[SMM_PATH], stem[256];
            smm_join(f, sizeof f, cfg, l.v[i].name);
            smm_stem(f, stem, sizeof stem);
            add_ref(out, stem, f);
        }
        smm_dirlist_free(&l);
    }
    if (out->n > 1) qsort(out->v, (size_t)out->n, sizeof(SmmConfigRef), cmp_refs);
    return out->n;
}

int smm_config_parse(const char *path, SmmConfigFile *out)
{
    if (smm_starts_with(path, "melon:")) {
        const char *cat = path + 6;
        memset(out, 0, sizeof *out);
        smm_copy(out->path, sizeof out->path, path);
        smm_copy(out->display_name, sizeof out->display_name, cat);
        char p[SMM_PATH];
        if (melon_cfg_path(p, sizeof p) && smm_os_is_file(p)) {
            SmmConfigFile all;
            if (parse_melon_all(p, &all) == 0) {
                for (int i = 0; i < all.n_sections; i++) {
                    if (strcmp(all.sections[i].name, cat) != 0) continue;
                    SmmConfigSection *dst = add_section(out, cat);
                    *dst = all.sections[i];
                    memset(&all.sections[i], 0, sizeof all.sections[i]);
                    break;
                }
                smm_config_file_free(&all);
            }
        }
        return 0;
    }
    return smm_config_parse_bepinex(path, out);
}

int smm_config_save(const SmmConfigFile *f)
{
    if (smm_starts_with(f->path, "melon:")) return save_melon(f);
    return save_bepinex(f);
}
