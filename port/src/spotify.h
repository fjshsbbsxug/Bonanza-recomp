/* Is Spotify playing music on this computer? (spotify.c; Windows, Linux, macOS) */
#ifndef SPOTIFY_H
#define SPOTIFY_H

/* on: look every two seconds (on a thread of its own: the answer comes from another
 * program); off: stop looking */
void spotify_watch(int on);
/* 1 while it is playing (the last answer) */
int spotify_playing(void);

#endif
