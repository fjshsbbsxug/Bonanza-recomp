/* In-game options menu (screen shape, colours, recolouring). Drawn into an overlay
 * image by menu_draw(); the frontend scales it over the game picture. */
#ifndef MENU_H_INCLUDED
#define MENU_H_INCLUDED
#include <stdint.h>

#define MENU_W 320
#define MENU_H 240

enum { MK_UP, MK_DOWN, MK_LEFT, MK_RIGHT, MK_OK, MK_BACK };
enum { MENU_ACT_NONE, MENU_ACT_CLOSE, MENU_ACT_RESET, MENU_ACT_QUIT, MENU_ACT_MODS /* re-apply mods, restart */ };

extern int menu_allow_quit;     /* show the Quit entry (not in the browser) */
extern int menu_settings_dirty; /* settings changed since the menu was opened */

int  menu_is_open(void);
void menu_open(void);
int  menu_key(int key);                 /* returns MENU_ACT_* */
int  menu_pointer(int x, int y);        /* tap in overlay coordinates; returns MENU_ACT_* */
void menu_draw(uint32_t *buf);          /* MENU_W x MENU_H, ARGB with alpha */
int  menu_highlight(void);              /* Mega Drive colour being recoloured, or -1 */

/* the Online page's address box: while it is being edited the frontend sends typed
 * text here (and only Enter / Escape / Backspace as keys) */
int  menu_text_editing(void);
void menu_text_typed(const char *utf8);
void menu_text_backspace(void);

/* small text helper shared with the frontend (8x8 font) */
void menu_text(uint32_t *buf, int w, int h, int x, int y, const char *s, uint32_t color);

#endif
