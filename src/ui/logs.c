#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static SmmStrs live;
static SmmStrs archived;
static int viewing_live = 1;
static int live_clear, arch_clear;
static SmmLogSessions sessions;
static char **labels;
static int n_labels;
static int sess_sel;
static char search[200];
static int sev_sel;
static Scroll scroll;
static int stick = 1;
static int was_running;
static char log_path[SMM_PATH];
static long long offset;
static long long started_at;
static long last_poll;
static int culprit_on;
static SmmCrashSuspect culprit;
static int culprit_dismissed;
static int inited;
static const char *const SEV[4] = {"All lines", "Errors only", "Warnings only", "Errors + Warnings"};

static int has_ci(const char *s, const char *needle)
{
    size_t n = strlen(needle);
    for (; *s; s++)
        if (strncasecmp(s, needle, n) == 0) return 1;
    return 0;
}

static int is_error(const char *l)
{
    return has_ci(l, "Exception") || has_ci(l, "Error") || has_ci(l, "Fatal");
}

static int is_warn(const char *l)
{
    return has_ci(l, "Warn");
}

static int passes(const char *l)
{
    if (search[0] && !has_ci(l, search)) return 0;
    switch (sev_sel) {
    case 1: return is_error(l);
    case 2: return is_warn(l);
    case 3: return is_error(l) || is_warn(l);
    default: return 1;
    }
}

static SmmStrs *active(void)
{
    return viewing_live ? &live : &archived;
}

static void update_culprit(void)
{
    culprit_dismissed = 0;
    memset(&culprit, 0, sizeof(culprit));
    culprit_on = smm_crash_analyze(active(), &culprit) > 0;
}

static void refresh_sessions(int keep)
{
    char keep_text[80] = "";
    if (keep && sess_sel > 0 && sess_sel < n_labels) snprintf(keep_text, sizeof(keep_text), "%s", labels[sess_sel]);
    smm_log_sessions_free(&sessions);
    memset(&sessions, 0, sizeof(sessions));
    smm_log_history_list(&sessions);
    for (int i = 0; i < n_labels; i++) free(labels[i]);
    free(labels);
    n_labels = sessions.n + 1;
    labels = calloc((size_t)n_labels, sizeof(char *));
    labels[0] = strdup("Live");
    for (int i = 0; i < sessions.n; i++) labels[i + 1] = strdup(sessions.v[i].display_name);
    sess_sel = 0;
    for (int i = 1; i < n_labels; i++)
        if (keep_text[0] && strcmp(labels[i], keep_text) == 0) sess_sel = i;
}

static void view_session(int idx)
{
    sess_sel = idx;
    scroll.off = 0;
    stick = 1;
    if (idx <= 0) {
        viewing_live = 1;
        update_culprit();
        return;
    }
    viewing_live = 0;
    arch_clear = 0;
    smm_strs_free(&archived);
    memset(&archived, 0, sizeof(archived));
    if (idx - 1 < sessions.n) smm_log_read_lines(sessions.v[idx - 1].path, &archived);
    if (archived.n == 0) smm_strs_add(&archived, "(couldn't read this session's log file)");
    update_culprit();
}

void logs_select_session(int idx)
{
    if (!inited) logs_activate();
    if (idx >= 0 && idx < n_labels) view_session(idx);
}

void logs_activate(void)
{
    if (!inited) {
        inited = 1;
        refresh_sessions(0);
    }
}

static void add_line(const char *l)
{
    smm_strs_add(&live, l);
}

void logs_game_state(int running)
{
    if (running && !was_running) {
        log_path[0] = 0;
        smm_log_path(log_path, sizeof(log_path));
        started_at = smm_now();
        smm_strs_free(&live);
        memset(&live, 0, sizeof(live));
        live_clear = 0;
        offset = 0;
        culprit_on = 0;
        if (sess_sel != 0) view_session(0);
        char t[200];
        snprintf(t, sizeof(t), "%s is running, watching its log file.", smm_settings.game_name);
        add_line(t);
        snprintf(t, sizeof(t), log_path[0] ? "Log path: %s" : "Couldn't figure out where this game's log file should be.", log_path);
        add_line(t);
    } else if (!running && was_running) {
        add_line("Game closed.");
        if (live.n > 2) smm_log_history_archive(started_at, &live);
        refresh_sessions(1);
        if (viewing_live) update_culprit();
    }
    was_running = running;
}

void logs_tick(void)
{
    if (!was_running || !log_path[0]) return;
    long now = (long)goome_now_ms();
    if (now - last_poll < 1000) return;
    last_poll = now;
    int before = live.n;
    smm_log_read_from(log_path, (long long *)&offset, &live);
    if (live.n != before && viewing_live) update_culprit();
}

static void export_bug(void)
{
    char dest[SMM_PATH], def[120];
    snprintf(def, sizeof(def), "SevsModManager-BugReport.zip");
    if (file_pick_save("Export Bug Report", def, "zip", dest, sizeof(dest)) <= 0) return;
    if (smm_bug_report_export(dest, active()) >= 0) {
        char t[SMM_PATH + 40];
        snprintf(t, sizeof(t), "Bug report saved to:\n%s", dest);
        dlg_message(TAG_BUG_SAVED, "Bug Report", t, DB_OK, DI_INFO);
    } else {
        char t[400];
        snprintf(t, sizeof(t), "Failed to export bug report: %s", smm_error());
        dlg_message(TAG_BUG_FAIL, "Bug Report", t, DB_OK, DI_ERROR);
    }
}

static int line_rows(const char *l, int cols)
{
    int n = 0;
    for (const char *p = l; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n <= cols || cols < 1 ? 1 : (n + cols - 1) / cols;
}

void panel_logs(R r)
{
    if (!inited) logs_activate();
    dlg_result(TAG_BUG_SAVED);
    dlg_result(TAG_BUG_FAIL);
    R tool = cut_bottom(&r, 54);
    fill(tool, 0, P.surface);
    double x = tool.x + 8, y = tool.y + 8;
    goome_radii rd = goome_radius(8);
    uint32_t keep = P.alt;
    dd_fill = 1;
    if (w_dropdown("sessdd", rect(x, y, 180, 30), (const char *const *)labels, n_labels, &sess_sel, rd)) view_session(sess_sel);
    x += 188;
    w_input("logsearch", rect(x, y, 200, 30), search, sizeof(search), "Search log...", 0, rd);
    x += 208;
    if (w_dropdown("sevdd", rect(x, y, 150, 30), SEV, 4, &sev_sel, rd)) scroll.off = 0;
    dd_fill = 0;
    x += 158;
    (void)keep;
    double bw = tx_w("Export Bug Report", 13, 0) + 24;
    if (w_button(rect(x, y, bw, 30), "Export Bug Report", BTN_GHOST, 1)) export_bug();
    x += bw + 8;
    if (w_button(rect(x, y, tx_w("Clear", 13, 0) + 24, 30), "Clear", BTN_GHOST, 1)) {
        if (viewing_live) live_clear = live.n;
        else arch_clear = archived.n;
    }

    if (culprit_on && !culprit_dismissed) {
        R ban = cut_top(&r, 34);
        fill(ban, 0, P.alt);
        R dis = rect(ban.x + ban.w - 36, ban.y + 2, 30, 30);
        if (w_button(dis, "", BTN_GHOST, 1)) culprit_dismissed = 1;
        ic_cross(rect(dis.x + 10, dis.y + 10, 10, 10), P.text);
        ic_warn(rect(ban.x + 10, ban.y + 11, 14, 12), col_rgb(255, 159, 10));
        char t[700];
        snprintf(t, sizeof(t), "Possible culprit: %s. %s", culprit.mod_name, culprit.reason);
        tx_fit(rect(ban.x + 32, ban.y, ban.w - 76, ban.h), t, 12.7, col_rgb(255, 159, 10), 1, AL_LEFT);
    }

    fill(r, 0, P.bg);
    SmmStrs *s = active();
    int clear = viewing_live ? live_clear : arch_clear;
    R view = inset(r, 8);
    double cw = tx_w("MMMMMMMMMM", 12, 2) / 10.0;
    int cols = (int)((view.w - 14) / (cw > 1 ? cw : 7));
    double lh = tx_h(12) + 1;
    int total_rows = 0;
    int shown = 0;
    for (int i = clear; i < s->n; i++)
        if (passes(s->v[i])) total_rows += line_rows(s->v[i], cols), shown++;
    double content = total_rows * lh;
    if (shown == 0 && viewing_live && !was_running && s->n == 0) {
        char t[300];
        snprintf(t, sizeof(t), "No game running right now, so there's nothing to show. Launch %s and this tab will start following its log live.", smm_settings.game_name);
        tx_wrap(rect(view.x, view.y, view.w, 60), t, 12, P.text, 2, NULL);
        return;
    }
    scroll.max = content > view.h ? content - view.h : 0;
    if (K.wheel && ui_hover(view)) stick = 0;
    if (stick && viewing_live) scroll.off = scroll.max;
    sc_begin(&scroll, view, content);
    if (scroll.off >= scroll.max - 1 && viewing_live) stick = 1;
    double yy = view.y - scroll.off;
    for (int i = clear; i < s->n; i++) {
        const char *l = s->v[i];
        if (!passes(l)) continue;
        int rows = line_rows(l, cols);
        if (yy + rows * lh >= view.y && yy <= view.y + view.h) {
            uint32_t c = is_error(l) ? col_rgb(255, 95, 86) : is_warn(l) ? col_rgb(255, 193, 7) : P.text;
            if (rows == 1) tx(view.x, yy, l, 12, c, 2);
            else {
                size_t len = strlen(l), pos = 0;
                for (int k = 0; k < rows && pos < len; k++) {
                    char seg[512];
                    int ch = 0;
                    size_t e = pos;
                    while (e < len && ch < cols) {
                        e++;
                        while (e < len && (l[e] & 0xC0) == 0x80) e++;
                        ch++;
                    }
                    snprintf(seg, sizeof(seg), "%.*s", (int)(e - pos), l + pos);
                    tx(view.x, yy + k * lh, seg, 12, c, 2);
                    pos = e;
                }
            }
        }
        yy += rows * lh;
    }
    sc_end(&scroll, view, content);
}
