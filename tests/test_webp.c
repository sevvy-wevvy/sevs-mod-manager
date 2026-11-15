#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui/webp.h"

static int g_checks, g_fails;

#define CHECK(c) do { g_checks++; if (!(c)) { g_fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static unsigned char *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)len + 1);
    if (b && fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b);
        b = NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static uint32_t pixel_hash(const uint32_t *px, size_t count)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < count; i++) {
        uint32_t c = px[i];
        if ((c >> 24) == 0) c = 0;
        for (int s = 0; s < 32; s += 8) h = (h ^ ((c >> s) & 255)) * 16777619u;
    }
    return h;
}

static void check_sample(const char *dir, const char *name, int w, int h, uint32_t hash)
{
    char path[512];
    snprintf(path, sizeof path, "%s/webp/%s", dir, name);
    size_t n;
    unsigned char *d = slurp(path, &n);
    CHECK(d != NULL);
    if (!d) return;
    int gw = 0, gh = 0;
    uint32_t *px = smm_webp_decode_argb(d, n, &gw, &gh);
    CHECK(px != NULL);
    if (px) {
        CHECK(gw == w && gh == h);
        uint32_t got = pixel_hash(px, (size_t)gw * gh);
        if (got != hash) printf("%s hash %08x\n", name, got);
        CHECK(got == hash);
        free(px);
    }
    goome_bitmap bm = {0};
    CHECK(smm_webp_decode(d, n, &bm) == GOOME_OK);
    CHECK(bm.w == w && bm.h == h && bm.stride == w && bm.px != NULL);
    if (bm.px) {
        for (int i = 0; i < w * h; i++) {
            uint32_t c = bm.px[i], a = c >> 24;
            CHECK(((c >> 16) & 255) <= a && ((c >> 8) & 255) <= a && (c & 255) <= a);
        }
        free(bm.px);
    }
    for (size_t cut = 0; cut < n; cut++) {
        uint32_t *t = smm_webp_decode_argb(d, cut, &gw, &gh);
        free(t);
    }
    unsigned char *copy = malloc(n);
    for (size_t pos = 0; copy && pos < n; pos++) {
        for (int bit = 0; bit < 8; bit += 3) {
            memcpy(copy, d, n);
            copy[pos] ^= (unsigned char)(1 << bit);
            uint32_t *t = smm_webp_decode_argb(copy, n, &gw, &gh);
            free(t);
        }
    }
    free(copy);
    free(d);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "fixtures";
    check_sample(dir, "lossy_plain.webp", 40, 30, 0x018a0b92u);
    check_sample(dir, "lossy_alpha.webp", 31, 23, 0xfea32afau);
    check_sample(dir, "lossless_alpha.webp", 32, 24, 0x63368adcu);
    CHECK(!smm_webp_check("RIFF", 4));
    CHECK(!smm_webp_check("RIFF\0\0\0\0WAVEfmt ", 16));
    CHECK(smm_webp_check("RIFF\0\0\0\0WEBPVP8 ", 16));
    int w, h;
    CHECK(smm_webp_decode_argb("RIFF\x04\0\0\0WEBPVP8 \0\0\0\0", 20, &w, &h) == NULL);
    printf("webp: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
