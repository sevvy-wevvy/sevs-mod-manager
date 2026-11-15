#include "internal.h"
#include <stdlib.h>

static SmmTheme g_theme;
static int g_ready;

static SmmRgb rgb(int r, int g, int b)
{
    SmmRgb c = {(unsigned char)r, (unsigned char)g, (unsigned char)b};
    return c;
}

static SmmTheme black(void)
{
    SmmTheme t;
    t.background = rgb(20, 20, 20);
    t.surface = rgb(30, 30, 30);
    t.surface_alt = rgb(62, 62, 62);
    t.border = rgb(80, 80, 80);
    t.text = rgb(232, 230, 239);
    t.sub_text = rgb(160, 160, 160);
    t.accent = rgb(124, 58, 237);
    t.accent_text = rgb(255, 255, 255);
    return t;
}

static SmmTheme white(void)
{
    SmmTheme t;
    t.background = rgb(248, 248, 248);
    t.surface = rgb(255, 255, 255);
    t.surface_alt = rgb(238, 238, 238);
    t.border = rgb(210, 210, 210);
    t.text = rgb(20, 20, 20);
    t.sub_text = rgb(100, 100, 100);
    t.accent = rgb(124, 58, 237);
    t.accent_text = rgb(255, 255, 255);
    return t;
}

static SmmTheme r2modman(void)
{
    SmmTheme t;
    t.background = rgb(27, 29, 39);
    t.surface = rgb(35, 37, 47);
    t.surface_alt = rgb(46, 48, 61);
    t.border = rgb(58, 61, 77);
    t.text = rgb(228, 228, 232);
    t.sub_text = rgb(154, 156, 173);
    t.accent = rgb(88, 101, 242);
    t.accent_text = rgb(255, 255, 255);
    return t;
}

static int clamp(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

SmmRgb smm_rgb_lighten(SmmRgb c, int a)
{
    return rgb(clamp(c.r + a), clamp(c.g + a), clamp(c.b + a));
}

SmmRgb smm_rgb_darken(SmmRgb c, int a)
{
    return rgb(clamp(c.r - a), clamp(c.g - a), clamp(c.b - a));
}

int smm_rgb_parse_hex(const char *hex, SmmRgb *out)
{
    if (!hex || !*hex) return 0;
    if (*hex == '#') hex++;
    if (strlen(hex) != 6) return 0;
    for (int i = 0; i < 6; i++)
        if (!((hex[i] >= '0' && hex[i] <= '9') || (hex[i] >= 'a' && hex[i] <= 'f') || (hex[i] >= 'A' && hex[i] <= 'F'))) return 0;
    unsigned v = (unsigned)strtoul(hex, NULL, 16);
    *out = rgb((int)(v >> 16) & 255, (int)(v >> 8) & 255, (int)v & 255);
    return 1;
}

void smm_rgb_to_hex(SmmRgb c, char *out)
{
    smm_fmt(out, 8, "#%02X%02X%02X", c.r, c.g, c.b);
}

unsigned smm_rgb_pack(SmmRgb c)
{
    return (unsigned)c.r << 16 | (unsigned)c.g << 8 | c.b;
}

int smm_rgb_is_dark(SmmRgb c)
{
    return c.r + c.g + c.b < 384;
}

SmmRgb smm_rgb_highlight(const SmmTheme *t)
{
    int amt = smm_rgb_is_dark(t->background) ? 22 : -22;
    return rgb(clamp(t->surface_alt.r + amt), clamp(t->surface_alt.g + amt), clamp(t->surface_alt.b + amt));
}

void smm_theme_apply(int mode, const char *custom_bg, const char *custom_accent, const char *sb_accent_hex, SmmTheme *out)
{
    SmmTheme t;
    SmmRgb c;
    if (mode == SMM_THEME_WHITE) t = white();
    else if (mode == SMM_THEME_R2MODMAN) t = r2modman();
    else if (mode == SMM_THEME_CUSTOM) {
        t = black();
        if (smm_rgb_parse_hex(custom_bg, &c)) {
            t.background = c;
            t.surface = smm_rgb_lighten(c, 10);
            t.surface_alt = smm_rgb_lighten(c, 20);
            t.border = smm_rgb_lighten(c, 35);
        }
        if (smm_rgb_parse_hex(custom_accent, &c)) t.accent = c;
    } else if (mode == SMM_THEME_SYNCED_SB) {
        t = black();
        if (smm_rgb_parse_hex(sb_accent_hex, &c)) t.accent = c;
    } else t = black();
    g_theme = t;
    g_ready = 1;
    if (out) *out = t;
}

void smm_theme_current(SmmTheme *out)
{
    if (!g_ready) smm_theme_apply(SMM_THEME_BLACK, NULL, NULL, NULL, NULL);
    *out = g_theme;
}
