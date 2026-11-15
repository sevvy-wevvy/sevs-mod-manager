#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "dialogs.h"

typedef struct {
    char path[512];
    char name[160];
    char abs[SMM_PATH];
    int depth;
    int is_dir;
    int checked;
    int open;
} Node;

static Node *nodes;
static int n_nodes, cap_nodes;
static char name_buf[160];
static Scroll scroll;
static int *visible;

static Node *add_node(void)
{
    if (n_nodes >= cap_nodes) {
        cap_nodes = cap_nodes ? cap_nodes * 2 : 256;
        nodes = realloc(nodes, sizeof(Node) * (size_t)cap_nodes);
    }
    Node *n = &nodes[n_nodes++];
    memset(n, 0, sizeof(*n));
    return n;
}

static int is_default(const SmmStrs *defs, const char *top)
{
    return smm_strs_find(defs, top, 1) >= 0;
}

static void build(void)
{
    free(nodes);
    nodes = NULL;
    n_nodes = cap_nodes = 0;
    SmmStrs files;
    memset(&files, 0, sizeof(files));
    smm_pack_files_for_export(&files);
    SmmStrs defs;
    memset(&defs, 0, sizeof(defs));
    smm_pack_default_roots(&defs);
    SmmPackRoot roots[8];
    int nr = smm_pack_roots(roots, 8);
    char game[SMM_PATH] = "";
    smm_game_dir(game, sizeof(game));
    size_t gl = strlen(game);
    for (int i = 0; i < files.n; i++) {
        const char *rel = files.v[i];
        if (strncmp(rel, game, gl) == 0) rel += gl;
        while (*rel == '/' || *rel == '\\') rel++;
        if (nr == 1) {
            const char *sl = strpbrk(rel, "/\\");
            if (sl) rel = sl + 1;
        }
        char acc[512] = "";
        int depth = 0;
        const char *p = rel;
        while (*p) {
            const char *e = strpbrk(p, "/\\");
            size_t len = e ? (size_t)(e - p) : strlen(p);
            char part[160];
            snprintf(part, sizeof(part), "%.*s", (int)len, p);
            size_t al = strlen(acc);
            snprintf(acc + al, sizeof(acc) - al, "%s%s", al ? "/" : "", part);
            int exists = 0;
            if (e) {
                for (int k = n_nodes - 1; k >= 0 && k >= n_nodes - 400; k--)
                    if (nodes[k].is_dir && strcmp(nodes[k].path, acc) == 0) {
                        exists = 1;
                        break;
                    }
            }
            if (!exists) {
                Node *n = add_node();
                snprintf(n->path, sizeof(n->path), "%s", acc);
                snprintf(n->name, sizeof(n->name), "%s", part);
                n->depth = depth;
                n->is_dir = e != NULL;
                char top[160];
                const char *s2 = strchr(acc, '/');
                snprintf(top, sizeof(top), "%.*s", s2 ? (int)(s2 - acc) : (int)strlen(acc), acc);
                n->checked = is_default(&defs, top);
                if (!e) snprintf(n->abs, sizeof(n->abs), "%s", files.v[i]);
            }
            if (!e) break;
            p = e + 1;
            depth++;
        }
    }
    smm_strs_free(&files);
    smm_strs_free(&defs);
    free(visible);
    visible = malloc(sizeof(int) * (size_t)(n_nodes + 1));
}

void dlg_export_open(void)
{
    if (!app_has_game()) {
        dlg_message(TAG_LOADOUT_ERR, "Error", "No game path set.", DB_OK, DI_ERROR);
        return;
    }
    SmmPackRoot roots[8];
    if (smm_pack_roots(roots, 8) == 0) {
        char t[200];
        snprintf(t, sizeof(t), "No %s folder found to save.", smm_loader_name(smm_detect_loader()));
        dlg_message(TAG_LOADOUT_ERR, "Error", t, DB_OK, DI_ERROR);
        return;
    }
    Dlg *d = dlg_push(DK_EXPORT);
    d->tag = 0;
    snprintf(name_buf, sizeof(name_buf), "My Setup");
    scroll.off = 0;
    build();
}

static void set_children(int idx, int v)
{
    Node *p = &nodes[idx];
    size_t pl = strlen(p->path);
    for (int i = 0; i < n_nodes; i++) {
        if (strncmp(nodes[i].path, p->path, pl) == 0 && nodes[i].path[pl] == '/') nodes[i].checked = v;
    }
}

static void confirm(Dlg *d)
{
    char name[160];
    snprintf(name, sizeof(name), "%s", name_buf);
    size_t l = strlen(name);
    while (l && name[l - 1] == ' ') name[--l] = 0;
    if (!name[0]) {
        dlg_message(TAG_EXPORT_EMPTY, "Name Required", "Enter a name for the modpack.", DB_OK, DI_WARN);
        return;
    }
    SmmStrs inc;
    memset(&inc, 0, sizeof(inc));
    for (int i = 0; i < n_nodes; i++)
        if (!nodes[i].is_dir && nodes[i].checked) smm_strs_add(&inc, nodes[i].abs);
    if (inc.n == 0) {
        smm_strs_free(&inc);
        dlg_message(TAG_EXPORT_EMPTY, "Nothing Selected", "Select at least one file to include.", DB_OK, DI_WARN);
        return;
    }
    const char *user = getenv("USER");
    if (!user) user = getenv("USERNAME");
    smm_queue_save_pack(name, user ? user : "user", &inc);
    smm_strs_free(&inc);
    app_status("Saving \"%s\"...", name);
    (void)d;
    dlg_pop();
}

void draw_export(Dlg *d)
{
    R c;
    int closed = dlg_frame("Create Modpack", 460, 580, &c, 1);
    if (closed || K.key == GOOME_K_ESC) {
        dlg_pop();
        return;
    }
    tx(c.x, c.y - 4, "Modpack name:", 13, P.text, 0);
    w_input("packname", rect(c.x, c.y + 18, c.w, 32), name_buf, sizeof(name_buf), NULL, 0, goome_radius(6));
    SmmStrs defs;
    memset(&defs, 0, sizeof(defs));
    smm_pack_default_roots(&defs);
    char dt[200] = "";
    for (int i = 0; i < defs.n; i++) {
        if (i) strncat(dt, " and ", sizeof(dt) - strlen(dt) - 1);
        strncat(dt, defs.v[i], sizeof(dt) - strlen(dt) - 1);
    }
    smm_strs_free(&defs);
    char lbl[300];
    snprintf(lbl, sizeof(lbl), "Files to include (%s are included by default):", dt);
    tx_fit(rect(c.x, c.y + 60, c.w, 18), lbl, 11.3, P.sub, 0, AL_LEFT);
    R view = rect(c.x, c.y + 84, c.w, c.h - 84 - 44);
    fill(view, 0, P.surface);
    stroke(rect(view.x + 0.5, view.y + 0.5, view.w - 1, view.h - 1), 0, 1, P.border);
    int nv = 0;
    int hide_depth = -1;
    for (int i = 0; i < n_nodes; i++) {
        if (hide_depth >= 0 && nodes[i].depth > hide_depth) continue;
        hide_depth = -1;
        visible[nv++] = i;
        if (nodes[i].is_dir && !nodes[i].open) hide_depth = nodes[i].depth;
    }
    double rh = 22;
    R inner = inset(view, 1);
    sc_begin(&scroll, inner, nv * rh);
    for (int k = 0; k < nv; k++) {
        double y = inner.y + k * rh - scroll.off;
        if (y + rh < inner.y || y > inner.y + inner.h) continue;
        Node *n = &nodes[visible[k]];
        double x = inner.x + 6 + n->depth * 18;
        R row = rect(inner.x, y, inner.w, rh);
        if (ui_hover(row)) fill(row, 0, col_alpha(P.alt, 120));
        if (n->is_dir) {
            R ar = rect(x, y + 5, 14, 12);
            if (ui_hover(ar)) ui_cursor(GOOME_CURSOR_HAND);
            if (n->open) ic_chev_down(rect(x + 2, y + 8, 9, 6), P.sub);
            else goome_draw_triangle(K.c, x + 4, y + 5, x + 4, y + 17, x + 10, y + 11, P.sub);
            if (ui_click(ar)) n->open = !n->open;
        }
        R box = rect(x + 18, y + 3, 16, 16);
        int hb = ui_hover(box);
        if (hb) ui_cursor(GOOME_CURSOR_HAND);
        if (n->checked) fill(box, 4, P.accent), ic_check(inset(box, 3.5), 0xFFFFFFFF);
        else stroke(inset(box, 0.7), 4, 1.3, P.sub);
        if (ui_click(box)) {
            n->checked = !n->checked;
            if (n->is_dir) set_children(visible[k], n->checked);
        }
        tx_fit(rect(x + 40, y, inner.x + inner.w - x - 46, rh), n->name, 12.7, P.text, 0, AL_LEFT);
    }
    sc_end(&scroll, inner, nv * rh);
    R row = rect(c.x, c.y + c.h - 34, c.w, 32);
    if (w_button(rect(row.x, row.y, 100, 32), "Cancel", BTN_SOLID, 1)) dlg_pop();
    else if (w_button(rect(row.x + row.w - 100, row.y, 100, 32), "Save", BTN_ACCENT, 1)) confirm(d);
}
