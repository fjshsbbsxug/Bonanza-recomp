/* Extra stages: new stages a mod puts between the cartridge's own (the level maker's "New
 * stage", e.g. the arcade's Department Store after Mint).
 *
 * A mod stores each one in a slot past the original ROM (0x80000 + 0x20000 * k), starting
 * with a descriptor:
 *   'BBXSTAGE', word version (1), word after (the entry it follows), word host (the entry
 *   whose code it plays on), word 0, long name frame mapping, long "STAGE 11" mapping,
 *   long "STAGE 12" mapping, long patch list: {long addr, word len, bytes (padded to
 *   even)}..., long -1.
 * The patch list is the host's table values for the new stage. Everything the game knows
 * about an entry comes from tables (scratchpad research: intro, targets screen, setup, HUD,
 * spawn and escape), and every way into an entry passes the dispatcher sub_11106 once. So
 * at that point, when the game moves on from the entry an extra stage follows, the host's
 * values are written into the ROM buffer and the entry set to the host; when the host
 * entry is finished they are put back and the game goes on with the next real entry.
 * The stage numbers of the following stages move up by one.
 *
 * Only ROM data is changed, and the state here (the last entry, the stage being played)
 * changes only from the game's own progress, so two machines in an online game stay the
 * same. With no extra stages nothing is hooked. */
#include <stdlib.h>
#include <string.h>
#include "extrastages.h"
#include "gamemods.h"
#include "md.h"
#include "recomp.h"

#define XS_MAX 8
#define FRAME_STAGE11 0x398    /* Japanese stage-name frames: unused by the export version */
#define FRAME_STAGE12 0x399
#define FRAME_NAME0   0x39A

typedef struct {
    uint32_t base, name_map, s11, s12, list;
    int after, host;
} xs_stage;

typedef struct { uint32_t addr; uint8_t old; } undo_rec;

static xs_stage xs[XS_MAX];
static int nxs;
static int last_entry, active = -1, forced = -1;
static undo_rec *undo;
static size_t nundo, capundo;

static const int STAGE_ENTRIES[10] = {1, 2, 3, 5, 6, 7, 9, 10, 11, 13};

static uint32_t r16(uint32_t a) { return (uint32_t)(md.rom[a] << 8 | md.rom[a + 1]); }
static uint32_t r32(uint32_t a) { return r16(a) << 16 | r16(a + 2); }
static int ram16(uint32_t a) { return md.ram[a & 0xFFFF] << 8 | md.ram[(a + 1) & 0xFFFF]; }
static void wram16(uint32_t a, int v) { md.ram[a & 0xFFFF] = (uint8_t)(v >> 8); md.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }

/* write a ROM byte, remembering the old value (put back by restore()) */
static void put8(uint32_t a, uint8_t v)
{
    if (a >= sizeof(md.rom)) return;
    if (nundo == capundo) {
        size_t c = capundo ? capundo * 2 : 4096;
        undo_rec *u = realloc(undo, c * sizeof(*u));
        if (!u) return;
        undo = u;
        capundo = c;
    }
    undo[nundo].addr = a;
    undo[nundo].old = md.rom[a];
    nundo++;
    md.rom[a] = v;
}
static void put16(uint32_t a, uint32_t v) { put8(a, (uint8_t)(v >> 8)); put8(a + 1, (uint8_t)v); }
static void put32(uint32_t a, uint32_t v) { put16(a, v >> 16); put16(a + 2, v & 0xFFFF); }
static void restore(void)
{
    while (nundo) {
        nundo--;
        md.rom[undo[nundo].addr] = undo[nundo].old;
    }
}

static int stage_number(int e)
{
    for (int i = 0; i < 10; i++)
        if (STAGE_ENTRIES[i] == e) return i + 1;
    return 0;
}
/* the number shown for a cartridge stage: one more for every extra stage before it */
static int shown_number(int e)
{
    int n = stage_number(e);
    for (int k = 0; k < nxs; k++)
        if (xs[k].after < e) n++;
    return n;
}
/* an extra stage's number: right after the stage it follows */
static int extra_number(int k)
{
    int n = shown_number(xs[k].after) + 1;
    for (int j = 0; j < k; j++)
        if (xs[j].after == xs[k].after) n++;
    return n;
}
/* the intro's "STAGE n" frame and the HUD's two digits */
static uint32_t stage_frame(int n)
{
    if (n <= 10) return 0x3AC + (uint32_t)(n < 1 ? 1 : n) - 1;
    return n == 11 ? FRAME_STAGE11 : FRAME_STAGE12;
}
static void hud_digits(int n, uint32_t *a, uint32_t *b)
{
    *a = n < 10 ? r16(0x10D84 + 4) : 0x8031;     /* entry 1's first tile is the blank */
    *b = 0x8030 + (uint32_t)(n % 10);
}
static uint32_t frame_table(void) { return r32(0x59E6) & 0xFFFFFF; }

void xs_scan(void)
{
    nxs = 0;
    nundo = 0;
    active = forced = -1;
    last_entry = 0;
    for (uint32_t base = 0x80000; base + 32 <= md.rom_size && nxs < XS_MAX; base += 0x20000) {
        if (memcmp(md.rom + base, "BBXSTAGE", 8) || r16(base + 8) != 1) continue;
        xs_stage x = { base, r32(base + 16), r32(base + 20), r32(base + 24), r32(base + 28),
                       (int)r16(base + 10), (int)r16(base + 12) };
        int host_ok = 0;
        for (int i = 0; i < 10; i++) host_ok |= STAGE_ENTRIES[i] == x.host;
        if (!host_ok || x.host == 1 || x.host == 6 || !stage_number(x.after)) continue;
        if (x.list >= md.rom_size || x.name_map >= md.rom_size) continue;
        int i = nxs++;
        while (i > 0 && xs[i - 1].after > x.after) { xs[i] = xs[i - 1]; i--; }    /* in game order */
        xs[i] = x;
    }
    if (!nxs) return;
    /* for good: frames for "STAGE 11/12" and the names, the stage numbers of the
     * cartridge's stages that come later now */
    uint32_t tbl = frame_table();
    uint32_t w[3] = { FRAME_STAGE11, FRAME_STAGE12, 0 }, v[2] = { xs[0].s11, xs[0].s12 };
    for (int i = 0; i < 2; i++) {
        md.rom[tbl + 4 * w[i]] = (uint8_t)(v[i] >> 24); md.rom[tbl + 4 * w[i] + 1] = (uint8_t)(v[i] >> 16);
        md.rom[tbl + 4 * w[i] + 2] = (uint8_t)(v[i] >> 8); md.rom[tbl + 4 * w[i] + 3] = (uint8_t)v[i];
    }
    for (int k = 0; k < nxs; k++) {
        uint32_t a = tbl + 4 * (FRAME_NAME0 + (uint32_t)k), m = xs[k].name_map;
        md.rom[a] = (uint8_t)(m >> 24); md.rom[a + 1] = (uint8_t)(m >> 16);
        md.rom[a + 2] = (uint8_t)(m >> 8); md.rom[a + 3] = (uint8_t)m;
    }
    for (int i = 0; i < 10; i++) {
        int e = STAGE_ENTRIES[i], n = shown_number(e);
        uint32_t f = stage_frame(n), d0, d1;
        hud_digits(n, &d0, &d1);
        md.rom[0x113CA + 2 * e] = (uint8_t)(f >> 8); md.rom[0x113CA + 2 * e + 1] = (uint8_t)f;
        md.rom[0x10D84 + 4 * e] = (uint8_t)(d0 >> 8); md.rom[0x10D84 + 4 * e + 1] = (uint8_t)d0;
        md.rom[0x10D84 + 4 * e + 2] = (uint8_t)(d1 >> 8); md.rom[0x10D84 + 4 * e + 3] = (uint8_t)d1;
    }
    /* code an extra stage changes is run by the interpreter */
    for (int k = 0; k < nxs; k++)
        for (uint32_t p = xs[k].list; p + 6 <= md.rom_size && r32(p) != 0xFFFFFFFFu;) {
            uint32_t a = r32(p), n = r16(p + 4);
            for (uint32_t i = 0; i < n; i++)
                if (a + i < 0x80000) recomp_invalidate(a + i);
            p += 6 + ((n + 1) & ~1u);
        }
    game_mods_apply();
}

static void apply(int k)
{
    const xs_stage *x = &xs[k];
    for (uint32_t p = x->list; p + 6 <= md.rom_size && r32(p) != 0xFFFFFFFFu;) {
        uint32_t a = r32(p), n = r16(p + 4);
        for (uint32_t i = 0; i < n; i++) put8(a + i, md.rom[p + 6 + i]);
        p += 6 + ((n + 1) & ~1u);
    }
    /* its own frame table (borrowed treasures) needs the new frames too */
    uint32_t tbl = frame_table();
    if (tbl != 0x24B80) {
        put32(tbl + 4 * FRAME_STAGE11, xs[0].s11);
        put32(tbl + 4 * FRAME_STAGE12, xs[0].s12);
        put32(tbl + 4 * (FRAME_NAME0 + (uint32_t)k), x->name_map);
    }
    int n = extra_number(k);
    uint32_t d0, d1;
    hud_digits(n, &d0, &d1);
    put16(0x113CA + 2 * (uint32_t)x->host, stage_frame(n));
    put16(0x113AE + 2 * (uint32_t)x->host, FRAME_NAME0 + (uint32_t)k);
    put16(0x10D84 + 4 * (uint32_t)x->host, d0);
    put16(0x10D84 + 4 * (uint32_t)x->host + 2, d1);
    active = k;
    wram16(0xFFC05A, x->host);
    last_entry = x->host;
}

void xs_hook(void)
{
    if (!nxs) return;
    int e = ram16(0xFFC05A), done_extra = 0;
    if (active >= 0) {
        const xs_stage *x = &xs[active];
        restore();
        active = -1;
        if (e == x->host + 1 && last_entry == x->host) {     /* it was cleared: go on */
            e = x->after + 1;
            done_extra = 1;
        }
    }
    if (forced >= 0) {
        int k = forced;
        forced = -1;
        apply(k);
        return;
    }
    if (!done_extra && last_entry > 0 && e == last_entry + 1)
        for (int k = 0; k < nxs; k++)
            if (xs[k].after == last_entry) { apply(k); return; }
    wram16(0xFFC05A, e);
    last_entry = e;
}

void xs_reset(void)
{
    restore();
    active = -1;
    last_entry = 0;
}

int xs_count(void) { return nxs; }

void xs_play(int k)
{
    if (k >= 0 && k < nxs) forced = k;
}
