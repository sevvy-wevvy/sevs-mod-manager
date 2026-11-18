#ifndef SMM_INTERNAL_H
#define SMM_INTERNAL_H

#include "smm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define SMM_SEP '\\'
#else
#define SMM_SEP '/'
#endif

#define SMM_TLS __thread

typedef struct {
    char name[SMM_PATH];
    int is_dir;
    long long size;
    long long mtime;
} SmmDirEntry;

typedef struct {
    SmmDirEntry *v;
    int n, cap;
} SmmDirList;

typedef struct SmmProc SmmProc;

struct SmmProgress {
    int op_id;
};

void *smm_xmalloc(size_t n);
void *smm_xrealloc(void *p, size_t n);
char *smm_xstrdup(const char *s);
void smm_copy(char *dst, size_t n, const char *src);
int smm_fmt(char *dst, size_t n, const char *fmt, ...);
void smm_join(char *out, size_t n, const char *a, const char *b);
void smm_join3(char *out, size_t n, const char *a, const char *b, const char *c);
int smm_ends_with(const char *s, const char *suffix);
int smm_ends_with_nocase(const char *s, const char *suffix);
int smm_starts_with(const char *s, const char *prefix);
int smm_starts_with_nocase(const char *s, const char *prefix);
int smm_strcasecmp(const char *a, const char *b);
char *smm_stristr(const char *h, const char *n);
void smm_trim(char *s);
const char *smm_basename(const char *path);
void smm_dirname(const char *path, char *out, size_t n);
void smm_stem(const char *path, char *out, size_t n);
void smm_json_escape(SmmBuf *b, const char *s);
void smm_buf_printf(SmmBuf *b, const char *fmt, ...);
void smm_bufs_cstr(SmmBuf *b);
int smm_iso_time(long long t, char *out, size_t n);
int smm_parse_iso(const char *s, long long *out);
void smm_sort_strs_nocase(SmmStrs *s);

int smm_file_read(const char *path, SmmBuf *out);
int smm_file_write(const char *path, const void *data, size_t n);
int smm_file_write_atomic(const char *path, const void *data, size_t n);
int smm_read_lines(const char *path, SmmStrs *out);
int smm_split_lines(const char *text, size_t n, SmmStrs *out);

FILE *smm_os_fopen(const char *path, const char *mode);
int smm_os_exists(const char *path);
int smm_os_is_file(const char *path);
int smm_os_is_dir(const char *path);
long long smm_os_size(const char *path);
long long smm_os_mtime(const char *path);
int smm_os_mkdir_p(const char *path);
int smm_os_remove(const char *path);
int smm_os_remove_tree(const char *path);
int smm_os_rename(const char *from, const char *to);
int smm_os_copy(const char *from, const char *to);
int smm_os_copy_tree(const char *from, const char *to);
int smm_os_move_tree(const char *from, const char *to);
int smm_os_list(const char *dir, SmmDirList *out);
void smm_dirlist_free(SmmDirList *l);
int smm_os_walk_files(const char *dir, SmmStrs *out);
long long smm_os_tree_size(const char *dir);
int smm_os_count_ext(const char *dir, const char *ext, int recursive);
const char *smm_os_home(void);
void smm_os_config_base(char *out, size_t n);
void smm_os_temp_dir(char *out, size_t n);
const char *smm_os_getenv(const char *k);
int smm_os_setenv(const char *k, const char *v);
void smm_cmdline_quote(const char *const *argv, SmmBuf *out);
int smm_os_run(const char *const *argv, SmmBuf *out, int *exit_code);
SmmProc *smm_os_spawn(const char *const *argv);
int smm_os_proc_running(SmmProc *p, int *exit_code);
void smm_os_proc_free(SmmProc *p);
int smm_os_launch_detached(const char *const *argv, const char *cwd);
int smm_os_process_running(const char *exe_name);
int smm_os_process_kill(const char *exe_name);
int smm_os_open(const char *target);
int smm_os_is_windows(void);
int smm_os_is_mac(void);
long long smm_os_now(void);
void smm_os_sleep(int ms);
long long smm_os_ms(void);
int smm_os_thread(SmmThreadFn fn, void *arg);
void *smm_os_mutex_new(void);
void smm_os_mutex_lock(void *m);
void smm_os_mutex_unlock(void *m);
void *smm_os_cond_new(void);
void smm_os_cond_wait(void *c, void *m);
void smm_os_cond_signal(void *c);
void smm_os_cond_broadcast(void *c);
int smm_os_registry_steam(char *out, size_t n);
void smm_os_prepare_args(int *argc, char ***argv);

typedef struct SmmJson SmmJson;
enum { SMM_J_NULL, SMM_J_BOOL, SMM_J_NUM, SMM_J_STR, SMM_J_ARR, SMM_J_OBJ };
struct SmmJson {
    int type;
    double num;
    int b;
    char *str;
    char *key;
    SmmJson *kids;
    int n, cap;
};

SmmJson *smm_json_parse(const char *text, size_t n);
void smm_json_free(SmmJson *j);
const SmmJson *smm_json_get(const SmmJson *o, const char *key);
const SmmJson *smm_json_at(const SmmJson *a, int i);
const char *smm_json_str(const SmmJson *o, const char *key, const char *def);
double smm_json_num(const SmmJson *o, const char *key, double def);
int smm_json_bool(const SmmJson *o, const char *key, int def);

typedef struct {
    char name[SMM_PATH];
    long long size;
    unsigned csize;
    unsigned method;
    unsigned crc;
    long long local_off;
} SmmZipEntry;

typedef struct {
    FILE *f;
    unsigned char *data;
    size_t len;
    SmmZipEntry *e;
    int n;
} SmmZip;

int smm_zip_open_mem(SmmZip *z, unsigned char *data, size_t len);
int smm_zip_open_file(SmmZip *z, const char *path);
void smm_zip_close(SmmZip *z);
int smm_zip_find(const SmmZip *z, const char *name);
int smm_zip_extract_mem(const SmmZip *z, int i, SmmBuf *out);
int smm_zip_extract_file(const SmmZip *z, int i, const char *dest);
int smm_zip_extract_all(const SmmZip *z, const char *dest_dir);
int smm_inflate(const unsigned char *src, size_t n, unsigned char *dst, size_t dn);
unsigned smm_crc32(unsigned crc, const void *data, size_t n);

typedef struct {
    FILE *f;
    SmmBuf cd;
    int n;
    long long off;
} SmmZipWriter;

int smm_zipw_open(SmmZipWriter *w, const char *path);
int smm_zipw_add_mem(SmmZipWriter *w, const char *name, const void *data, size_t n);
int smm_zipw_add_file(SmmZipWriter *w, const char *name, const char *path);
int smm_zipw_close(SmmZipWriter *w);

int smm_op_fail(SmmProgress *p, const char *fmt, ...);
void smm_state_defaults(SmmSettings *s);
void smm_settings_free(SmmSettings *s);
void smm_set_root_override(const char *dir);
const char *smm_root_override(void);
int smm_demo_http(const char *url, const char *post, SmmBuf *out, int *handled);
int smm_demo_download(const char *url, const char *dest, int *handled);
int smm_config_parse_bepinex(const char *path, SmmConfigFile *file);
int smm_demo_running_flag(int set, int val);
void smm_url_escape(const char *s, char *out, size_t n);
void smm_dir_for_loader(int kind, char *out, size_t n);
int smm_mod_install_zip_file(const char *zip, const char *folder_name, SmmProgress *p);
int smm_sha256_file(const char *path, char *hex_out);

typedef struct {
    char url[512];
    char filename[160];
    char sha256[72];
    char kind[8];
    char version[48];
} SmmMfFile;

int smm_mf_parse_list(const char *text, size_t n, int loader, SmmMods *out, long long *total);
int smm_mf_pick_file(const char *text, size_t n, int loader, SmmMfFile *out);

#endif
