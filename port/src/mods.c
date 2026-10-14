/* Mods (.bbro files).
 *
 * A .bbro file is a zip archive (stored or deflated, as any zip tool makes it); a mod can
 * also be a plain folder with the same contents. In a "mods" folder:
 *
 *   mod.ini              name = ..., author = ..., version = ..., description = ...
 *   patches/NAME.ips     IPS patches for the ROM (the usual ROM-hacking format; may make
 *                        the ROM bigger, up to 4 MiB)
 *   patches/NAME.txt     text patches: "ADDRESS: XX XX XX ..." per line (hex), # comments
 *   NAME.ips             (IPS patches at the top level work too)
 *   levels/...           anything else is ignored by the game (the level maker keeps
 *                        its level sources there)
 *
 * Enabled mods are applied in list order when the ROM is loaded, each mod's patches in
 * file-name order; a later mod wins where two change the same bytes. Changing the
 * game's own instructions is not supported (all code is recompiled from the original
 * ROM; there is no interpreter): data, tables and new routines in Bonanza Maker's
 * shapes are (slotcode.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mods.h"
#include "settings.h"
#include "recomp.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define MOD_FILE_MAX (64u << 20)    /* a .bbro bigger than this is not read */

static char dirs[3][512];
static int ndirs;
static mod_info mods[MODS_MAX];
static int nmods;
static char status_text[96];

/* ------------------------------------------------------------ inflate (RFC 1951) */

typedef struct {
    const uint8_t *in; size_t inlen, inpos;
    uint32_t bitbuf; int bitcnt;
    uint8_t *out; size_t outlen, outpos;
    int err;
} inf_t;

typedef struct { short count[16], symbol[288]; } huff_t;

static int getbits(inf_t *s, int need)
{
    uint32_t v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) { s->err = 1; return 0; }
        v |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1u << need) - 1));
}

static int put(inf_t *s, uint8_t b)
{
    if (s->outpos >= s->outlen) { s->err = 1; return 0; }
    s->out[s->outpos++] = b;
    return 1;
}

static int stored(inf_t *s)
{
    s->bitbuf = 0; s->bitcnt = 0;
    if (s->inpos + 4 > s->inlen) return s->err = 1, 0;
    unsigned len = s->in[s->inpos] | s->in[s->inpos + 1] << 8;
    unsigned nlen = s->in[s->inpos + 2] | s->in[s->inpos + 3] << 8;
    s->inpos += 4;
    if (len != (~nlen & 0xFFFF) || s->inpos + len > s->inlen) return s->err = 1, 0;
    while (len--) if (!put(s, s->in[s->inpos++])) return 0;
    return 1;
}

static int build(huff_t *h, const short *length, int n)
{
    short offs[16];
    memset(h->count, 0, sizeof(h->count));
    for (int i = 0; i < n; i++) h->count[length[i]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return -1;              /* over-subscribed */
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h->count[len];
    for (int i = 0; i < n; i++)
        if (length[i]) h->symbol[offs[length[i]]++] = (short)i;
    return left;
}

static int decode(inf_t *s, const huff_t *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= getbits(s, 1);
        if (s->err) return -1;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->err = 1;
    return -1;
}

static const short len_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                   35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                    4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                    8193, 12289, 16385, 24577};
static const short dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
                                     9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(inf_t *s, const huff_t *lencode, const huff_t *distcode)
{
    for (;;) {
        int sym = decode(s, lencode);
        if (sym < 0) return 0;
        if (sym < 256) { if (!put(s, (uint8_t)sym)) return 0; continue; }
        if (sym == 256) return 1;
        sym -= 257;
        if (sym >= 29) return s->err = 1, 0;
        int len = len_base[sym] + getbits(s, len_extra[sym]);
        int d = decode(s, distcode);
        if (d < 0 || d >= 30) return s->err = 1, 0;
        size_t dist = (size_t)dist_base[d] + getbits(s, dist_extra[d]);
        if (s->err || dist > s->outpos) return s->err = 1, 0;
        while (len--) if (!put(s, s->out[s->outpos - dist])) return 0;
    }
}

static int fixed(inf_t *s)
{
    static huff_t lencode, distcode;
    static int built;
    if (!built) {
        short lengths[288];
        int i = 0;
        for (; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        build(&lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        build(&distcode, lengths, 30);
        built = 1;
    }
    return codes(s, &lencode, &distcode);
}

static int dynamic(inf_t *s)
{
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320];
    huff_t lencode, distcode;
    int nlen = getbits(s, 5) + 257, ndist = getbits(s, 5) + 1, ncode = getbits(s, 4) + 4;
    if (s->err || nlen > 286 || ndist > 30) return s->err = 1, 0;
    int i = 0;
    for (; i < ncode; i++) lengths[order[i]] = (short)getbits(s, 3);
    for (; i < 19; i++) lengths[order[i]] = 0;
    if (s->err || build(&lencode, lengths, 19) != 0) return s->err = 1, 0;
    for (i = 0; i < nlen + ndist;) {
        int sym = decode(s, &lencode);
        if (sym < 0) return 0;
        if (sym < 16) { lengths[i++] = (short)sym; continue; }
        int len = 0, rep;
        if (sym == 16) {
            if (i == 0) return s->err = 1, 0;
            len = lengths[i - 1];
            rep = 3 + getbits(s, 2);
        } else if (sym == 17) {
            rep = 3 + getbits(s, 3);
        } else {
            rep = 11 + getbits(s, 7);
        }
        if (s->err || i + rep > nlen + ndist) return s->err = 1, 0;
        while (rep--) lengths[i++] = (short)len;
    }
    if (lengths[256] == 0) return s->err = 1, 0;
    int err = build(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return s->err = 1, 0;
    err = build(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return s->err = 1, 0;
    return codes(s, &lencode, &distcode);
}

static int inflate_raw(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen)
{
    inf_t s;
    memset(&s, 0, sizeof(s));
    s.in = in; s.inlen = inlen; s.out = out; s.outlen = outlen;
    int last;
    do {
        last = getbits(&s, 1);
        int type = getbits(&s, 2);
        if (s.err) return 0;
        int ok = type == 0 ? stored(&s) : type == 1 ? fixed(&s) : type == 2 ? dynamic(&s) : 0;
        if (!ok || s.err) return 0;
    } while (!last);
    return s.outpos == outlen;
}

/* ------------------------------------------------------------ zip */

static uint32_t le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* calls fn for every file in the archive; stops when it returns non-zero */
static int zip_each(const uint8_t *z, size_t n, int (*fn)(const char *name, size_t nlen, size_t at, void *u), void *u)
{
    if (n < 22) return -1;
    size_t e = n - 22, stop = n > 65557 ? n - 65557 : 0;
    for (;; e--) {
        if (le32(z + e) == 0x06054b50) break;
        if (e == stop) return -1;
    }
    uint32_t count = le16(z + e + 10), cd = le32(z + e + 16);
    size_t p = cd;
    for (uint32_t i = 0; i < count; i++) {
        if (p + 46 > n || le32(z + p) != 0x02014b50) return -1;
        size_t nl = le16(z + p + 28), xl = le16(z + p + 30), cl = le16(z + p + 32);
        if (p + 46 + nl > n) return -1;
        int r = fn((const char *)z + p + 46, nl, p, u);
        if (r) return r;
        p += 46 + nl + xl + cl;
    }
    return 0;
}

static uint8_t *zip_read_at(const uint8_t *z, size_t n, size_t cdp, size_t *out_size)
{
    uint32_t method = le16(z + cdp + 10), csize = le32(z + cdp + 20), usize = le32(z + cdp + 24);
    size_t lh = le32(z + cdp + 42);
    if (lh + 30 > n || le32(z + lh) != 0x04034b50 || usize > MOD_FILE_MAX) return NULL;
    size_t data = lh + 30 + le16(z + lh + 26) + le16(z + lh + 28);
    if (data + csize > n) return NULL;
    uint8_t *out = malloc(usize ? usize : 1);
    if (!out) return NULL;
    if (method == 0 && csize == usize) {
        memcpy(out, z + data, usize);
    } else if (method != 8 || !inflate_raw(z + data, csize, out, usize)) {
        free(out);
        return NULL;
    }
    *out_size = usize;
    return out;
}

struct find_ctx { const char *name; size_t at; };
static int find_cb(const char *name, size_t nlen, size_t at, void *u)
{
    struct find_ctx *c = u;
    if (strlen(c->name) == nlen && !memcmp(name, c->name, nlen)) { c->at = at; return 1; }
    return 0;
}

uint8_t *zip_extract(const uint8_t *zip, size_t zsize, const char *name, size_t *out_size)
{
    struct find_ctx c = {name, 0};
    if (zip_each(zip, zsize, find_cb, &c) != 1) return NULL;
    return zip_read_at(zip, zsize, c.at, out_size);
}

/* ------------------------------------------------------------ files */

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || (unsigned long)n > MOD_FILE_MAX) { fclose(f); return NULL; }
    uint8_t *b = malloc(n ? (size_t)n : 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) *size = (size_t)n;
    return b;
}

typedef void (*list_fn)(const char *name, int is_dir, void *u);

static void list_dir(const char *dir, list_fn fn, void *u)
{
#ifdef _WIN32
    char pat[600];
    snprintf(pat, sizeof(pat), "%s*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == '.') continue;
        fn(fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, u);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char p[1100];
        snprintf(p, sizeof(p), "%s%s", dir, e->d_name);
        struct stat st;
        int is_dir = stat(p, &st) == 0 && S_ISDIR(st.st_mode);
        fn(e->d_name, is_dir, u);
    }
    closedir(d);
#endif
}

/* ------------------------------------------------------------ a mod's files */

#define MOD_FILES 64
typedef struct {
    const mod_info *m;
    uint8_t *zip; size_t zsize;          /* archive in memory, or NULL for a folder */
    char names[MOD_FILES][128];          /* patch files, sorted */
    int n;
    int has_ini;
} mod_files;

static int ends_with(const char *s, const char *ext)
{
    size_t a = strlen(s), b = strlen(ext);
    if (a < b) return 0;
    for (size_t i = 0; i < b; i++)
        if (tolower((unsigned char)s[a - b + i]) != ext[i]) return 0;
    return 1;
}

static int is_patch(const char *name)
{
    const char *base = strrchr(name, '/');
    int in_patches = !strncmp(name, "patches/", 8) && !strchr(name + 8, '/');
    int at_top = !base;
    (void)base;
    return (in_patches && (ends_with(name, ".ips") || ends_with(name, ".txt"))) ||
           (at_top && ends_with(name, ".ips"));
}

static void add_name(mod_files *f, const char *name, size_t len)
{
    if (len == 7 && !memcmp(name, "mod.ini", 7)) f->has_ini = 1;
    if (len >= sizeof(f->names[0]) || f->n >= MOD_FILES) return;
    char tmp[128];
    memcpy(tmp, name, len);
    tmp[len] = 0;
    for (char *c = tmp; *c; c++) if (*c == '\\') *c = '/';
    if (is_patch(tmp)) strcpy(f->names[f->n++], tmp);
}

static int zip_name_cb(const char *name, size_t nlen, size_t at, void *u)
{
    (void)at;
    add_name(u, name, nlen);
    return 0;
}

static void dir_top_cb(const char *name, int is_dir, void *u)
{
    if (!is_dir) add_name(u, name, strlen(name));
}

static void dir_patch_cb(const char *name, int is_dir, void *u)
{
    if (is_dir) return;
    char p[160];
    snprintf(p, sizeof(p), "patches/%s", name);
    add_name(u, p, strlen(p));
}

static int name_cmp(const void *a, const void *b) { return strcmp(a, b); }

static int mod_open(const mod_info *m, mod_files *f)
{
    memset(f, 0, sizeof(*f));
    f->m = m;
    if (m->is_dir) {
        list_dir(m->path, dir_top_cb, f);
        char sub[640];
        snprintf(sub, sizeof(sub), "%spatches/", m->path);
        list_dir(sub, dir_patch_cb, f);
    } else {
        f->zip = read_file(m->path, &f->zsize);
        if (!f->zip) return 0;
        if (zip_each(f->zip, f->zsize, zip_name_cb, f) < 0) { free(f->zip); f->zip = NULL; return 0; }
    }
    qsort(f->names, f->n, sizeof(f->names[0]), name_cmp);
    return 1;
}

static uint8_t *mod_read(const mod_files *f, const char *name, size_t *size)
{
    if (f->zip) return zip_extract(f->zip, f->zsize, name, size);
    char p[800];
    snprintf(p, sizeof(p), "%s%s", f->m->path, name);
    return read_file(p, size);
}

static void mod_close(mod_files *f)
{
    free(f->zip);
    f->zip = NULL;
}

/* ------------------------------------------------------------ mod.ini */

static void ini_value(const char *text, size_t n, const char *key, char *out, size_t outsz)
{
    size_t kl = strlen(key);
    for (size_t i = 0; i < n;) {
        size_t e = i;
        while (e < n && text[e] != '\n') e++;
        const char *l = text + i;
        size_t ll = e - i;
        while (ll && (*l == ' ' || *l == '\t')) { l++; ll--; }
        if (ll > kl && !strncmp(l, key, kl) && (l[kl] == ' ' || l[kl] == '=' || l[kl] == '\t')) {
            const char *v = memchr(l, '=', ll);
            if (v) {
                v++;
                size_t vl = ll - (size_t)(v - l);
                while (vl && (*v == ' ' || *v == '\t')) { v++; vl--; }
                while (vl && (v[vl - 1] == '\r' || v[vl - 1] == ' ' || v[vl - 1] == '\t')) vl--;
                if (vl >= outsz) vl = outsz - 1;
                memcpy(out, v, vl);
                out[vl] = 0;
                /* the menu font knows ASCII only */
                for (char *c = out; *c; c++) if ((unsigned char)*c >= 0x80 || *c < 0x20) *c = '?';
            }
            return;
        }
        i = e + 1;
    }
}

static void read_info(mod_info *m)
{
    mod_files f;
    snprintf(m->name, sizeof(m->name), "%.47s", m->file);
    if (!mod_open(m, &f)) {
        snprintf(m->error, sizeof(m->error), "cannot read it (not a zip / .bbro?)");
        return;
    }
    m->name[0] = 0;
    m->patches = f.n;
    size_t n = 0;
    uint8_t *ini = f.has_ini ? mod_read(&f, "mod.ini", &n) : NULL;
    if (ini) {
        ini_value((const char *)ini, n, "name", m->name, sizeof(m->name));
        ini_value((const char *)ini, n, "author", m->author, sizeof(m->author));
        ini_value((const char *)ini, n, "version", m->version, sizeof(m->version));
        ini_value((const char *)ini, n, "description", m->description, sizeof(m->description));
        free(ini);
    }
    if (!m->name[0]) {
        snprintf(m->name, sizeof(m->name), "%.47s", m->file);
        char *dot = strrchr(m->name, '.');
        if (dot && !m->is_dir) *dot = 0;
    }
    if (!f.n) snprintf(m->error, sizeof(m->error), "no patches in it");
    mod_close(&f);
}

/* ------------------------------------------------------------ patches */

static int apply_ips(const uint8_t *p, size_t n, uint8_t *rom, size_t *size, size_t cap)
{
    if (n < 8 || memcmp(p, "PATCH", 5)) return 0;
    size_t i = 5;
    for (;;) {
        if (i + 3 > n) return 0;
        if (!memcmp(p + i, "EOF", 3)) {
            i += 3;
            if (i + 3 <= n) {                    /* truncation extension */
                size_t t = (size_t)p[i] << 16 | p[i + 1] << 8 | p[i + 2];
                if (t && t < *size) *size = t;
            }
            return 1;
        }
        if (i + 5 > n) return 0;
        size_t off = (size_t)p[i] << 16 | p[i + 1] << 8 | p[i + 2];
        size_t len = (size_t)p[i + 3] << 8 | p[i + 4];
        i += 5;
        if (len == 0) {                          /* run of one byte */
            if (i + 3 > n) return 0;
            size_t rl = (size_t)p[i] << 8 | p[i + 1];
            uint8_t v = p[i + 2];
            i += 3;
            if (off + rl > cap) return 0;
            if (off > *size) memset(rom + *size, 0xFF, off - *size);
            memset(rom + off, v, rl);
            if (off + rl > *size) *size = off + rl;
        } else {
            if (i + len > n || off + len > cap) return 0;
            if (off > *size) memset(rom + *size, 0xFF, off - *size);
            memcpy(rom + off, p + i, len);
            i += len;
            if (off + len > *size) *size = off + len;
        }
    }
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* "ADDRESS: XX XX XX" lines; ADDRESS may start with 0x or $ */
static int apply_text(const uint8_t *p, size_t n, uint8_t *rom, size_t *size, size_t cap)
{
    size_t i = 0;
    while (i < n) {
        size_t e = i;
        while (e < n && p[e] != '\n') e++;
        size_t k = i;
        while (k < e && (p[k] == ' ' || p[k] == '\t' || p[k] == '\r')) k++;
        if (k < e && p[k] != '#' && p[k] != ';') {
            if (p[k] == '$') k++;
            else if (k + 1 < e && p[k] == '0' && (p[k + 1] == 'x' || p[k + 1] == 'X')) k += 2;
            size_t addr = 0;
            int digits = 0;
            while (k < e && hexval(p[k]) >= 0) { addr = addr * 16 + (size_t)hexval(p[k]); k++; digits++; }
            while (k < e && (p[k] == ' ' || p[k] == '\t')) k++;
            if (!digits || k >= e || (p[k] != ':' && p[k] != '=')) return 0;
            k++;
            for (;;) {
                while (k < e && (p[k] == ' ' || p[k] == '\t' || p[k] == ',' || p[k] == '\r')) k++;
                if (k >= e || p[k] == '#' || p[k] == ';') break;
                int a = hexval(p[k]), b = k + 1 < e ? hexval(p[k + 1]) : -1;
                if (a < 0 || b < 0 || addr >= cap) return 0;
                if (addr > *size) memset(rom + *size, 0xFF, addr - *size);
                rom[addr++] = (uint8_t)(a * 16 + b);
                if (addr > *size) *size = addr;
                k += 2;
            }
        }
        i = e + 1;
    }
    return 1;
}

/* ------------------------------------------------------------ list */

void mods_add_dir(const char *dir)
{
    if (ndirs >= 3 || !dir || !dir[0]) return;
    for (int i = 0; i < ndirs; i++) if (!strcmp(dirs[i], dir)) return;
    snprintf(dirs[ndirs++], sizeof(dirs[0]), "%s", dir);
}

const char *mods_main_dir(void) { return ndirs ? dirs[0] : ""; }

static int enabled_pos(const char *file)
{
    /* settings.mods: enabled mods in order, separated by '|' */
    const char *s = settings.mods;
    int pos = 0;
    size_t fl = strlen(file);
    while (*s) {
        const char *e = strchr(s, '|');
        size_t l = e ? (size_t)(e - s) : strlen(s);
        if (l == fl && !strncmp(s, file, l)) return pos;
        pos++;
        if (!e) break;
        s = e + 1;
    }
    return -1;
}

struct scan_ctx { const char *dir; };
static void scan_cb(const char *name, int is_dir, void *u)
{
    struct scan_ctx *c = u;
    if (nmods >= MODS_MAX || strlen(name) >= sizeof(mods[0].file)) return;
    if (!is_dir && !ends_with(name, ".bbro") && !ends_with(name, ".zip")) return;
    for (int i = 0; i < nmods; i++) if (!strcmp(mods[i].file, name)) return;   /* first folder wins */
    mod_info *m = &mods[nmods];
    memset(m, 0, sizeof(*m));
    snprintf(m->file, sizeof(m->file), "%s", name);
    snprintf(m->path, sizeof(m->path), "%s%s%s", c->dir, name, is_dir ? "/" : "");
    m->is_dir = is_dir;
    read_info(m);
    if (is_dir && !m->patches) return;          /* some other folder */
    nmods++;
}

static int mod_order(const void *a, const void *b)
{
    const mod_info *x = a, *y = b;
    int px = enabled_pos(x->file), py = enabled_pos(y->file);
    if (px >= 0 && py >= 0) return px - py;
    if (px >= 0 || py >= 0) return px >= 0 ? -1 : 1;
    return strcmp(x->file, y->file);
}

void mods_scan(void)
{
    nmods = 0;
    for (int d = 0; d < ndirs; d++) {
        struct scan_ctx c = {dirs[d]};
        list_dir(dirs[d], scan_cb, &c);
    }
    qsort(mods, nmods, sizeof(mods[0]), mod_order);
    for (int i = 0; i < nmods; i++)
        mods[i].enabled = enabled_pos(mods[i].file) >= 0 && !mods[i].error[0];
}

int mods_count(void) { return nmods; }
mod_info *mods_get(int i) { return i >= 0 && i < nmods ? &mods[i] : NULL; }

int mods_enabled_count(void)
{
    int n = 0;
    for (int i = 0; i < nmods; i++) n += mods[i].enabled;
    return n;
}

static void save_enabled(void)
{
    char s[sizeof(settings.mods)] = "";
    for (int i = 0; i < nmods; i++) {
        if (!mods[i].enabled) continue;
        if (strlen(s) + strlen(mods[i].file) + 2 >= sizeof(s)) break;
        if (s[0]) strcat(s, "|");
        strcat(s, mods[i].file);
    }
    snprintf(settings.mods, sizeof(settings.mods), "%s", s);
}

void mods_toggle(int i)
{
    if (i < 0 || i >= nmods || mods[i].error[0]) return;
    mods[i].enabled = !mods[i].enabled;
    if (mods[i].enabled) {
        /* newly enabled mods go last (they win over the others) */
        mod_info m = mods[i];
        int last = i;
        while (last + 1 < nmods && mods[last + 1].enabled) last++;
        memmove(&mods[i], &mods[i + 1], (size_t)(last - i) * sizeof(mods[0]));
        mods[last] = m;
    }
    save_enabled();
}

/* ------------------------------------------------------------ apply */

size_t mods_apply(const uint8_t *rom, size_t size, uint8_t *out, size_t cap)
{
    memcpy(out, rom, size);
    recomp_validate_all();
    status_text[0] = 0;
    size_t n = size;
    int applied = 0, failed = 0;
    uint8_t *tmp = NULL;
    for (int i = 0; i < nmods; i++) {
        mod_info *m = &mods[i];
        if (!m->enabled) continue;
        mod_files f;
        if (!mod_open(m, &f)) { snprintf(m->error, sizeof(m->error), "cannot read it"); failed++; continue; }
        /* a mod is applied completely or not at all */
        if (!tmp && !(tmp = malloc(cap))) { mod_close(&f); break; }
        memcpy(tmp, out, n);
        size_t tn = n;
        int ok = 1;
        for (int k = 0; k < f.n && ok; k++) {
            size_t ps = 0;
            uint8_t *p = mod_read(&f, f.names[k], &ps);
            if (!p) { ok = 0; snprintf(m->error, sizeof(m->error), "cannot read %.60s", f.names[k]); break; }
            ok = ends_with(f.names[k], ".ips") ? apply_ips(p, ps, tmp, &tn, cap) : apply_text(p, ps, tmp, &tn, cap);
            if (!ok) snprintf(m->error, sizeof(m->error), "bad patch %.60s", f.names[k]);
            free(p);
        }
        mod_close(&f);
        if (ok) {
            memcpy(out, tmp, tn);
            n = tn;
            applied++;
        } else {
            failed++;
        }
    }
    free(tmp);
    if (n & 1) out[n++] = 0xFF;
    /* several mods may each have set the header for their own changes: the ROM end and
     * checksum (the game checks it at boot) are made right for all of them together */
    if (applied && n >= 0x200) {
        uint32_t end = (uint32_t)n - 1;
        uint16_t sum = 0;
        out[0x1A4] = (uint8_t)(end >> 24); out[0x1A5] = (uint8_t)(end >> 16);
        out[0x1A6] = (uint8_t)(end >> 8); out[0x1A7] = (uint8_t)end;
        for (size_t a = 0x200; a + 1 < n; a += 2) sum = (uint16_t)(sum + (out[a] << 8 | out[a + 1]));
        out[0x18E] = (uint8_t)(sum >> 8); out[0x18F] = (uint8_t)sum;
    }
    /* changes to the game's own instructions cannot run (no native code for them) */
    int code = 0;
    for (size_t a = 0; a < size && a < n; a++)
        if (out[a] != rom[a] && recomp_invalidate((uint32_t)a)) code++;
    if (applied || failed)
        snprintf(status_text, sizeof(status_text), "%d mod%s active%s%s", applied, applied == 1 ? "" : "s",
                 failed ? ", some failed (see Mods...)" : "", code ? " (changes game code: not supported)" : "");
    return n;
}

const char *mods_status(void) { return status_text; }
