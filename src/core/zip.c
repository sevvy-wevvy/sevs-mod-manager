#include "internal.h"
#include <stdlib.h>

static unsigned crc_table[256];
static int crc_ready;

unsigned smm_crc32(unsigned crc, const void *data, size_t n)
{
    if (!crc_ready) {
        for (unsigned i = 0; i < 256; i++) {
            unsigned c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
        crc_ready = 1;
    }
    const unsigned char *p = data;
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

typedef struct {
    const unsigned char *in;
    size_t inlen, inpos;
    unsigned bitbuf;
    int bitcnt;
    unsigned char *out;
    size_t outlen, outpos;
    int err;
} Inf;

typedef struct {
    short count[16];
    short symbol[288];
} Huff;

static int get_bits(Inf *s, int need)
{
    long val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) {
            s->err = 1;
            return 0;
        }
        val |= (long)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = (unsigned)(val >> need);
    s->bitcnt -= need;
    return (int)(val & ((1L << need) - 1));
}

static int decode(Inf *s, const Huff *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        code |= get_bits(s, 1);
        if (s->err) return -1;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static int construct(Huff *h, const short *length, int n)
{
    short offs[16];
    for (int i = 0; i <= 15; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[length[i]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return left;
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (length[i] != 0) h->symbol[offs[length[i]]++] = (short)i;
    return left;
}

static const short lbase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short lext[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short dbase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const short dext[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(Inf *s, const Huff *lencode, const Huff *distcode)
{
    for (;;) {
        int symbol = decode(s, lencode);
        if (symbol < 0) return -1;
        if (symbol < 256) {
            if (s->outpos >= s->outlen) return -1;
            s->out[s->outpos++] = (unsigned char)symbol;
        } else if (symbol == 256) {
            return 0;
        } else {
            symbol -= 257;
            if (symbol >= 29) return -1;
            int len = lbase[symbol] + get_bits(s, lext[symbol]);
            int ds = decode(s, distcode);
            if (ds < 0 || ds >= 30) return -1;
            size_t dist = (size_t)dbase[ds] + (size_t)get_bits(s, dext[ds]);
            if (s->err || dist > s->outpos || s->outpos + (size_t)len > s->outlen) return -1;
            while (len--) {
                s->out[s->outpos] = s->out[s->outpos - dist];
                s->outpos++;
            }
        }
    }
}

static int fixed_block(Inf *s)
{
    static Huff lencode, distcode;
    static int ready;
    if (!ready) {
        short lengths[288];
        int i;
        for (i = 0; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        construct(&lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        construct(&distcode, lengths, 30);
        ready = 1;
    }
    return codes(s, &lencode, &distcode);
}

static int dynamic_block(Inf *s)
{
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320];
    Huff lencode, distcode;
    int nlen = get_bits(s, 5) + 257, ndist = get_bits(s, 5) + 1, ncode = get_bits(s, 4) + 4;
    if (s->err || nlen > 286 || ndist > 30) return -1;
    int idx;
    for (idx = 0; idx < ncode; idx++) lengths[order[idx]] = (short)get_bits(s, 3);
    for (; idx < 19; idx++) lengths[order[idx]] = 0;
    if (construct(&lencode, lengths, 19) != 0) return -1;
    idx = 0;
    while (idx < nlen + ndist) {
        int symbol = decode(s, &lencode);
        if (symbol < 0) return -1;
        if (symbol < 16) {
            lengths[idx++] = (short)symbol;
        } else {
            int len = 0, rep;
            if (symbol == 16) {
                if (idx == 0) return -1;
                len = lengths[idx - 1];
                rep = 3 + get_bits(s, 2);
            } else if (symbol == 17) {
                rep = 3 + get_bits(s, 3);
            } else {
                rep = 11 + get_bits(s, 7);
            }
            if (idx + rep > nlen + ndist) return -1;
            while (rep--) lengths[idx++] = (short)len;
        }
    }
    if (lengths[256] == 0) return -1;
    int err = construct(&lencode, lengths, nlen);
    if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) return -1;
    err = construct(&distcode, lengths + nlen, ndist);
    if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) return -1;
    return codes(s, &lencode, &distcode);
}

int smm_inflate(const unsigned char *src, size_t n, unsigned char *dst, size_t dn)
{
    Inf s = {src, n, 0, 0, 0, dst, dn, 0, 0};
    int last;
    do {
        last = get_bits(&s, 1);
        int type = get_bits(&s, 2);
        int rc;
        if (s.err) return -1;
        if (type == 0) {
            s.bitbuf = 0;
            s.bitcnt = 0;
            if (s.inpos + 4 > s.inlen) return -1;
            unsigned len = s.in[s.inpos] | (unsigned)s.in[s.inpos + 1] << 8;
            unsigned nlen = s.in[s.inpos + 2] | (unsigned)s.in[s.inpos + 3] << 8;
            s.inpos += 4;
            if (len != (~nlen & 0xFFFF)) return -1;
            if (s.inpos + len > s.inlen || s.outpos + len > s.outlen) return -1;
            memcpy(s.out + s.outpos, s.in + s.inpos, len);
            s.inpos += len;
            s.outpos += len;
            rc = 0;
        } else if (type == 1) {
            rc = fixed_block(&s);
        } else if (type == 2) {
            rc = dynamic_block(&s);
        } else {
            return -1;
        }
        if (rc != 0) return -1;
    } while (!last);
    return (int)s.outpos == (int)dn ? 0 : -1;
}

static unsigned rd16(const unsigned char *p)
{
    return p[0] | (unsigned)p[1] << 8;
}

static unsigned rd32(const unsigned char *p)
{
    return rd16(p) | rd16(p + 2) << 16;
}

static int read_at(const SmmZip *z, long long off, void *dst, size_t n)
{
    if (z->data) {
        if (off < 0 || (size_t)off + n > z->len) return -1;
        memcpy(dst, z->data + off, n);
        return 0;
    }
    if (fseek(z->f, (long)off, SEEK_SET) != 0) return -1;
    return fread(dst, 1, n, z->f) == n ? 0 : -1;
}

static int parse_central(SmmZip *z, long long total)
{
    size_t scan = total < 66000 ? (size_t)total : 66000;
    unsigned char *tail = smm_xmalloc(scan);
    if (read_at(z, total - (long long)scan, tail, scan) != 0) {
        free(tail);
        return -1;
    }
    long eocd = -1;
    for (long i = (long)scan - 22; i >= 0; i--)
        if (rd32(tail + i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    if (eocd < 0) {
        free(tail);
        return -1;
    }
    unsigned count = rd16(tail + eocd + 10);
    unsigned cdsize = rd32(tail + eocd + 12), cdoff = rd32(tail + eocd + 16);
    free(tail);
    if ((long long)cdoff + cdsize > total) return -1;
    unsigned char *cd = smm_xmalloc(cdsize ? cdsize : 1);
    if (read_at(z, cdoff, cd, cdsize) != 0) {
        free(cd);
        return -1;
    }
    z->e = smm_xmalloc((size_t)(count ? count : 1) * sizeof(SmmZipEntry));
    size_t p = 0;
    for (unsigned i = 0; i < count; i++) {
        if (p + 46 > cdsize || rd32(cd + p) != 0x02014b50u) break;
        SmmZipEntry *e = &z->e[z->n];
        unsigned nl = rd16(cd + p + 28), el = rd16(cd + p + 30), cl = rd16(cd + p + 32);
        if (p + 46 + nl > cdsize) break;
        e->method = rd16(cd + p + 10);
        e->crc = rd32(cd + p + 16);
        e->csize = rd32(cd + p + 20);
        e->size = rd32(cd + p + 24);
        e->local_off = rd32(cd + p + 42);
        if (e->csize == 0xFFFFFFFFu || e->size == 0xFFFFFFFFu || e->local_off == 0xFFFFFFFFll) {
            free(cd);
            return -1;
        }
        size_t cp = nl < sizeof e->name - 1 ? nl : sizeof e->name - 1;
        memcpy(e->name, cd + p + 46, cp);
        e->name[cp] = 0;
        for (char *c = e->name; *c; c++)
            if (*c == '\\') *c = '/';
        z->n++;
        p += 46 + nl + el + cl;
    }
    free(cd);
    return 0;
}

int smm_zip_open_mem(SmmZip *z, unsigned char *data, size_t len)
{
    memset(z, 0, sizeof *z);
    z->data = data;
    z->len = len;
    if (parse_central(z, (long long)len) != 0) {
        smm_zip_close(z);
        smm_set_error("Not a valid zip file");
        return -1;
    }
    return 0;
}

int smm_zip_open_file(SmmZip *z, const char *path)
{
    memset(z, 0, sizeof *z);
    z->f = smm_os_fopen(path, "rb");
    if (!z->f) {
        smm_set_error("Couldn't open %s", path);
        return -1;
    }
    fseek(z->f, 0, SEEK_END);
    long total = ftell(z->f);
    if (total < 22 || parse_central(z, total) != 0) {
        smm_zip_close(z);
        smm_set_error("%s is not a valid zip file", smm_basename(path));
        return -1;
    }
    return 0;
}

void smm_zip_close(SmmZip *z)
{
    if (z->f) fclose(z->f);
    free(z->e);
    memset(z, 0, sizeof *z);
}

int smm_zip_find(const SmmZip *z, const char *name)
{
    for (int i = 0; i < z->n; i++)
        if (!strcmp(z->e[i].name, name)) return i;
    return -1;
}

int smm_zip_extract_mem(const SmmZip *z, int i, SmmBuf *out)
{
    const SmmZipEntry *e = &z->e[i];
    unsigned char lh[30];
    if (e->size > (1u << 30) || e->csize > (1u << 30)) return -1;
    if (read_at(z, e->local_off, lh, 30) != 0 || rd32(lh) != 0x04034b50u) return -1;
    long long data_off = e->local_off + 30 + rd16(lh + 26) + rd16(lh + 28);
    unsigned char *comp = smm_xmalloc(e->csize ? e->csize : 1);
    if (read_at(z, data_off, comp, e->csize) != 0) {
        free(comp);
        return -1;
    }
    unsigned char *raw = smm_xmalloc((size_t)e->size + 1);
    int rc = -1;
    if (e->method == 0 && e->csize == e->size) {
        memcpy(raw, comp, e->csize);
        rc = 0;
    } else if (e->method == 8) {
        rc = smm_inflate(comp, e->csize, raw, (size_t)e->size);
    }
    free(comp);
    if (rc == 0 && smm_crc32(0, raw, (size_t)e->size) != e->crc) rc = -1;
    if (rc == 0) {
        smm_buf_append(out, raw, (size_t)e->size);
        smm_bufs_cstr(out);
    }
    free(raw);
    return rc;
}

int smm_zip_extract_file(const SmmZip *z, int i, const char *dest)
{
    SmmBuf b = {0};
    if (smm_zip_extract_mem(z, i, &b) != 0) {
        smm_buf_free(&b);
        smm_set_error("Couldn't unpack %s", z->e[i].name);
        return -1;
    }
    char dir[SMM_PATH];
    smm_dirname(dest, dir, sizeof dir);
    if (dir[0]) smm_os_mkdir_p(dir);
    int rc = smm_file_write(dest, b.p, b.n);
    smm_buf_free(&b);
    return rc;
}

static int safe_name(const char *name)
{
    if (!*name || name[0] == '/' || name[0] == '\\') return 0;
    if (strchr(name, ':')) return 0;
    for (const char *p = name; *p; p++)
        if ((p == name || p[-1] == '/' || p[-1] == '\\') && p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\\' || !p[2])) return 0;
    return 1;
}

int smm_zip_extract_all(const SmmZip *z, const char *dest_dir)
{
    smm_os_mkdir_p(dest_dir);
    for (int i = 0; i < z->n; i++) {
        const char *name = z->e[i].name;
        if (!safe_name(name)) continue;
        size_t l = strlen(name);
        char full[SMM_PATH];
        smm_join(full, sizeof full, dest_dir, name);
        if (name[l - 1] == '/') {
            smm_os_mkdir_p(full);
            continue;
        }
        if (smm_zip_extract_file(z, i, full) != 0) return -1;
    }
    return 0;
}

static void put16(SmmBuf *b, unsigned v)
{
    unsigned char o[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
    smm_buf_append(b, o, 2);
}

static void put32(SmmBuf *b, unsigned v)
{
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}

int smm_zipw_open(SmmZipWriter *w, const char *path)
{
    memset(w, 0, sizeof *w);
    w->f = smm_os_fopen(path, "wb");
    if (!w->f) {
        smm_set_error("Couldn't create %s", path);
        return -1;
    }
    return 0;
}

typedef struct {
    unsigned char *out;
    size_t n, cap;
    unsigned acc;
    int bits;
} BitW;

static void bw_put(BitW *b, unsigned val, int nbits)
{
    b->acc |= val << b->bits;
    b->bits += nbits;
    while (b->bits >= 8) {
        if (b->n == b->cap) {
            b->cap = b->cap ? b->cap * 2 : 4096;
            b->out = smm_xrealloc(b->out, b->cap);
        }
        b->out[b->n++] = (unsigned char)b->acc;
        b->acc >>= 8;
        b->bits -= 8;
    }
}

static unsigned rev(unsigned v, int n)
{
    unsigned r = 0;
    for (int i = 0; i < n; i++) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}

static void put_lit(BitW *b, unsigned sym)
{
    if (sym < 144) bw_put(b, rev(0x30 + sym, 8), 8);
    else if (sym < 256) bw_put(b, rev(0x190 + sym - 144, 9), 9);
    else if (sym < 280) bw_put(b, rev(sym - 256, 7), 7);
    else bw_put(b, rev(0xC0 + sym - 280, 8), 8);
}

static void put_match(BitW *b, int len, int dist)
{
    int li = 28;
    while (lbase[li] > len) li--;
    put_lit(b, 257 + (unsigned)li);
    if (lext[li]) bw_put(b, (unsigned)(len - lbase[li]), lext[li]);
    int di = 29;
    while (dbase[di] > dist) di--;
    bw_put(b, rev((unsigned)di, 5), 5);
    if (dext[di]) bw_put(b, (unsigned)(dist - dbase[di]), dext[di]);
}

#define HASH_BITS 15

static size_t deflate_fixed(const unsigned char *in, size_t n, unsigned char **out)
{
    BitW b = {0};
    int *head = smm_xmalloc(sizeof(int) << HASH_BITS);
    for (int i = 0; i < (1 << HASH_BITS); i++) head[i] = -1;
    int *prev = smm_xmalloc((n ? n : 1) * sizeof(int));
    bw_put(&b, 1, 1);
    bw_put(&b, 1, 2);
    size_t i = 0;
    while (i < n) {
        int best = 0, bestd = 0;
        if (i + 3 <= n) {
            unsigned h = ((unsigned)in[i] << 10 ^ (unsigned)in[i + 1] << 5 ^ in[i + 2]) & ((1u << HASH_BITS) - 1);
            int cand = head[h], tries = 24;
            while (cand >= 0 && i - (size_t)cand <= 32768 && tries--) {
                int l = 0;
                while (l < 258 && i + (size_t)l < n && in[cand + l] == in[i + l]) l++;
                if (l > best) {
                    best = l;
                    bestd = (int)(i - (size_t)cand);
                    if (l == 258) break;
                }
                cand = prev[cand];
            }
            prev[i] = head[h];
            head[h] = (int)i;
        }
        if (best >= 3) {
            put_match(&b, best, bestd);
            for (int k = 1; k < best; k++) {
                size_t j = i + (size_t)k;
                if (j + 3 <= n) {
                    unsigned h = ((unsigned)in[j] << 10 ^ (unsigned)in[j + 1] << 5 ^ in[j + 2]) & ((1u << HASH_BITS) - 1);
                    prev[j] = head[h];
                    head[h] = (int)j;
                }
            }
            i += (size_t)best;
        } else {
            put_lit(&b, in[i]);
            i++;
        }
    }
    put_lit(&b, 256);
    if (b.bits > 0) bw_put(&b, 0, 8 - b.bits);
    free(head);
    free(prev);
    *out = b.out;
    return b.n;
}

int smm_zipw_add_mem(SmmZipWriter *w, const char *name, const void *data, size_t n)
{
    unsigned crc = smm_crc32(0, data, n);
    unsigned char *comp = NULL;
    size_t cn = 0;
    unsigned method = 0;
    const void *payload = data;
    size_t plen = n;
    if (n > 64) {
        cn = deflate_fixed(data, n, &comp);
        if (cn < n) {
            method = 8;
            payload = comp;
            plen = cn;
        }
    }
    size_t nl = strlen(name);
    if (n >= 0xFFFFFFFFu || w->off + (long long)n + 1024 >= 0xFFFFFFFFll || w->n >= 65535 || nl > 65535) {
        free(comp);
        smm_set_error("That is too much to fit in one zip file");
        return -1;
    }
    SmmBuf h = {0};
    put32(&h, 0x04034b50u);
    put16(&h, 20);
    put16(&h, 0x0800);
    put16(&h, method);
    put16(&h, 0);
    put16(&h, 0x21);
    put32(&h, crc);
    put32(&h, (unsigned)plen);
    put32(&h, (unsigned)n);
    put16(&h, (unsigned)nl);
    put16(&h, 0);
    smm_buf_append(&h, name, nl);
    int ok = fwrite(h.p, 1, h.n, w->f) == h.n && (plen == 0 || fwrite(payload, 1, plen, w->f) == plen);
    smm_buf_free(&h);
    SmmBuf *cd = &w->cd;
    put32(cd, 0x02014b50u);
    put16(cd, 20);
    put16(cd, 20);
    put16(cd, 0x0800);
    put16(cd, method);
    put16(cd, 0);
    put16(cd, 0x21);
    put32(cd, crc);
    put32(cd, (unsigned)plen);
    put32(cd, (unsigned)n);
    put16(cd, (unsigned)nl);
    put16(cd, 0);
    put16(cd, 0);
    put16(cd, 0);
    put16(cd, 0);
    put32(cd, 0);
    put32(cd, (unsigned)w->off);
    smm_buf_append(cd, name, nl);
    w->off += 30 + (long long)nl + (long long)plen;
    w->n++;
    free(comp);
    if (!ok) smm_set_error("Couldn't write the zip file");
    return ok ? 0 : -1;
}

int smm_zipw_add_file(SmmZipWriter *w, const char *name, const char *path)
{
    SmmBuf b = {0};
    if (smm_file_read(path, &b) != 0) return -1;
    int rc = smm_zipw_add_mem(w, name, b.p ? b.p : (void *)"", b.n);
    smm_buf_free(&b);
    return rc;
}

int smm_zipw_close(SmmZipWriter *w)
{
    SmmBuf e = {0};
    put32(&e, 0x06054b50u);
    put16(&e, 0);
    put16(&e, 0);
    put16(&e, (unsigned)w->n);
    put16(&e, (unsigned)w->n);
    put32(&e, (unsigned)w->cd.n);
    put32(&e, (unsigned)w->off);
    put16(&e, 0);
    int ok = (w->cd.n == 0 || fwrite(w->cd.p, 1, w->cd.n, w->f) == w->cd.n) && fwrite(e.p, 1, e.n, w->f) == e.n;
    if (fclose(w->f) != 0) ok = 0;
    smm_buf_free(&e);
    smm_buf_free(&w->cd);
    memset(w, 0, sizeof *w);
    if (!ok) smm_set_error("Couldn't finish the zip file");
    return ok ? 0 : -1;
}
