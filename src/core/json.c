#include "internal.h"
#include <stdlib.h>

typedef struct {
    const char *s, *end;
    int depth;
} Cur;

static void skip_ws(Cur *c)
{
    while (c->s < c->end && (*c->s == ' ' || *c->s == '\t' || *c->s == '\n' || *c->s == '\r')) c->s++;
}

static int hex4(const char *s)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char ch = s[i];
        v <<= 4;
        if (ch >= '0' && ch <= '9') v |= ch - '0';
        else if (ch >= 'a' && ch <= 'f') v |= ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') v |= ch - 'A' + 10;
        else return -1;
    }
    return v;
}

static void put_utf8(SmmBuf *b, unsigned cp)
{
    char o[4];
    int n;
    if (cp < 0x80) {
        o[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        o[0] = (char)(0xC0 | (cp >> 6));
        o[1] = (char)(0x80 | (cp & 63));
        n = 2;
    } else if (cp < 0x10000) {
        o[0] = (char)(0xE0 | (cp >> 12));
        o[1] = (char)(0x80 | ((cp >> 6) & 63));
        o[2] = (char)(0x80 | (cp & 63));
        n = 3;
    } else {
        o[0] = (char)(0xF0 | (cp >> 18));
        o[1] = (char)(0x80 | ((cp >> 12) & 63));
        o[2] = (char)(0x80 | ((cp >> 6) & 63));
        o[3] = (char)(0x80 | (cp & 63));
        n = 4;
    }
    smm_buf_append(b, o, (size_t)n);
}

static char *parse_string(Cur *c)
{
    if (c->s >= c->end || *c->s != '"') return NULL;
    c->s++;
    SmmBuf b = {0};
    while (c->s < c->end && *c->s != '"') {
        if (*c->s != '\\') {
            const char *a = c->s;
            while (c->s < c->end && *c->s != '"' && *c->s != '\\') c->s++;
            smm_buf_append(&b, a, (size_t)(c->s - a));
            continue;
        }
        c->s++;
        if (c->s >= c->end) break;
        char e = *c->s++;
        switch (e) {
        case 'n': smm_buf_append(&b, "\n", 1); break;
        case 't': smm_buf_append(&b, "\t", 1); break;
        case 'r': smm_buf_append(&b, "\r", 1); break;
        case 'b': smm_buf_append(&b, "\b", 1); break;
        case 'f': smm_buf_append(&b, "\f", 1); break;
        case 'u': {
            if (c->end - c->s < 4) {
                smm_buf_free(&b);
                return NULL;
            }
            int cp = hex4(c->s);
            if (cp < 0) {
                smm_buf_free(&b);
                return NULL;
            }
            c->s += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && c->end - c->s >= 6 && c->s[0] == '\\' && c->s[1] == 'u') {
                int lo = hex4(c->s + 2);
                if (lo >= 0xDC00 && lo < 0xE000) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    c->s += 6;
                }
            }
            put_utf8(&b, (unsigned)cp);
            break;
        }
        default: smm_buf_append(&b, &e, 1);
        }
    }
    if (c->s >= c->end) {
        smm_buf_free(&b);
        return NULL;
    }
    c->s++;
    smm_bufs_cstr(&b);
    return b.p;
}

static int parse_value(Cur *c, SmmJson *out);

static SmmJson *add_kid(SmmJson *parent)
{
    if (parent->n == parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->kids = smm_xrealloc(parent->kids, (size_t)parent->cap * sizeof(SmmJson));
    }
    SmmJson *k = &parent->kids[parent->n++];
    memset(k, 0, sizeof *k);
    return k;
}

static int parse_value(Cur *c, SmmJson *out)
{
    skip_ws(c);
    if (c->s >= c->end || c->depth > 200) return -1;
    char ch = *c->s;
    if (ch == '"') {
        out->type = SMM_J_STR;
        out->str = parse_string(c);
        return out->str ? 0 : -1;
    }
    if (ch == '{' || ch == '[') {
        int obj = ch == '{';
        out->type = obj ? SMM_J_OBJ : SMM_J_ARR;
        c->s++;
        c->depth++;
        skip_ws(c);
        if (c->s < c->end && *c->s == (obj ? '}' : ']')) {
            c->s++;
            c->depth--;
            return 0;
        }
        for (;;) {
            SmmJson *k = add_kid(out);
            skip_ws(c);
            if (obj) {
                k->key = parse_string(c);
                if (!k->key) return -1;
                skip_ws(c);
                if (c->s >= c->end || *c->s != ':') return -1;
                c->s++;
            }
            if (parse_value(c, k) != 0) return -1;
            skip_ws(c);
            if (c->s >= c->end) return -1;
            if (*c->s == ',') {
                c->s++;
                continue;
            }
            if (*c->s == (obj ? '}' : ']')) {
                c->s++;
                c->depth--;
                return 0;
            }
            return -1;
        }
    }
    if (c->end - c->s >= 4 && !strncmp(c->s, "true", 4)) {
        out->type = SMM_J_BOOL;
        out->b = 1;
        c->s += 4;
        return 0;
    }
    if (c->end - c->s >= 5 && !strncmp(c->s, "false", 5)) {
        out->type = SMM_J_BOOL;
        c->s += 5;
        return 0;
    }
    if (c->end - c->s >= 4 && !strncmp(c->s, "null", 4)) {
        out->type = SMM_J_NULL;
        c->s += 4;
        return 0;
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        char tmp[64];
        size_t n = 0;
        while (c->s < c->end && n < sizeof tmp - 1 &&
               ((*c->s >= '0' && *c->s <= '9') || *c->s == '-' || *c->s == '+' || *c->s == '.' || *c->s == 'e' || *c->s == 'E'))
            tmp[n++] = *c->s++;
        tmp[n] = 0;
        out->type = SMM_J_NUM;
        out->num = atof(tmp);
        return 0;
    }
    return -1;
}

static void smm_json_free_inner(SmmJson *j)
{
    for (int i = 0; i < j->n; i++) smm_json_free_inner(&j->kids[i]);
    free(j->kids);
    free(j->str);
    free(j->key);
}

void smm_json_free(SmmJson *j)
{
    if (!j) return;
    smm_json_free_inner(j);
    free(j);
}

SmmJson *smm_json_parse(const char *text, size_t n)
{
    Cur c = {text, text + n, 0};
    if (n >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) c.s += 3;
    SmmJson *j = smm_xmalloc(sizeof *j);
    memset(j, 0, sizeof *j);
    if (parse_value(&c, j) != 0) {
        smm_json_free(j);
        return NULL;
    }
    return j;
}

const SmmJson *smm_json_get(const SmmJson *o, const char *key)
{
    if (!o || o->type != SMM_J_OBJ) return NULL;
    for (int i = 0; i < o->n; i++)
        if (o->kids[i].key && !strcmp(o->kids[i].key, key)) return &o->kids[i];
    return NULL;
}

const SmmJson *smm_json_at(const SmmJson *a, int i)
{
    if (!a || (a->type != SMM_J_ARR && a->type != SMM_J_OBJ) || i < 0 || i >= a->n) return NULL;
    return &a->kids[i];
}

const char *smm_json_str(const SmmJson *o, const char *key, const char *def)
{
    const SmmJson *v = smm_json_get(o, key);
    return v && v->type == SMM_J_STR ? v->str : def;
}

double smm_json_num(const SmmJson *o, const char *key, double def)
{
    const SmmJson *v = smm_json_get(o, key);
    if (v && v->type == SMM_J_NUM) return v->num;
    if (v && v->type == SMM_J_BOOL) return v->b;
    return def;
}

int smm_json_bool(const SmmJson *o, const char *key, int def)
{
    const SmmJson *v = smm_json_get(o, key);
    if (v && v->type == SMM_J_BOOL) return v->b;
    if (v && v->type == SMM_J_NUM) return v->num != 0;
    return def;
}
