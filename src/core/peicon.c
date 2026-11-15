#include "internal.h"
#include <stdlib.h>

#define RSRC_MAX (96u * 1024 * 1024)
#define ICON_MAX (8u * 1024 * 1024)
#define RT_ICON 3
#define RT_GROUP_ICON 14

typedef struct {
    unsigned char *p;
    size_t n;
    unsigned va;
} Rsrc;

static unsigned rd16(const unsigned char *p)
{
    return p[0] | (unsigned)p[1] << 8;
}

static unsigned rd32(const unsigned char *p)
{
    return p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static int fread_at(FILE *f, unsigned long long off, void *dst, size_t n)
{
    if (off > 0x7FFFFFFFull) return 0;
    if (fseek(f, (long)off, SEEK_SET) != 0) return 0;
    return fread(dst, 1, n, f) == n;
}

static int load_rsrc(FILE *f, Rsrc *rs)
{
    unsigned char head[64];
    if (!fread_at(f, 0, head, 64) || head[0] != 'M' || head[1] != 'Z') return 0;
    unsigned lfanew = rd32(head + 60);
    unsigned char nt[24 + 240];
    memset(nt, 0, sizeof nt);
    if (!fread_at(f, lfanew, nt, 24 + 112)) return 0;
    if (memcmp(nt, "PE\0\0", 4) != 0) return 0;
    unsigned nsec = rd16(nt + 6), optsize = rd16(nt + 20);
    unsigned magic = rd16(nt + 24);
    unsigned dd_off, min_opt;
    if (magic == 0x10b) {
        dd_off = 24 + 96;
        min_opt = 96 + 8 * 3;
    } else if (magic == 0x20b) {
        dd_off = 24 + 112;
        min_opt = 112 + 8 * 3;
    } else return 0;
    if (optsize < min_opt || nsec == 0 || nsec > 96) return 0;
    unsigned char dd[8];
    if (!fread_at(f, (unsigned long long)lfanew + dd_off + 16, dd, 8)) return 0;
    unsigned rva = rd32(dd), size = rd32(dd + 4);
    if (rva == 0 || size < 16) return 0;
    unsigned char sec[96 * 40];
    if (!fread_at(f, (unsigned long long)lfanew + 24 + optsize, sec, (size_t)nsec * 40)) return 0;
    for (unsigned i = 0; i < nsec; i++) {
        const unsigned char *s = sec + i * 40;
        unsigned vsize = rd32(s + 8), va = rd32(s + 12), rawsize = rd32(s + 16), rawptr = rd32(s + 20);
        unsigned span = rawsize < vsize || vsize == 0 ? rawsize : vsize;
        if (rva < va || (unsigned long long)rva - va >= span) continue;
        if (span > RSRC_MAX || rawptr == 0) return 0;
        unsigned char *buf = malloc(span);
        if (!buf) return 0;
        size_t got = 0;
        if (fseek(f, (long)rawptr, SEEK_SET) == 0) got = fread(buf, 1, span, f);
        if (got < 16) {
            free(buf);
            return 0;
        }
        rs->p = buf;
        rs->n = got;
        rs->va = va;
        return 1;
    }
    return 0;
}

static int rva_span(const Rsrc *rs, unsigned rva, unsigned size, const unsigned char **out)
{
    if (rva < rs->va) return 0;
    unsigned long long off = (unsigned long long)rva - rs->va;
    if (off + size > rs->n) return 0;
    *out = rs->p + off;
    return 1;
}

static int dir_entry(const Rsrc *rs, unsigned dir_off, int want_id, int nth, unsigned *target, int *is_dir, unsigned *id_out)
{
    if ((unsigned long long)dir_off + 16 > rs->n) return 0;
    const unsigned char *d = rs->p + dir_off;
    unsigned named = rd16(d + 12), ids = rd16(d + 14);
    unsigned total = named + ids;
    if ((unsigned long long)dir_off + 16 + (unsigned long long)total * 8 > rs->n) return 0;
    int seen = 0;
    for (unsigned i = named; i < total; i++) {
        const unsigned char *e = d + 16 + i * 8;
        unsigned id = rd32(e), off = rd32(e + 4);
        if (id & 0x80000000u) continue;
        if (want_id >= 0 && (int)id != want_id) continue;
        if (seen++ != nth) continue;
        *is_dir = (off & 0x80000000u) != 0;
        *target = off & 0x7FFFFFFFu;
        if (id_out) *id_out = id;
        return 1;
    }
    return 0;
}

static int leaf_data(const Rsrc *rs, unsigned off, int is_dir, const unsigned char **data, unsigned *size)
{
    int depth = 0;
    while (is_dir) {
        if (++depth > 2) return 0;
        unsigned next;
        int nd;
        if (!dir_entry(rs, off, -1, 0, &next, &nd, NULL)) return 0;
        off = next;
        is_dir = nd;
    }
    if ((unsigned long long)off + 16 > rs->n) return 0;
    unsigned rva = rd32(rs->p + off), sz = rd32(rs->p + off + 4);
    if (sz == 0 || sz > ICON_MAX) return 0;
    if (!rva_span(rs, rva, sz, data)) return 0;
    *size = sz;
    return 1;
}

static int type_dir(const Rsrc *rs, int type, unsigned *off)
{
    int is_dir;
    if (!dir_entry(rs, 0, type, 0, off, &is_dir, NULL)) return 0;
    return is_dir;
}

static int find_resource(const Rsrc *rs, int type, int id, int nth, const unsigned char **data, unsigned *size, unsigned *id_out)
{
    unsigned tdir;
    if (!type_dir(rs, type, &tdir)) return 0;
    unsigned off;
    int is_dir;
    if (!dir_entry(rs, tdir, id, nth, &off, &is_dir, id_out)) return 0;
    return leaf_data(rs, off, is_dir, data, size);
}

typedef struct {
    int size, bits, id, tried;
} Cand;

static int pick(const Cand *c, int n, int want)
{
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (c[i].tried) continue;
        if (best < 0) {
            best = i;
            continue;
        }
        const Cand *a = &c[i], *b = &c[best];
        int a_fits = a->size >= want, b_fits = b->size >= want;
        int better;
        if (a_fits != b_fits) better = a_fits;
        else if (a->size != b->size) better = a_fits ? a->size < b->size : a->size > b->size;
        else better = a->bits > b->bits;
        if (better) best = i;
    }
    return best;
}

static int is_png(const unsigned char *p, size_t n)
{
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    return n >= 8 && memcmp(p, sig, 8) == 0;
}

static unsigned premul(unsigned a, unsigned r, unsigned g, unsigned b)
{
    if (a != 255) {
        r = (r * a + 127) / 255;
        g = (g * a + 127) / 255;
        b = (b * a + 127) / 255;
    }
    return a << 24 | r << 16 | g << 8 | b;
}

static int decode_dib(const unsigned char *d, size_t n, unsigned **argb, int *ow, int *oh)
{
    if (n < 40 || rd32(d) < 40) return 0;
    unsigned hsize = rd32(d);
    long w = (int)rd32(d + 4), h2 = (int)rd32(d + 8);
    unsigned bits = rd16(d + 14), comp = rd32(d + 16), used = rd32(d + 32);
    if (w <= 0 || w > 1024 || h2 == 0) return 0;
    long h = (h2 < 0 ? -h2 : h2) / 2;
    int top_down = h2 < 0;
    if (h <= 0 || h > 1024) return 0;
    if (comp != 0 && comp != 3) return 0;
    if (bits != 1 && bits != 4 && bits != 8 && bits != 24 && bits != 32) return 0;
    unsigned ncol = 0;
    if (bits <= 8) {
        ncol = used ? used : 1u << bits;
        if (ncol > (1u << bits)) return 0;
    }
    size_t pal_off = hsize;
    if (hsize == 40 && comp == 3) pal_off += 12;
    size_t px_off = pal_off + (size_t)ncol * 4;
    size_t stride = (((size_t)w * bits + 31) / 32) * 4;
    size_t mstride = (((size_t)w + 31) / 32) * 4;
    size_t xor_size = stride * (size_t)h;
    if (px_off + xor_size > n) return 0;
    size_t mask_off = px_off + xor_size;
    int has_mask = mask_off + mstride * (size_t)h <= n;
    unsigned *out = malloc((size_t)w * (size_t)h * 4);
    if (!out) return 0;
    int any_alpha = 0;
    if (bits == 32)
        for (long i = 0; i < w * h && !any_alpha; i++) {
            long y = i / w, x = i % w;
            if (d[px_off + (size_t)y * stride + (size_t)x * 4 + 3]) any_alpha = 1;
        }
    for (long y = 0; y < h; y++) {
        long sy = top_down ? y : h - 1 - y;
        const unsigned char *row = d + px_off + (size_t)sy * stride;
        const unsigned char *mrow = has_mask ? d + mask_off + (size_t)sy * mstride : NULL;
        for (long x = 0; x < w; x++) {
            unsigned r, g, b, a = 255;
            if (bits == 32) {
                b = row[x * 4];
                g = row[x * 4 + 1];
                r = row[x * 4 + 2];
                a = any_alpha ? row[x * 4 + 3] : 255;
            } else if (bits == 24) {
                b = row[x * 3];
                g = row[x * 3 + 1];
                r = row[x * 3 + 2];
            } else {
                unsigned idx;
                if (bits == 8) idx = row[x];
                else if (bits == 4) idx = (row[x / 2] >> (x % 2 ? 0 : 4)) & 15;
                else idx = (row[x / 8] >> (7 - x % 8)) & 1;
                if (idx >= ncol) idx = 0;
                const unsigned char *c = d + pal_off + (size_t)idx * 4;
                b = c[0];
                g = c[1];
                r = c[2];
            }
            if (mrow && (bits != 32 || !any_alpha) && (mrow[x / 8] >> (7 - x % 8) & 1)) a = 0;
            out[y * w + x] = premul(a, r, g, b);
        }
    }
    *argb = out;
    *ow = (int)w;
    *oh = (int)h;
    return 1;
}

static unsigned be32(const unsigned char *p)
{
    return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3];
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

static int decode_png(const unsigned char *d, size_t n, unsigned **argb, int *ow, int *oh)
{
    if (!is_png(d, n)) return 0;
    size_t pos = 8;
    unsigned w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    unsigned char pal[256 * 3], trns[256];
    unsigned npal = 0, ntrns = 0;
    int got_hdr = 0;
    SmmBuf idat = {0};
    int ok = 0;
    unsigned char *raw = NULL;
    unsigned *out = NULL;
    while (pos + 12 <= n) {
        unsigned len = be32(d + pos);
        const unsigned char *type = d + pos + 4, *body = d + pos + 8;
        if ((unsigned long long)pos + 12 + len > n) break;
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            w = be32(body);
            h = be32(body + 4);
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
            got_hdr = 1;
        } else if (!memcmp(type, "PLTE", 4)) {
            npal = len / 3 > 256 ? 256 : len / 3;
            memcpy(pal, body, npal * 3);
        } else if (!memcmp(type, "tRNS", 4)) {
            ntrns = len > 256 ? 256 : len;
            memcpy(trns, body, ntrns);
        } else if (!memcmp(type, "IDAT", 4)) {
            if (idat.n + len > ICON_MAX) goto done;
            smm_buf_append(&idat, body, len);
        } else if (!memcmp(type, "IEND", 4)) break;
        pos += 12 + (size_t)len;
    }
    if (!got_hdr || w == 0 || h == 0 || w > 1024 || h > 1024 || depth != 8 || interlace != 0 || idat.n < 3) goto done;
    unsigned chans;
    if (ctype == 6) chans = 4;
    else if (ctype == 2) chans = 3;
    else if (ctype == 0 || ctype == 3) chans = 1;
    else if (ctype == 4) chans = 2;
    else goto done;
    size_t stride = (size_t)w * chans;
    size_t rawn = (stride + 1) * h;
    raw = malloc(rawn);
    if (!raw) goto done;
    if (smm_inflate((unsigned char *)idat.p + 2, idat.n - 2, raw, rawn) != 0) goto done;
    out = malloc((size_t)w * h * 4);
    if (!out) goto done;
    for (unsigned y = 0; y < h; y++) {
        unsigned char *row = raw + y * (stride + 1) + 1;
        const unsigned char *prev = y ? row - (stride + 1) : NULL;
        unsigned f = row[-1];
        if (f > 4) goto done;
        for (size_t i = 0; i < stride; i++) {
            int a = i >= chans ? row[i - chans] : 0, b = prev ? prev[i] : 0, c = prev && i >= chans ? prev[i - chans] : 0;
            int add = f == 0 ? 0 : f == 1 ? a : f == 2 ? b : f == 3 ? (a + b) / 2 : paeth(a, b, c);
            row[i] = (unsigned char)(row[i] + add);
        }
        for (unsigned x = 0; x < w; x++) {
            const unsigned char *s = row + x * chans;
            unsigned r, g, b, a = 255;
            if (ctype == 6) r = s[0], g = s[1], b = s[2], a = s[3];
            else if (ctype == 2) r = s[0], g = s[1], b = s[2];
            else if (ctype == 0) r = g = b = s[0];
            else if (ctype == 4) r = g = b = s[0], a = s[1];
            else {
                unsigned i = s[0];
                if (i >= npal) i = 0;
                r = pal[i * 3];
                g = pal[i * 3 + 1];
                b = pal[i * 3 + 2];
                if (i < ntrns) a = trns[i];
            }
            out[y * w + x] = premul(a, r, g, b);
        }
    }
    *argb = out;
    out = NULL;
    *ow = (int)w;
    *oh = (int)h;
    ok = 1;
done:
    free(out);
    free(raw);
    smm_buf_free(&idat);
    return ok;
}

int smm_pe_icon(const char *exe_path, int want_size, unsigned **argb, int *w, int *h)
{
    if (!exe_path || !argb || !w || !h) return 0;
    if (want_size <= 0) want_size = 64;
    FILE *f = smm_os_fopen(exe_path, "rb");
    if (!f) return 0;
    Rsrc rs = {0};
    int ok = 0;
    if (!load_rsrc(f, &rs)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    const unsigned char *grp;
    unsigned gsize;
    if (!find_resource(&rs, RT_GROUP_ICON, -1, 0, &grp, &gsize, NULL) || gsize < 6) goto done;
    unsigned count = rd16(grp + 4);
    if (rd16(grp + 2) != 1 || count == 0 || count > 64 || 6 + (size_t)count * 14 > gsize) goto done;
    Cand cands[64];
    for (unsigned i = 0; i < count; i++) {
        const unsigned char *e = grp + 6 + i * 14;
        cands[i].size = e[0] ? e[0] : 256;
        cands[i].bits = rd16(e + 6);
        cands[i].id = (int)rd16(e + 12);
        cands[i].tried = 0;
    }
    for (unsigned tries = 0; tries < count && !ok; tries++) {
        int k = pick(cands, (int)count, want_size);
        if (k < 0) break;
        cands[k].tried = 1;
        const unsigned char *img;
        unsigned isize;
        if (!find_resource(&rs, RT_ICON, cands[k].id, 0, &img, &isize, NULL)) continue;
        if (is_png(img, isize)) ok = decode_png(img, isize, argb, w, h);
        else ok = decode_dib(img, isize, argb, w, h);
    }
done:
    free(rs.p);
    return ok;
}
