/* Online 2-player games over UDP (LAN or the internet) - see net.c. */
#ifndef NET_H
#define NET_H
#include <stdint.h>

#define NET_DEFAULT_PORT 7777

enum { NET_OFF, NET_HOSTING, NET_JOINING, NET_PLAYING };

int  net_available(void);                 /* sockets work on this platform */
int  net_host(int port);                  /* wait for player 2; 0 on failure (see net_status) */
int  net_join(const char *address);       /* "host" or "host:port" */
void net_stop(void);
void net_poll(void);                      /* call often: receive, resend, time out */
int  net_state(void);
int  net_local_player(void);              /* 0 = player 1 (host), 1 = player 2 */
const char *net_status(void);             /* one line for the menu / title */

/* Lockstep. Both machines run the same game; only button states are exchanged. */
int  net_take_start(void);                /* 1 once when a game starts: reset the machine */
int  net_option_floor_alert(void);        /* game options chosen by the host */
int  net_option_prop_hunt(void);          /* Prop Hunt (only while playing online) */
int  net_option_ph_minutes(void);         /* Prop Hunt: minutes a round lasts */
void net_local_input(uint8_t pad);        /* this player's buttons, every refresh */
int  net_inputs_for_next_frame(uint8_t pads[2]); /* 1 if the next frame can run */
void net_frame_done(uint32_t state_hash); /* after running that frame */
int  net_frame(void);                     /* frames run in this game */
int  net_frames_waiting(void);            /* how far the other player is ahead (catch up) */

/* LAN: games being hosted on the local network */
void net_lan_search(void);
int  net_lan_count(void);
const char *net_lan_entry(int i);         /* "name  address:port" */
const char *net_lan_address(int i);       /* "address:port" to join */

const char *net_local_address(void);      /* this machine's LAN address */

#endif
