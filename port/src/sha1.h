#ifndef SHA1_H
#define SHA1_H
#include <stddef.h>
#include <stdint.h>
/* writes 40 lowercase hex chars + NUL into out */
void sha1_hex(const uint8_t *data, size_t len, char out[41]);
#endif
