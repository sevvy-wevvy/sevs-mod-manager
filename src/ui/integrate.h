#ifndef SMM_INTEGRATE_H
#define SMM_INTEGRATE_H

#include <stddef.h>
#include "../core/smm.h"

#define INTEG_APP_NAME "Sevs Mod Manager"
#define INTEG_APP_ID "sevs-mod-manager"
#define INTEG_PACK_NAME "Sev's Mod Manager Pack"
#define INTEG_PACK_MIME "application/x-smmpack"

int integrate_exe_path(char *out, size_t n);
int integrate_register(void);
int integrate_unregister(void);
void integrate_auto(void);
int integrate_cli(const char *arg);

int integ_os_register(const char *exe, const unsigned char *png, size_t png_len);
int integ_os_unregister(void);
int integ_write(const char *path, const void *data, size_t n);
int integ_run_code(const char *a, const char *b, const char *c, const char *d, const char *e);
void integ_applet_source(SmmBuf *b, const char *exe);
void integ_plist_types(SmmBuf *b);
void integ_plist_full(SmmBuf *b);
void integ_plist_fragment(SmmBuf *b);
void integ_icns(SmmBuf *b, const unsigned char *png, size_t n);
void integ_run_quiet(const char *a, const char *b, const char *c, const char *d, const char *e);

#endif
