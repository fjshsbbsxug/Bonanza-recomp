/* Minimal SHA-1 (public domain style implementation) */
#include <string.h>
#include <stdio.h>
#include "sha1.h"

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void block(uint32_t h[5], const uint8_t *p)
{
    uint32_t w[80], a, b, c, d, e;
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 80; i++)
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t t = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void sha1_hex(const uint8_t *data, size_t len, char out[41])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t tail[128];
    size_t full = len & ~(size_t)63, rem = len - full;
    for (size_t i = 0; i < full; i += 64)
        block(h, data + i);
    memset(tail, 0, sizeof(tail));
    memcpy(tail, data + full, rem);
    tail[rem] = 0x80;
    size_t tl = rem < 56 ? 64 : 128;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        tail[tl - 1 - i] = (uint8_t)(bits >> (i * 8));
    block(h, tail);
    if (tl == 128)
        block(h, tail + 64);
    for (int i = 0; i < 5; i++)
        sprintf(out + i * 8, "%08x", h[i]);
}
