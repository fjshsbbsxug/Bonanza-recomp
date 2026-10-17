/* The picture drawn on the graphics card (see render_gl.h).
 *
 * Each frame the scene (scene.h) goes up as a few small textures: video memory, one
 * row per line (its plan, palette, scroll, the sprites and pieces it shows), the
 * sprites, the port's pieces and, during play, the level map. A fragment shader then
 * works out every pixel's colour code exactly as render.c's drawing does (the same
 * planes, window, sprites, player colours, shadow / highlight, sides), into a 16-bit
 * texture; a second pass turns codes into colours (the colour settings' table). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_gl.h"
#include "scene.h"
#include "settings.h"
#include "vdp.h"

/* ---- the GL functions used (loaded at init: no GL headers or libraries needed) */
typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef unsigned char GLboolean;
typedef char GLchar;
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_R8UI 0x8232
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_RGBA8 0x8058
#define GL_RGBA 0x1908
#define GL_RED_INTEGER 0x8D94
#define GL_RGBA_INTEGER 0x8D99
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TRIANGLES 0x0004
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_VIEWPORT 0x0BA2
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_LINEAR 0x2601
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TRIANGLE_STRIP 0x0005

#define GLFUNCS \
    F(GLuint, glCreateShader, (GLenum)) \
    F(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    F(void, glCompileShader, (GLuint)) \
    F(void, glGetShaderiv, (GLuint, GLenum, GLint *)) \
    F(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
    F(void, glDeleteShader, (GLuint)) \
    F(GLuint, glCreateProgram, (void)) \
    F(void, glAttachShader, (GLuint, GLuint)) \
    F(void, glLinkProgram, (GLuint)) \
    F(void, glGetProgramiv, (GLuint, GLenum, GLint *)) \
    F(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
    F(void, glDeleteProgram, (GLuint)) \
    F(void, glUseProgram, (GLuint)) \
    F(GLint, glGetUniformLocation, (GLuint, const GLchar *)) \
    F(void, glUniform1i, (GLint, GLint)) \
    F(void, glGenTextures, (GLsizei, GLuint *)) \
    F(void, glDeleteTextures, (GLsizei, const GLuint *)) \
    F(void, glBindTexture, (GLenum, GLuint)) \
    F(void, glActiveTexture, (GLenum)) \
    F(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    F(void, glTexParameteri, (GLenum, GLenum, GLint)) \
    F(void, glPixelStorei, (GLenum, GLint)) \
    F(void, glGenFramebuffers, (GLsizei, GLuint *)) \
    F(void, glDeleteFramebuffers, (GLsizei, const GLuint *)) \
    F(void, glBindFramebuffer, (GLenum, GLuint)) \
    F(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    F(GLenum, glCheckFramebufferStatus, (GLenum)) \
    F(void, glGenVertexArrays, (GLsizei, GLuint *)) \
    F(void, glDeleteVertexArrays, (GLsizei, const GLuint *)) \
    F(void, glBindVertexArray, (GLuint)) \
    F(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
    F(void, glGetIntegerv, (GLenum, GLint *)) \
    F(void, glDisable, (GLenum)) \
    F(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
    F(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    F(void, glEnable, (GLenum)) \
    F(void, glBlendFunc, (GLenum, GLenum)) \
    F(void, glClearColor, (float, float, float, float)) \
    F(void, glClear, (GLbitfield)) \
    F(void, glUniform4f, (GLint, float, float, float, float))

#define F(ret, name, args) static ret (*p_##name) args;
GLFUNCS
#undef F

/* ---- the shaders */
static const char *vs_src =
    "void main() {\n"
    "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

/* the window: a rectangle of it (u_dst, in -1..1) showing part of an image (u_src) */
static const char *vs_quad =
    "uniform vec4 u_dst, u_src;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    vec2 c = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
    "    gl_Position = vec4(mix(u_dst.xy, u_dst.zw, c), 0.0, 1.0);\n"
    "    v_uv = mix(u_src.xy, u_src.zw, c);\n"
    "}\n";

static const char *fs_image =
    "uniform sampler2D u_img;\n"
    "uniform int u_bgra;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    vec4 c = texture(u_img, v_uv);\n"
    "    o_color = u_bgra != 0 ? c.bgra : c;\n"
    "}\n";

static const char *fs_fill =
    "uniform vec4 u_color;\n"
    "out vec4 o_color;\n"
    "void main() { o_color = u_color; }\n";

/* the colour codes: render.c's draw_line, pixel by pixel */
static const char *fs_codes =
    "uniform highp usampler2D u_vram;\n"
    "uniform highp isampler2D u_line, u_spr, u_piece;\n"
    "uniform highp usampler2D u_map, u_roof;\n"
    "uniform int u_map_w, u_roof_row, u_char_on;\n"
    "out uvec4 o_code;\n"
    "const int BLACK = 0x200, CHAR_BASE = 0x400, SHADOW = 0x1000, HIGHLIGHT = 0x2000;\n"
    "const int SL_OFF = 1, SL_A_MAP = 2, SL_A_MAP_ALL = 4, SL_B_WRAP = 8, SL_ROOM_ABOVE = 16,\n"
    "          SL_SPRITES = 32, SL_STACKED = 64, SL_CROP = 128, SL_CLAMP = 256, SL_SHADE = 512,\n"
    "          SL_COLUMN_VS = 1024, SL_WINDOW = 2048;\n"
    "int vb(int a) { a &= 0xFFFF; return int(texelFetch(u_vram, ivec2(a & 255, a >> 8), 0).r); }\n"
    "int L(int y, int i) { return texelFetch(u_line, ivec2(i, y), 0).r; }\n"
    "int S(int k, int i) { return texelFetch(u_spr, ivec2(i, k), 0).r; }\n"
    "int Pc(int list, int idx, int i) { return texelFetch(u_piece, ivec2(i, list * 256 + idx), 0).r; }\n"
    "int tile_pixel(int entry, int px, int py) {\n"
    "    if ((entry & 0x1000) != 0) py = 7 - py;\n"
    "    if ((entry & 0x0800) != 0) px = 7 - px;\n"
    "    int b = vb(((entry & 0x07FF) << 5) + (py << 2) + (px >> 1));\n"
    "    int c = (px & 1) != 0 ? (b & 15) : (b >> 4);\n"
    "    if (c == 0) return (entry >> 8) & 0x80;\n"
    "    return c | ((entry >> 9) & 0x30) | ((entry >> 8) & 0x80);\n"
    "}\n"
    "int map_entry(int wx, int wy) {\n"
    "    if (wx < 0 || wy < 0 || wx >= u_map_w * 16 || wy >= 64 * 16) return 0xFFFF;\n"
    "    return int(texelFetch(u_map, ivec2(wx >> 3, wy >> 3), 0).r);\n"
    "}\n"
    "bool above_roof(int wx, int wy) {\n"
    "    if (wy >= u_roof_row * 16) return false;\n"
    "    if (wy < 0 || wx < 0 || wx >= u_map_w * 16) return true;\n"
    "    return texelFetch(u_roof, ivec2(wx >> 4, wy >> 4), 0).r != 0u;\n"
    "}\n"
    "int plane_px(int y, int x, int pb, int ovrhs, int ovrvs, bool wrap, bool mapon, bool mapall, int mcamx, int mwy) {\n"
    "    int width = L(y, 1);\n"
    "    if (mapon && pb == 0 && (mapall || x < 16 || x >= width - 16)) {\n"
    "        int wx = mcamx + x;\n"
    "        int me = map_entry(wx, mwy);\n"
    "        return me == 0xFFFF ? 0 : tile_pixel(me, wx & 7, mwy & 7);\n"
    "    }\n"
    "    int pw = L(y, 14), ph = L(y, 15);\n"
    "    int name = pb != 0 ? L(y, 17) : L(y, 16);\n"
    "    int hs = L(y, 12 + pb);\n"
    "    bool colvs = (L(y, 0) & SL_COLUMN_VS) != 0;\n"
    "    if (ovrhs >= 0) { hs = ovrhs; colvs = false; }\n"
    "    int vs = ovrvs >= 0 ? ovrvs : L(y, 96 + pb);\n"
    "    if (colvs) {\n"
    "        int col = x < 0 ? 0 : (x >> 4);\n"
    "        if (col > (width >> 4) - 1) col = (width >> 4) - 1;\n"
    "        vs = L(y, 96 + col * 2 + pb);\n"
    "    }\n"
    "    int sx = x;\n"
    "    if (wrap && (x < 0 || x >= width)) sx = x < 0 ? x + width : x - width;\n"
    "    int px = (sx - hs) & (pw * 8 - 1), py = (y + vs) & (ph * 8 - 1);\n"
    "    int ea = name + (((py >> 3) * pw + (px >> 3)) << 1);\n"
    "    return tile_pixel((vb(ea) << 8) | vb(ea + 1), px & 7, py & 7);\n"
    "}\n"
    "int window_px(int y, int x, out bool inwin) {\n"
    "    int width = L(y, 1), r17 = L(y, 19), r18 = L(y, 20);\n"
    "    int ww = width == 320 ? 64 : 32;\n"
    "    int vp = (r18 & 0x1F) * 8, hp = (r17 & 0x1F) * 16;\n"
    "    bool vin = (r18 & 0x80) != 0 ? (y >= vp) : (y < vp);\n"
    "    int cx = x < 0 ? 0 : (x >= width ? width - 1 : x);\n"
    "    bool hin = (r17 & 0x80) != 0 ? (cx >= hp) : (cx < hp);\n"
    "    inwin = vin || hin;\n"
    "    if (!inwin) return 0;\n"
    "    int wx = x & (ww * 8 - 1);\n"
    "    int ea = L(y, 18) + (((y >> 3) * ww + (wx >> 3)) << 1);\n"
    "    return tile_pixel((vb(ea) << 8) | vb(ea + 1), wx & 7, y & 7);\n"
    "}\n"
    "int char_part(int ch, int c, int row, int lx, int k) {\n"
    "    int light = ch == 0 ? 9 : 0xB, dark = ch == 0 ? 0xA : 0xC;\n"
    "    int base = ch * 8 + 1;\n"
    "    bool shirt = ((u_char_on >> (ch * 3)) & 1) != 0, pants = ((u_char_on >> (ch * 3 + 1)) & 1) != 0,\n"
    "         skin = ((u_char_on >> (ch * 3 + 2)) & 1) != 0;\n"
    "    bool in_body = row >= 0 && row < 32 && lx >= 0 && lx < 32 && ((S(k, 8 + row) >> lx) & 1) != 0;\n"
    "    if (c == light && shirt && in_body) return base + 0;\n"
    "    if (c == dark && shirt && in_body) return base + 1;\n"
    "    if (c == 0xD && skin) return base + 3;\n"
    "    if (c == 0xE && skin) return base + 4;\n"
    "    if (c == 8 && pants && row > S(k, 6) && row <= S(k, 7)) return base + 2;\n"
    "    return 0;\n"
    "}\n"
    "int compose(int y, int X) {\n"
    "    int flags = L(y, 0), width = L(y, 1), m = L(y, 2);\n"
    "    int x = X - m;\n"
    "    int a, b;\n"
    "    if ((flags & SL_ROOM_ABOVE) == 0) {\n"
    "        a = plane_px(y, x, 0, -1, -1, false, (flags & SL_A_MAP) != 0, (flags & SL_A_MAP_ALL) != 0, L(y, 3), L(y, 4));\n"
    "        b = plane_px(y, x, 1, -1, -1, (flags & SL_B_WRAP) != 0, false, false, 0, 0);\n"
    "    } else {\n"
    "        b = plane_px(y, x, 1, L(y, 5), L(y, 6), m != 0, false, false, 0, 0);\n"
    "        a = plane_px(y, x, 0, -1, -1, false, true, true, L(y, 7), L(y, 8));\n"
    "        if (above_roof(L(y, 7) + x, L(y, 8)))\n"
    "            b = plane_px(y, x, 1, L(y, 9), L(y, 10), m != 0, false, false, 0, 0);\n"
    "    }\n"
    "    if ((flags & SL_WINDOW) != 0) {\n"
    "        bool inw;\n"
    "        int w = window_px(y, x, inw);\n"
    "        if (inw) a = w;\n"
    "    }\n"
    "    int s = 0, part = 0;\n"
    "    int sy_line = L(y, 11);\n"
    "    if ((flags & SL_SPRITES) != 0) {\n"
    "        int n = L(y, 22);\n"
    "        for (int i = 0; i < n; i++) {\n"
    "            if ((s & 15) != 0) break;\n"
    "            int pk = L(y, 136 + i), k = pk & 255, cols = pk >> 8;\n"
    "            int lx = x - (S(k, 0) - 128);\n"
    "            if (lx < 0 || lx >= cols) continue;\n"
    "            int hsz = S(k, 2), vsz = S(k, 3), attr = S(k, 4);\n"
    "            int row = sy_line + 128 - S(k, 1);\n"
    "            int cx = lx >> 3, px = lx & 7, cy = row >> 3, py = row & 7;\n"
    "            if ((attr & 0x1000) != 0) { cy = vsz - 1 - cy; py = 7 - py; }\n"
    "            int tcx = (attr & 0x0800) != 0 ? hsz - 1 - cx : cx;\n"
    "            int tile = (attr & 0xF800) | ((attr + tcx * vsz + cy) & 0x07FF);\n"
    "            s = tile_pixel(tile & ~0x1000, px, py);\n"
    "            int ch = S(k, 5);\n"
    "            if (ch >= 0 && (s & 15) != 0) part = char_part(ch, s & 15, row, lx, k);\n"
    "        }\n"
    "    }\n"
    "    int nw = L(y, 23);\n"
    "    for (int i = 0; i < nw; i++) {\n"
    "        if ((s & 15) != 0) break;\n"
    "        int ref = L(y, 160 + i), list = ref >> 10, idx = ref & 1023;\n"
    "        int lx = x - Pc(list, idx, 0);\n"
    "        int hsz = Pc(list, idx, 2), vsz = Pc(list, idx, 3), attr = Pc(list, idx, 6);\n"
    "        if (lx < 0 || lx >= hsz * 8) continue;\n"
    "        if (Pc(list, idx, 7) != 0 && x >= 0 && x < width) continue;\n"
    "        int row = (list == 2 ? y : sy_line) - Pc(list, idx, 1);\n"
    "        int cy = row >> 3, py = row & 7;\n"
    "        if ((attr & 0x1000) != 0) { cy = vsz - 1 - cy; py = 7 - py; }\n"
    "        int cx = lx >> 3, tcx = (attr & 0x0800) != 0 ? hsz - 1 - cx : cx;\n"
    "        s = tile_pixel((attr & 0xE800) | ((attr + tcx * vsz + cy) & 0x07FF), lx & 7, py);\n"
    "    }\n"
    "    int bg = L(y, 21);\n"
    "    int sp = s, ap = a, bp = b;\n"
    "    if ((flags & SL_SHADE) != 0) {\n"
    "        int shade = ((ap & 0x80) != 0 || (bp & 0x80) != 0) ? 0 : SHADOW;\n"
    "        int under; bool plane_prio = false;\n"
    "        if ((ap & 0x80) != 0 && (ap & 15) != 0) { under = L(y, 32 + (ap & 0x3F)); plane_prio = true; }\n"
    "        else if ((bp & 0x80) != 0 && (bp & 15) != 0) { under = L(y, 32 + (bp & 0x3F)); plane_prio = true; }\n"
    "        else if ((ap & 15) != 0) under = L(y, 32 + (ap & 0x3F));\n"
    "        else if ((bp & 15) != 0) under = L(y, 32 + (bp & 0x3F));\n"
    "        else under = bg;\n"
    "        int c = sp & 0x3F;\n"
    "        if ((sp & 15) != 0 && (c == 0x3E || c == 0x3F)) {\n"
    "            if (c == 0x3F) shade = SHADOW; else shade = shade != 0 ? 0 : HIGHLIGHT;\n"
    "            return under | shade;\n"
    "        } else if ((sp & 15) != 0 && ((sp & 0x80) != 0 || !plane_prio)) {\n"
    "            int col = part != 0 ? CHAR_BASE + part - 1 : L(y, 32 + c);\n"
    "            return col | (((sp & 0x80) != 0 || (sp & 15) == 0x0E) ? 0 : shade);\n"
    "        }\n"
    "        return under | shade;\n"
    "    }\n"
    "    if ((sp & 0x80) != 0 && (sp & 15) != 0) return part != 0 ? CHAR_BASE + part - 1 : L(y, 32 + (sp & 0x3F));\n"
    "    if ((ap & 0x80) != 0 && (ap & 15) != 0) return L(y, 32 + (ap & 0x3F));\n"
    "    if ((bp & 0x80) != 0 && (bp & 15) != 0) return L(y, 32 + (bp & 0x3F));\n"
    "    if ((sp & 15) != 0) return part != 0 ? CHAR_BASE + part - 1 : L(y, 32 + (sp & 0x3F));\n"
    "    if ((ap & 15) != 0) return L(y, 32 + (ap & 0x3F));\n"
    "    if ((bp & 15) != 0) return L(y, 32 + (bp & 0x3F));\n"
    "    return bg;\n"
    "}\n"
    "void main() {\n"
    "    int X = int(gl_FragCoord.x), R = int(gl_FragCoord.y);\n"
    "    int y = L(R, 24);\n"
    "    int flags = L(y, 0), code;\n"
    "    if ((flags & SL_OFF) != 0) {\n"
    "        code = L(y, 21);\n"
    "    } else {\n"
    "        int width = L(y, 1), m = L(y, 2);\n"
    "        if (X >= width + 2 * m) code = BLACK;\n"
    "        else if ((flags & SL_CROP) != 0 && (X < m || X >= m + width)) code = BLACK;\n"
    "        else if ((flags & SL_CLAMP) != 0 && X < m) {\n"
    "            int l = compose(y, m), l1 = compose(y, m + 1), l2 = compose(y, m + 2);\n"
    "            code = (l != l1 && l1 == l2) ? l1 : l;\n"
    "        } else if ((flags & SL_CLAMP) != 0 && X >= m + width) {\n"
    "            int r = compose(y, m + width - 1), r1 = compose(y, m + width - 2), r2 = compose(y, m + width - 3);\n"
    "            code = (r != r1 && r1 == r2) ? r1 : r;\n"
    "        } else code = compose(y, X);\n"
    "    }\n"
    "    o_code = uvec4(uint(code), 0u, 0u, 0u);\n"
    "}\n";

/* codes to colours: md.c's md_framebuffer */
static const char *fs_colors =
    "uniform highp usampler2D u_codes;\n"
    "uniform highp sampler2D u_lut;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    int c = int(texelFetch(u_codes, ivec2(gl_FragCoord.xy), 0).r);\n"
    "    vec4 rgb = texelFetch(u_lut, ivec2(c & 0x0FFF, 0), 0);\n"
    "    ivec3 v = ivec3(rgb.rgb * 255.0 + 0.5);\n"
    "    if ((c & 0x1000) != 0) v = v >> 1;\n"
    "    else if ((c & 0x2000) != 0) v = (v >> 1) + 0x7F;\n"
    "    o_color = vec4(vec3(v) / 255.0, 1.0);\n"
    "}\n";

/* ---- state */
#define LINE_W 256
#define LUT_W 1040
static int gl_ok, gl_es;
static GLuint prog_codes, prog_colors, vao, fbo_codes, fbo_colors;
static GLuint prog_image, prog_fill;
#define IMAGES 4
static GLuint tex_image[IMAGES];
static int win_w, win_h;
static GLuint tex_vram, tex_line, tex_spr, tex_piece, tex_map, tex_roof, tex_codes, tex_colors, tex_lut;
static uint32_t map_gen_up = 0xFFFFFFFFu;
static int map_w_up = -1;
static char err[512];
static int32_t line_tab[SCENE_LINES][LINE_W];
static int32_t spr_tab[SCENE_SPRITES][40];
static int32_t piece_tab[3 * SCENE_LIST][8];
static uint8_t lut[LUT_W][4];             /* R, G, B, A */

const char *render_gl_error(void) { return err; }

static GLuint compile(GLenum kind, const char *body)
{
    const char *head = gl_es
        ? "#version 300 es\nprecision highp float;\nprecision highp int;\n"
        : "#version 330 core\n";
    const char *src[2] = {head, body};
    GLuint s = p_glCreateShader(kind);
    p_glShaderSource(s, 2, src, NULL);
    p_glCompileShader(s);
    GLint ok = 0;
    p_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        p_glGetShaderInfoLog(s, sizeof(err), NULL, err);
        p_glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint program2(const char *vs, const char *fs)
{
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = v ? compile(GL_FRAGMENT_SHADER, fs) : 0;
    if (!v || !f) return 0;
    GLuint p = p_glCreateProgram();
    p_glAttachShader(p, v);
    p_glAttachShader(p, f);
    p_glLinkProgram(p);
    p_glDeleteShader(v);
    p_glDeleteShader(f);
    GLint ok = 0;
    p_glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        p_glGetProgramInfoLog(p, sizeof(err), NULL, err);
        p_glDeleteProgram(p);
        return 0;
    }
    return p;
}

static GLuint program(const char *fs) { return program2(vs_src, fs); }

static GLuint texture(void)
{
    GLuint t;
    p_glGenTextures(1, &t);
    p_glBindTexture(GL_TEXTURE_2D, t);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static void upload(GLuint t, GLint ifmt, int w, int h, GLenum fmt, GLenum type, const void *data)
{
    p_glBindTexture(GL_TEXTURE_2D, t);
    p_glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, data);
}

static GLuint target(GLuint tex, GLint ifmt, GLenum fmt, GLenum type)
{
    upload(tex, ifmt, VDP_FB_W, SCENE_LINES, fmt, type, NULL);
    GLuint f;
    p_glGenFramebuffers(1, &f);
    p_glBindFramebuffer(GL_FRAMEBUFFER, f);
    p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        snprintf(err, sizeof(err), "a frame buffer could not be made");
        return 0;
    }
    return f;
}

int render_gl_init(void *(*getproc)(const char *name), int es)
{
    gl_es = es;
    err[0] = 0;
#define F(ret, name, args) \
    if (!(p_##name = (ret (*) args)getproc(#name))) { snprintf(err, sizeof(err), "no %s", #name); return 1; }
    GLFUNCS
#undef F
    prog_codes = program(fs_codes);
    prog_colors = prog_codes ? program(fs_colors) : 0;
    prog_image = prog_colors ? program2(vs_quad, fs_image) : 0;
    prog_fill = prog_image ? program2(vs_quad, fs_fill) : 0;
    if (!prog_fill) return 1;
    p_glGenVertexArrays(1, &vao);
    tex_vram = texture(); tex_line = texture(); tex_spr = texture(); tex_piece = texture();
    tex_map = texture(); tex_roof = texture(); tex_codes = texture(); tex_colors = texture();
    tex_lut = texture();
    for (int i = 0; i < IMAGES; i++) tex_image[i] = texture();
    static const uint8_t zero[4];
    upload(tex_map, GL_R16UI, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_SHORT, zero);
    upload(tex_roof, GL_R8UI, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, zero);
    GLint prev = 0;
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    fbo_codes = target(tex_codes, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
    fbo_colors = fbo_codes ? target(tex_colors, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE) : 0;
    p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
    if (!fbo_colors) return 1;
    p_glUseProgram(prog_codes);
    static const char *names[] = {"u_vram", "u_line", "u_spr", "u_piece", "u_map", "u_roof"};
    for (int i = 0; i < 6; i++) p_glUniform1i(p_glGetUniformLocation(prog_codes, names[i]), i);
    p_glUseProgram(prog_colors);
    p_glUniform1i(p_glGetUniformLocation(prog_colors, "u_codes"), 0);
    p_glUniform1i(p_glGetUniformLocation(prog_colors, "u_lut"), 1);
    p_glUseProgram(prog_image);
    p_glUniform1i(p_glGetUniformLocation(prog_image, "u_img"), 0);
    p_glUseProgram(0);
    map_gen_up = 0xFFFFFFFFu;
    gl_ok = 1;
    scene_wanted = 1;
    return 0;
}

void render_gl_quit(void)
{
    if (!gl_ok) return;
    GLuint t[] = {tex_vram, tex_line, tex_spr, tex_piece, tex_map, tex_roof, tex_codes, tex_colors, tex_lut};
    p_glDeleteTextures(9, t);
    p_glDeleteTextures(IMAGES, tex_image);
    p_glDeleteProgram(prog_image);
    p_glDeleteProgram(prog_fill);
    GLuint f[] = {fbo_codes, fbo_colors};
    p_glDeleteFramebuffers(2, f);
    p_glDeleteVertexArrays(1, &vao);
    p_glDeleteProgram(prog_codes);
    p_glDeleteProgram(prog_colors);
    gl_ok = 0;
    scene_wanted = 0;
}

unsigned render_gl_texture(void) { return tex_colors; }

/* the scene into the tables the shader reads */
static void tables(void)
{
    for (int y = 0; y < SCENE_LINES; y++) {
        const scene_line *L = &scene.line[y];
        int32_t *t = line_tab[y];
        memset(t, 0, sizeof(line_tab[y]));
        t[0] = (int32_t)L->flags; t[1] = L->width; t[2] = L->m;
        t[3] = L->a_camx; t[4] = L->a_wy; t[5] = L->ra_hs; t[6] = L->ra_vs;
        t[7] = L->ra_camx; t[8] = L->ra_wy; t[9] = L->sky_hs; t[10] = L->sky_vs;
        t[11] = L->spr_y; t[12] = L->hs[0]; t[13] = L->hs[1]; t[14] = L->pw; t[15] = L->ph;
        t[16] = L->name_a; t[17] = L->name_b; t[18] = L->name_w; t[19] = L->reg17; t[20] = L->reg18;
        t[21] = L->bg; t[22] = L->npick; t[23] = L->nwobj;
        t[24] = scene.src_line[y];              /* (this row of the output: its line) */
        for (int i = 0; i < 64; i++) t[32 + i] = L->pal[i];
        for (int i = 0; i < 40; i++) t[96 + i] = L->vsram[i];
        for (int i = 0; i < L->npick; i++) t[136 + i] = L->pick[i];
        for (int i = 0; i < L->nwobj; i++) t[160 + i] = L->wobj[i];
    }
    for (int k = 0; k < scene.nsprites; k++) {
        const scene_sprite *s = &scene.sprite[k];
        int32_t *t = spr_tab[k];
        t[0] = s->sx; t[1] = s->sy; t[2] = s->hsz; t[3] = s->vsz; t[4] = s->attr;
        t[5] = s->ch; t[6] = s->shirt_end; t[7] = s->pants_end;
        for (int r = 0; r < 32; r++) t[8 + r] = (int32_t)s->body[r];
    }
    for (int l = 0; l < 3; l++)
        for (int i = 0; i < scene.npieces[l]; i++) {
            const scene_piece *p = &scene.piece[l][i];
            int32_t *t = piece_tab[l * SCENE_LIST + i];
            t[0] = p->x; t[1] = p->y; t[2] = p->hsz; t[3] = p->vsz;
            t[4] = p->ymin; t[5] = p->ymax; t[6] = p->attr; t[7] = p->only_outside;
        }
    for (int k = 0; k < LUT_W; k++) {
        uint32_t c = k < 512 ? md_color_lut[k] : k >= VDP_CHAR_BASE ? char_color_lut[(k - VDP_CHAR_BASE) & 15] : 0xFF000000u;
        lut[k][0] = (uint8_t)(c >> 16); lut[k][1] = (uint8_t)(c >> 8); lut[k][2] = (uint8_t)c; lut[k][3] = 255;
    }
}

int render_gl_frame(void)
{
    if (!gl_ok || !scene.valid)
        return 0;
    tables();
    GLint prev = 0, vp[4];
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    p_glGetIntegerv(GL_VIEWPORT, vp);
    p_glDisable(GL_BLEND);
    p_glDisable(GL_SCISSOR_TEST);
    p_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    p_glActiveTexture(GL_TEXTURE0 + 0);
    upload(tex_vram, GL_R8UI, 256, 256, GL_RED_INTEGER, GL_UNSIGNED_BYTE, scene.vram);
    p_glActiveTexture(GL_TEXTURE0 + 1);
    upload(tex_line, GL_R32I, LINE_W, SCENE_LINES, GL_RED_INTEGER, GL_INT, line_tab);
    p_glActiveTexture(GL_TEXTURE0 + 2);
    upload(tex_spr, GL_R32I, 40, SCENE_SPRITES, GL_RED_INTEGER, GL_INT, spr_tab);
    p_glActiveTexture(GL_TEXTURE0 + 3);
    upload(tex_piece, GL_R32I, 8, 3 * SCENE_LIST, GL_RED_INTEGER, GL_INT, piece_tab);
    p_glActiveTexture(GL_TEXTURE0 + 4);
    p_glBindTexture(GL_TEXTURE_2D, tex_map);
    if (scene.map_w && (scene.map_gen != map_gen_up || scene.map_w != map_w_up)) {
        upload(tex_map, GL_R16UI, scene.map_w * 2, 128, GL_RED_INTEGER, GL_UNSIGNED_SHORT, scene.map_cells);
        p_glActiveTexture(GL_TEXTURE0 + 5);
        upload(tex_roof, GL_R8UI, scene.map_w, 64, GL_RED_INTEGER, GL_UNSIGNED_BYTE, scene.roof_cells);
        map_gen_up = scene.map_gen;
        map_w_up = scene.map_w;
    }
    p_glActiveTexture(GL_TEXTURE0 + 5);
    p_glBindTexture(GL_TEXTURE_2D, tex_roof);
    p_glBindVertexArray(vao);
    p_glViewport(0, 0, VDP_FB_W, SCENE_LINES);
    /* the codes */
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo_codes);
    p_glUseProgram(prog_codes);
    p_glUniform1i(p_glGetUniformLocation(prog_codes, "u_map_w"), scene.map_w);
    p_glUniform1i(p_glGetUniformLocation(prog_codes, "u_roof_row"), scene.roof_row);
    int on = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 3; p++)
            if (scene.char_on[c][p]) on |= 1 << (c * 3 + p);
    p_glUniform1i(p_glGetUniformLocation(prog_codes, "u_char_on"), on);
    p_glDrawArrays(GL_TRIANGLES, 0, 3);
    /* the colours */
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo_colors);
    p_glUseProgram(prog_colors);
    p_glActiveTexture(GL_TEXTURE0 + 0);
    p_glBindTexture(GL_TEXTURE_2D, tex_codes);
    p_glActiveTexture(GL_TEXTURE0 + 1);
    upload(tex_lut, GL_RGBA8, LUT_W, 1, GL_RGBA, GL_UNSIGNED_BYTE, lut);
    p_glDrawArrays(GL_TRIANGLES, 0, 3);
    p_glUseProgram(0);
    p_glBindVertexArray(0);
    p_glActiveTexture(GL_TEXTURE0);
    p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
    p_glViewport(vp[0], vp[1], vp[2], vp[3]);
    return 1;
}

void render_gl_read_codes(uint16_t *out)
{
    static uint32_t px[SCENE_LINES][VDP_FB_W][4];
    GLint prev = 0;
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo_codes);
    p_glPixelStorei(GL_PACK_ALIGNMENT, 4);
    p_glReadPixels(0, 0, VDP_FB_W, SCENE_LINES, GL_RGBA_INTEGER, GL_UNSIGNED_INT, px);
    p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
    for (int y = 0; y < SCENE_LINES; y++)
        for (int x = 0; x < VDP_FB_W; x++)
            out[y * VDP_FB_W + x] = (uint16_t)px[y][x][0];
}

/* ---- the window */
void render_gl_screen_begin(int w, int h)
{
    win_w = w > 0 ? w : 1;
    win_h = h > 0 ? h : 1;
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    p_glViewport(0, 0, win_w, win_h);
    p_glDisable(GL_SCISSOR_TEST);
    p_glClearColor(0, 0, 0, 1);
    p_glClear(GL_COLOR_BUFFER_BIT);
    p_glEnable(GL_BLEND);
    p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void quad(GLuint prog, int x, int y, int w, int h, float u0, float v0, float u1, float v1)
{
    /* window pixels (y down) -> -1..1 (y up) */
    float x0 = 2.0f * x / win_w - 1, x1 = 2.0f * (x + w) / win_w - 1;
    float y0 = 1 - 2.0f * y / win_h, y1 = 1 - 2.0f * (y + h) / win_h;
    p_glUseProgram(prog);
    p_glUniform4f(p_glGetUniformLocation(prog, "u_dst"), x0, y0, x1, y1);
    p_glUniform4f(p_glGetUniformLocation(prog, "u_src"), u0, v0, u1, v1);
    p_glBindVertexArray(vao);
    p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    p_glBindVertexArray(0);
}

static void filter(int linear)
{
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
}

void render_gl_screen_picture(int src_w, int x, int y, int w, int h, int linear)
{
    p_glActiveTexture(GL_TEXTURE0);
    p_glBindTexture(GL_TEXTURE_2D, tex_colors);
    filter(linear);
    p_glUseProgram(prog_image);
    p_glUniform1i(p_glGetUniformLocation(prog_image, "u_bgra"), 0);
    p_glDisable(GL_BLEND);
    quad(prog_image, x, y, w, h, 0, 0, (float)src_w / VDP_FB_W, 1);
    p_glEnable(GL_BLEND);
}

void render_gl_screen_image(int slot, const uint32_t *argb, int iw, int ih, int src_w, int src_h,
                            int x, int y, int w, int h, int linear, int blend)
{
    if (slot < 0 || slot >= IMAGES) return;
    p_glActiveTexture(GL_TEXTURE0);
    p_glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    upload(tex_image[slot], GL_RGBA8, iw, ih, GL_RGBA, GL_UNSIGNED_BYTE, argb);
    filter(linear);
    p_glUseProgram(prog_image);
    p_glUniform1i(p_glGetUniformLocation(prog_image, "u_bgra"), 1);
    if (!blend) p_glDisable(GL_BLEND);
    quad(prog_image, x, y, w, h, 0, 0, (float)src_w / iw, (float)src_h / ih);
    p_glEnable(GL_BLEND);
}

void render_gl_screen_fill(int x, int y, int w, int h, uint32_t argb)
{
    p_glUseProgram(prog_fill);
    p_glUniform4f(p_glGetUniformLocation(prog_fill, "u_color"), ((argb >> 16) & 255) / 255.0f,
                  ((argb >> 8) & 255) / 255.0f, (argb & 255) / 255.0f, (argb >> 24) / 255.0f);
    quad(prog_fill, x, y, w, h, 0, 0, 1, 1);
}
