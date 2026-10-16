/* The game's video stores, written directly (see video.h). The stores themselves are
 * the arrays in vdp_t (vdp.h), which the picture is drawn from. */
#include "video.h"
#include "vdp.h"
#include "md.h"

void video_store(int to, uint16_t addr, uint16_t w)
{
    switch (to & 0x0F) {
    case VIDEO_VRAM:                    /* big-endian words; an odd address swaps the bytes */
        if (addr & 1)
            w = (uint16_t)((w >> 8) | (w << 8));
        vdp.vram[addr & 0xFFFE] = (uint8_t)(w >> 8);
        vdp.vram[(addr & 0xFFFE) | 1] = (uint8_t)w;
        break;
    case VIDEO_CRAM:
        vdp.cram[(addr >> 1) & 0x3F] = w & 0x0EEE;
        break;
    case VIDEO_VSRAM: {
        int i = (addr >> 1) & 0x3F;
        if (i < 40)
            vdp.vsram[i] = w & 0x07FF;
        break;
    }
    default:
        break;
    }
}

void video_at(int to, uint16_t addr)
{
    vdp.code = (uint8_t)to;
    vdp.addr = addr;
}

void video_put(uint16_t w)
{
    if (vdp.pending || vdp.fill_pending) {     /* a mod's half-written command on the port */
        vdp_write_data(w);
        return;
    }
    video_store(vdp.code, vdp.addr, w);
    vdp.addr += vdp.reg[15];
}

void video_set(int r, uint8_t v)
{
    vdp.reg[r] = v;
    if (r == 0 || r == 1)               /* the frame's and the bands' interrupts on or off */
        cpu_set_irq(vdp_irq_level());
}

void video_reg_word(uint16_t w)
{
    if (vdp.pending || (w & 0xC000) != 0x8000 || ((w >> 8) & 0x1F) >= 24) {
        vdp_reg_word(w);                /* not a register word: as the port takes it */
        return;
    }
    video_set((w >> 8) & 0x1F, (uint8_t)w);
}

void video_at_long(uint32_t cmd)
{
    uint16_t hi = (uint16_t)(cmd >> 16), lo = (uint16_t)cmd;
    if (vdp.pending) {
        vdp_command_long(cmd);
        return;
    }
    if ((hi & 0xC000) == 0x8000) {      /* two register words */
        video_reg_word(hi);
        video_reg_word(lo);
        return;
    }
    int to = ((hi >> 14) & 3) | ((lo >> 2) & 0x3C);
    uint16_t addr = (uint16_t)((hi & 0x3FFF) | ((lo & 3) << 14));
    if (to & 0x20) {                    /* a copy set up through the registers (mods) */
        vdp_command_long(cmd);
        return;
    }
    video_at(to, addr);
}

uint16_t video_from_ram(int to, uint16_t dst, uint32_t src, uint32_t n, int stride)
{
    while (n--) {
        uint16_t w = (uint16_t)md_read16(src);
        src = (src & 0xFE0000) | ((src + 2) & 0x1FFFF);   /* within a 128 KiB window */
        video_store(to, dst, w);
        dst = (uint16_t)(dst + stride);
    }
    return dst;
}

uint16_t video_fill(uint16_t dst, uint32_t len, int stride, uint16_t w)
{
    video_store(VIDEO_VRAM, dst, w);
    dst = (uint16_t)(dst + stride);
    while (len--) {
        vdp.vram[dst ^ 1] = (uint8_t)(w >> 8);
        dst = (uint16_t)(dst + stride);
    }
    return dst;
}

uint16_t video_vram_copy(uint16_t dst, uint16_t src, uint32_t len, int stride)
{
    while (len--) {
        vdp.vram[dst] = vdp.vram[src];
        src++;
        dst = (uint16_t)(dst + stride);
    }
    return dst;
}

uint16_t video_status(void)
{
    uint16_t s = 0x3400 | 0x0200 | 0x04;
    if (vdp.vint_flag)
        s |= 0x80;
    if (vdp.in_vblank || !(vdp.reg[1] & 0x40))
        s |= 0x08;
    return s;
}
