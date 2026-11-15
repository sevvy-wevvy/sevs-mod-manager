#ifndef SMM_WEBP_H
#define SMM_WEBP_H

#include <stddef.h>
#include <stdint.h>
#include "../../goome.h"

int smm_webp_check(const void *data, size_t n);
int smm_webp_decode(const void *data, size_t n, goome_bitmap *out);
uint32_t *smm_webp_decode_argb(const void *data, size_t n, int *w, int *h);

#endif
