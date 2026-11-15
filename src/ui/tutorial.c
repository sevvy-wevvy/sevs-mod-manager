#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#define STEPS 11

static const char *const TITLE[STEPS] = {
    "Hey, welcome!", "Getting around", "The Play button", "What a mod loader does", "Try it: click Mods", "Installing a mod",
    "Try it: click Installed", "Enable and disable", "Removing a mod", "One more thing", "You're all set"};

static const char *const BODY[STEPS] = {
    "Quick tour of the app, it takes a minute.",
    "These tabs are your main navigation. Mods for browsing and installing, Installed for managing what is on your system, Modpacks for saving and sharing setups, Config for tweaking settings, Logs for watching your game, and Settings for everything else.",
    "This launches your game. Once you have some mods set up, click here anytime to play.",
    "Mods cannot run on their own. They need a mod loader, BepInEx or MelonLoader, installed first. Sev's Mod Manager figures out which one your game needs and installs it automatically the first time you install a mod.",
    "Go ahead and click the Mods tab above.",
    "This is the real Mods screen with a sample entry dropped in. Click Install and watch the progress bar, that is exactly what a real install looks like. Nothing actually downloads here.",
    "Now click the Installed tab above.",
    "You can turn a mod off without removing it. This is handy for figuring out which mod is causing a problem. Click the toggle next to Example Mod to try it.",
    "When you are done with a mod, click the trash icon to remove it completely. Try it on Example Mod.",
    "Settings has your themes, loader options, and this tutorial again if you ever want to redo it.",
    "That is the whole loop. Explore and have fun modding."};

static int active;
static int step;
static int mods_demo, inst_demo;

int tutorial_active(void)
{
    return active;
}

int tutorial_mode(void)
{
    return active;
}

void tutorial_start(void)
{
    active = 1;
    step = 0;
    mods_demo = inst_demo = 0;
}

static void cleanup(void)
{
    if (mods_demo) mods_tutorial_show(0);
    if (inst_demo) installed_tutorial(0);
    mods_demo = inst_demo = 0;
    active = 0;
    K.tut = 0;
}

void tutorial_stop(void)
{
    cleanup();
}

void tutorial_set_step(int s)
{
    step = s;
}

static void finish(void)
{
    cleanup();
    smm_settings.has_seen_tutorial = 1;
    smm_state_save();
}

static int can_advance(int s)
{
    return !(s >= 4 && s <= 8);
}

static R inflate(R r, double d)
{
    return rect(r.x - d, r.y - d, r.w + d * 2, r.h + d * 2);
}

static int get_hole(int s, R *out)
{
    switch (s) {
    case 1: {
        R a = tab_rect(0), b = tab_rect(TAB_COUNT - 1);
        *out = inflate(rect(a.x, a.y, b.x + b.w - a.x, a.h), 6);
        return 1;
    }
    case 2: *out = inflate(play_rect(), 6); return 1;
    case 4: *out = inflate(tab_rect(0), 6); return 1;
    case 5: *out = inflate(mods_install_rect(), 6); return 1;
    case 6: *out = inflate(tab_rect(1), 6); return 1;
    case 7:
    case 8: *out = inflate(installed_list_rect(), 4); return 1;
    case 9: *out = inflate(tab_rect(TAB_COUNT - 1), 6); return 1;
    default: return 0;
    }
}

void tutorial_pre(void)
{
    if (!active) {
        K.tut = 0;
        return;
    }
    int want_mods = step == 5, want_inst = step == 7 || step == 8;
    if (want_mods && !mods_demo) {
        mods_tutorial_show(1);
        mods_demo = 1;
    } else if (!want_mods && mods_demo) {
        mods_tutorial_show(0);
        mods_demo = 0;
    }
    if (want_inst && !inst_demo) {
        installed_tutorial(1);
        inst_demo = 1;
    } else if (!want_inst && inst_demo) {
        installed_tutorial(0);
        inst_demo = 0;
    }
    if (step == 4 && app_tab() == TAB_MODS) step++;
    else if (step == 5 && mods_tutorial_installed()) step++;
    else if (step == 6 && app_tab() == TAB_INSTALLED) step++;
    else if (step == 7 && !installed_tutorial_enabled()) step++;
    else if (step == 8 && installed_tutorial_deleted()) step++;
    R h;
    K.tut = 1;
    K.hole = get_hole(step, &h) ? h : rect(-10, -10, 0, 0);
    if (step == 5 && !mods_demo) K.hole = rect(-10, -10, 0, 0);
}

void tutorial_draw(void)
{
    if (!active) return;
    R h;
    int has = get_hole(step, &h);
    R full = rect(0, 0, K.w, K.h);
    uint32_t mask = 0xFF0E0E12;
    if (!has) fill(full, 0, mask);
    else {
        if (h.y > 0) fill(rect(0, 0, K.w, h.y), 0, mask);
        if (h.y + h.h < K.h) fill(rect(0, h.y + h.h, K.w, K.h - h.y - h.h), 0, mask);
        if (h.x > 0) fill(rect(0, h.y, h.x, h.h), 0, mask);
        if (h.x + h.w < K.w) fill(rect(h.x + h.w, h.y, K.w - h.x - h.w, h.h), 0, mask);
    }
    K.tut = 0;
    int saved = K.blocked;
    K.blocked = 0;
    double w = 380, hh = 170, x, y;
    if (has) {
        x = h.x + (h.w - w) / 2;
        if (x < 12) x = 12;
        if (x > K.w - w - 12) x = K.w - w - 12;
        y = h.y + h.h + 14 + hh <= K.h ? h.y + h.h + 14 : (h.y - 14 - hh > 12 ? h.y - 14 - hh : 12);
    } else {
        x = (K.w - w) / 2;
        y = (K.h - hh) / 2;
    }
    R p = rect(x, y, w, hh);
    shadow(p, 12, 20, 0x88000000);
    fill(p, 12, P.surface);
    tx(p.x + 18, p.y + 14, TITLE[step], 15.3, P.text, 1);
    tx_wrap(rect(p.x + 18, p.y + 44, w - 36, 76), BODY[step], 11.2, P.sub, 0, NULL);
    double by = p.y + hh - 42;
    if (w_button(rect(p.x + 18, by, 80, 30), "Skip", BTN_GHOST, 1)) finish();
    if (step > 0 && w_button(rect(p.x + w - 180, by, 80, 30), "Back", BTN_GHOST, 1)) {
        step--;
        if (step == 4) app_goto_tab(TAB_MODS);
        if (step == 6) app_goto_tab(TAB_MODS);
    }
    if (w_button(rect(p.x + w - 92, by, 80, 30), step == STEPS - 1 ? "Finish" : "Next", BTN_ACCENT, can_advance(step))) {
        if (step >= STEPS - 1) finish();
        else step++;
    }
    K.blocked = saved;
}
