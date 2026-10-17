/* The picture drawn on the graphics card (OpenGL ES 3 / WebGL 2 / OpenGL 3.3), from the
 * frame's scene (scene.h). */
#ifndef RENDER_GL_H
#define RENDER_GL_H
#include <stdint.h>

/* after a GL context is current: getproc finds GL functions (SDL_GL_GetProcAddress,
 * eglGetProcAddress); es: an OpenGL ES / WebGL context (else desktop OpenGL 3.3).
 * 0 when it works; then the scene is filled every frame (scene_wanted). */
int  render_gl_init(void *(*getproc)(const char *name), int es);
void render_gl_quit(void);

/* this frame's picture (after md_run_frame), into the texture below; 0 if the scene
 * could not be drawn here (then the frame was drawn on the CPU: vdp.fbmd) */
int  render_gl_frame(void);
unsigned render_gl_texture(void);           /* RGBA, VDP_FB_W x 224 (rows top down) */
const char *render_gl_error(void);

/* drawing the window, for a frontend that draws with OpenGL itself: window pixels, y down */
void render_gl_screen_begin(int w, int h);          /* the window's frame buffer, cleared black */
/* this frame's picture (its first src_w columns) into a rectangle of the window */
void render_gl_screen_picture(int src_w, int x, int y, int w, int h, int linear);
/* an image the CPU made (0xAARRGGBB words, iw x ih, its top-left src_w x src_h shown);
 * slot: which of 4 textures holds it */
void render_gl_screen_image(int slot, const uint32_t *argb, int iw, int ih, int src_w, int src_h,
                            int x, int y, int w, int h, int linear, int blend);
void render_gl_screen_fill(int x, int y, int w, int h, uint32_t argb);   /* see-through by its alpha */

/* the colour codes it drew (tests): VDP_FB_W x 224 */
void render_gl_read_codes(uint16_t *out);

#endif
