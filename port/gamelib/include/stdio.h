#ifndef BG_STDIO_H
#define BG_STDIO_H
/* the engine's (given to the library when it is loaded) */
typedef struct bg_file FILE;
extern FILE *stderr;
extern int fprintf(FILE *f, const char *fmt, ...);
#endif
