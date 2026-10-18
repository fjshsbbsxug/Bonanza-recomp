/* Online 2-player games (LAN or the internet, the host's UDP port forwarded).
 *
 * Delay-based lockstep: both machines run the very same game - the port is
 * bit-identical everywhere - and only exchange each player's buttons per frame. A frame
 * runs once both players' buttons for it are known; each player's own buttons are
 * scheduled a few frames ahead (the input delay, chosen from the measured round trip)
 * so that normally nobody waits. Every packet repeats the recent buttons the other side
 * has not confirmed yet, so a lost packet costs nothing. Now and then the machines
 * compare a checksum of the whole game state to notice if they ever drift apart.
 *
 * Packets start with 'B' 'Z' and a type:
 *   H hello (player 2 -> host): version, ROM id, name
 *   W welcome (host -> player 2): version, game options, name
 *   X refused: reason
 *   I buttons: first frame, count, buttons..., frames received, checksum frame+value,
 *     send time, echoed time
 *   R race status (Race mode, race.c): whatever race.c sends, the last one kept
 *   Q leaving
 *   D / E: LAN search / answer (port, name) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "md.h"
#include "settings.h"

#define NET_VERSION 3      /* 2: the welcome carries the Prop Hunt options; 3: Race */
#define RING 1024
#define HASH_EVERY 60

#if defined(__EMSCRIPTEN__)
#define NET_NONE 1
#endif

static char status_text[200] = "Not connected";
static int state = NET_OFF, local_player;
static int start_pending, opt_floor_alert, opt_prop_hunt, opt_ph_minutes = 3, opt_race;
static uint8_t race_in[64];
static int race_in_n;
static char peer_name[32];

const char *net_status(void) { return status_text; }
int net_state(void) { return state; }
int net_local_player(void) { return local_player; }
int net_option_floor_alert(void) { return opt_floor_alert; }
int net_option_prop_hunt(void) { return state == NET_PLAYING && opt_prop_hunt; }
int net_option_ph_minutes(void) { return opt_ph_minutes; }
int net_option_race(void) { return state == NET_PLAYING && opt_race; }

int net_race_peer(uint8_t *data, int max)
{
    int n = race_in_n < max ? race_in_n : max;
    memcpy(data, race_in, (size_t)n);
    return n;
}

int net_take_start(void)
{
    int s = start_pending;
    start_pending = 0;
    return s;
}

#ifdef NET_NONE
int net_available(void) { return 0; }
int net_host(int port) { (void)port; snprintf(status_text, sizeof(status_text), "Online play needs the PC or Android version"); return 0; }
int net_join(const char *a) { (void)a; return net_host(0); }
void net_stop(void) {}
void net_poll(void) {}
void net_race_send(const uint8_t *d, int n) { (void)d; (void)n; }
void net_local_input(uint8_t pad) { (void)pad; }
int net_inputs_for_next_frame(uint8_t pads[2]) { (void)pads; return 0; }
int net_frames_waiting(void) { return 0; }
void net_frame_done(uint32_t h) { (void)h; }
int net_frame(void) { return 0; }
void net_lan_search(void) {}
int net_lan_count(void) { return 0; }
const char *net_lan_entry(int i) { (void)i; return ""; }
const char *net_lan_address(int i) { (void)i; return ""; }
const char *net_local_address(void) { return "-"; }
#else

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#define close_sock closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define close_sock close
#endif
#ifndef _WIN32
#include <time.h>
#endif

static sock_t sock = BAD_SOCK;
static struct sockaddr_in peer;
static uint32_t last_recv, last_send, last_hello;
static char my_name[32];

/* lockstep */
static uint32_t frame;               /* next frame to run */
static uint8_t in_local[RING], in_remote[RING];
static uint32_t local_next;          /* our buttons are known for frames < local_next */
static uint32_t remote_have;         /* their buttons are known for frames < remote_have */
static uint32_t peer_has;            /* they have our buttons for frames < peer_has */
static int delay = 3;
static uint32_t rtt_ms = 60, t_echo;
static uint32_t my_hash_frame, my_hash, their_hash_frame, their_hash;
static int desync;

/* LAN */
static struct { char entry[96], addr[64]; } lan[8];
static int lan_n;

static uint32_t now_ms(void)
{
#ifdef _WIN32
    return (uint32_t)GetTickCount();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000);
#endif
}

int net_available(void) { return 1; }
int net_frame(void) { return (int)frame; }

static int sockets_up(void)
{
#ifdef _WIN32
    static int done;
    if (!done) {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0;
        done = 1;
    }
#endif
    return 1;
}

static void set_nonblocking(sock_t s)
{
#ifdef _WIN32
    u_long one = 1;
    ioctlsocket(s, FIONBIO, &one);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static int open_socket(int port)
{
    if (sock != BAD_SOCK) { close_sock(sock); sock = BAD_SOCK; }
    if (!sockets_up()) return 0;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == BAD_SOCK) return 0;
    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(sock, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close_sock(sock);
        sock = BAD_SOCK;
        return 0;
    }
    set_nonblocking(sock);
    if (!my_name[0]) {
        if (gethostname(my_name, sizeof(my_name) - 1) != 0 || !my_name[0])
            snprintf(my_name, sizeof(my_name), "Bonanza");
        my_name[sizeof(my_name) - 1] = 0;
    }
    return 1;
}

static void send_to(const struct sockaddr_in *to, const uint8_t *buf, int n)
{
    if (sock == BAD_SOCK) return;
    sendto(sock, (const char *)buf, n, 0, (const struct sockaddr *)to, sizeof(*to));
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = (uint8_t)v; }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* the ROM, as the first 8 bytes of its SHA-1 (both must play the same game) */
/* the ROM as loaded, mods included: both players need the same */
static void rom_id(uint8_t out[8])
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < md.rom_size; i++) {
        h ^= md.rom[i];
        h *= 1099511628211ull;
    }
    for (int i = 0; i < 8; i++)
        out[i] = (uint8_t)(h >> (i * 8));
}

static void reset_lockstep(void)
{
    frame = 0;
    local_next = 0;
    remote_have = 0;
    peer_has = 0;
    my_hash_frame = their_hash_frame = 0;
    desync = 0;
    memset(in_local, 0, sizeof(in_local));
    memset(in_remote, 0, sizeof(in_remote));
}

static void send_simple(const struct sockaddr_in *to, char type, const char *text)
{
    uint8_t b[128] = {'B', 'Z', (uint8_t)type};
    int n = 3;
    if (text) {
        int l = (int)strlen(text);
        if (l > 100) l = 100;
        memcpy(b + n, text, l);
        n += l;
    }
    send_to(to, b, n);
}

static void send_hello(void)
{
    uint8_t b[64] = {'B', 'Z', 'H', NET_VERSION};
    rom_id(b + 4);
    memcpy(b + 12, my_name, 24);
    send_to(&peer, b, 36);
    last_hello = now_ms();
}

static void send_welcome(void)
{
    uint8_t b[64] = {'B', 'Z', 'W', NET_VERSION, (uint8_t)settings.floor_alert};
    memcpy(b + 5, my_name, 24);
    b[29] = (uint8_t)opt_prop_hunt;           /* as connected() took them */
    b[30] = (uint8_t)opt_ph_minutes;
    b[31] = (uint8_t)opt_race;
    send_to(&peer, b, 32);
}

static void send_inputs(void)
{
    uint8_t b[64 + 128];
    uint32_t first = peer_has, n = local_next > first ? local_next - first : 0;
    if (n > 120) { first = local_next - 120; n = 120; }
    b[0] = 'B'; b[1] = 'Z'; b[2] = 'I';
    put32(b + 3, first);
    b[7] = (uint8_t)n;
    for (uint32_t i = 0; i < n; i++)
        b[8 + i] = in_local[(first + i) % RING];
    uint8_t *p = b + 8 + n;
    put32(p, remote_have);
    put32(p + 4, my_hash_frame);
    put32(p + 8, my_hash);
    put32(p + 12, now_ms());
    put32(p + 16, t_echo);
    send_to(&peer, b, (int)(8 + n + 20));
    last_send = now_ms();
}

static void connected(const char *name)
{
    if (local_player == 0) {                  /* the host's game options count */
        opt_floor_alert = settings.floor_alert;
        opt_prop_hunt = settings.prop_hunt != 0;
        opt_ph_minutes = settings.ph_minutes >= 1 && settings.ph_minutes <= 9 ? settings.ph_minutes : 3;
        opt_race = settings.race != 0 && !opt_prop_hunt;
    }
    race_in_n = 0;
    snprintf(peer_name, sizeof(peer_name), "%.*s", 24, name);
    state = NET_PLAYING;
    start_pending = 1;
    reset_lockstep();
    last_recv = now_ms();
    snprintf(status_text, sizeof(status_text), "Playing with %s - you are player %d", peer_name, local_player + 1);
}

void net_stop(void)
{
    if (state == NET_PLAYING || state == NET_JOINING)
        send_simple(&peer, 'Q', NULL);
    state = NET_OFF;
    if (sock != BAD_SOCK) { close_sock(sock); sock = BAD_SOCK; }
    snprintf(status_text, sizeof(status_text), "Not connected");
}

int net_host(int port)
{
    net_stop();
    if (port <= 0) port = NET_DEFAULT_PORT;
    if (!open_socket(port)) {
        snprintf(status_text, sizeof(status_text), "Cannot use port %d", port);
        return 0;
    }
    state = NET_HOSTING;
    local_player = 0;
    snprintf(status_text, sizeof(status_text), "Waiting for player 2 at %s:%d", net_local_address(), port);
    return 1;
}

static int resolve(const char *address, struct sockaddr_in *out)
{
    char host[128];
    int port = NET_DEFAULT_PORT;
    snprintf(host, sizeof(host), "%s", address);
    char *c = strrchr(host, ':');
    if (c) { *c = 0; port = atoi(c + 1); }
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (!sockets_up() || getaddrinfo(host, NULL, &hints, &res) != 0 || !res)
        return 0;
    memcpy(out, res->ai_addr, sizeof(*out));
    out->sin_port = htons((uint16_t)port);
    freeaddrinfo(res);
    return 1;
}

int net_join(const char *address)
{
    net_stop();
    if (!resolve(address, &peer)) {
        snprintf(status_text, sizeof(status_text), "Cannot find %s", address);
        return 0;
    }
    if (!open_socket(0)) {
        snprintf(status_text, sizeof(status_text), "Network not available");
        return 0;
    }
    state = NET_JOINING;
    local_player = 1;
    send_hello();
    last_recv = now_ms();
    snprintf(status_text, sizeof(status_text), "Joining %s ...", address);
    return 1;
}

static int same_peer(const struct sockaddr_in *a)
{
    return a->sin_addr.s_addr == peer.sin_addr.s_addr && a->sin_port == peer.sin_port;
}

static void got_inputs(const uint8_t *b, int n)
{
    if (n < 8) return;
    uint32_t first = get32(b + 3);
    int cnt = b[7];
    if (n < 8 + cnt + 20) return;
    for (int i = 0; i < cnt; i++) {
        uint32_t f = first + (uint32_t)i;
        if (f == remote_have && f < frame + RING - 8) {   /* in order, room in the ring */
            in_remote[f % RING] = b[8 + i];
            remote_have++;
        }
    }
    const uint8_t *p = b + 8 + cnt;
    uint32_t ack = get32(p);
    if (ack > peer_has && ack <= local_next) peer_has = ack;
    uint32_t hf = get32(p + 4), hv = get32(p + 8);
    if (hf) { their_hash_frame = hf; their_hash = hv; }
    t_echo = get32(p + 12);
    uint32_t echo = get32(p + 16);
    if (echo) {
        uint32_t r = now_ms() - echo;
        if (r < 5000) rtt_ms = (rtt_ms * 7 + r) / 8;
    }
    if (their_hash_frame && their_hash_frame == my_hash_frame && their_hash != my_hash && !desync) {
        desync = 1;
        snprintf(status_text, sizeof(status_text), "Out of sync with %s at frame %u", peer_name, my_hash_frame);
    }
}

static void receive(void)
{
    uint8_t b[512];
    for (;;) {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int n = (int)recvfrom(sock, (char *)b, sizeof(b), 0, (struct sockaddr *)&from, &fl);
        if (n < 3 || b[0] != 'B' || b[1] != 'Z')
            break;
        char type = (char)b[2];
        if (type == 'D') {                          /* LAN search: answer while hosting */
            if (state == NET_HOSTING) {
                uint8_t r[40] = {'B', 'Z', 'E'};
                struct sockaddr_in me;
                socklen_t ml = sizeof(me);
                getsockname(sock, (struct sockaddr *)&me, &ml);
                r[3] = (uint8_t)(ntohs(me.sin_port) >> 8);
                r[4] = (uint8_t)ntohs(me.sin_port);
                memcpy(r + 5, my_name, 24);
                send_to(&from, r, 29);
            }
            continue;
        }
        if (type == 'E' && n >= 29) {               /* LAN answer */
            char addr[64], name[32];
            snprintf(addr, sizeof(addr), "%s:%d", inet_ntoa(from.sin_addr), b[3] << 8 | b[4]);
            snprintf(name, sizeof(name), "%.24s", (const char *)b + 5);
            int known = 0;
            for (int i = 0; i < lan_n; i++) known |= !strcmp(lan[i].addr, addr);
            if (!known && lan_n < 8) {
                snprintf(lan[lan_n].addr, sizeof(lan[lan_n].addr), "%s", addr);
                snprintf(lan[lan_n].entry, sizeof(lan[lan_n].entry), "%s  %s", name, addr);
                lan_n++;
            }
            continue;
        }
        if (type == 'H' && n >= 36) {               /* someone wants to join */
            uint8_t id[8];
            rom_id(id);
            if (state == NET_HOSTING) {
                if (b[3] != NET_VERSION || memcmp(b + 4, id, 8)) {
                    send_simple(&from, 'X', b[3] != NET_VERSION ? "different version of the port" : "different ROM or mods");
                    continue;
                }
                peer = from;
                local_player = 0;
                char name[32];
                snprintf(name, sizeof(name), "%.24s", (const char *)b + 12);
                connected(name);
                send_welcome();
            } else if (state == NET_PLAYING && same_peer(&from) && local_player == 0 && remote_have == 0) {
                send_welcome();                     /* our welcome was lost */
            } else if (state == NET_PLAYING) {
                send_simple(&from, 'X', "game already full");
            }
            continue;
        }
        if (!same_peer(&from))
            continue;
        last_recv = now_ms();
        if (type == 'W' && state == NET_JOINING && n >= 32) {
            if (b[3] != NET_VERSION) {
                net_stop();
                snprintf(status_text, sizeof(status_text), "Host runs a different version of the port");
                return;
            }
            opt_floor_alert = b[4];
            opt_prop_hunt = b[29] != 0;
            opt_ph_minutes = b[30] >= 1 && b[30] <= 9 ? b[30] : 3;
            opt_race = b[31] != 0;
            char name[32];
            snprintf(name, sizeof(name), "%.24s", (const char *)b + 5);
            local_player = 1;
            connected(name);
            rtt_ms = now_ms() - last_hello;
        } else if (type == 'X') {
            char why[110];
            snprintf(why, sizeof(why), "%.*s", n - 3, (const char *)b + 3);
            net_stop();
            snprintf(status_text, sizeof(status_text), "Refused: %s", why);
            return;
        } else if (type == 'Q') {
            net_stop();
            snprintf(status_text, sizeof(status_text), "%s left the game", peer_name[0] ? peer_name : "The other player");
            return;
        } else if (type == 'I' && state == NET_PLAYING) {
            got_inputs(b, n);
        } else if (type == 'R' && state == NET_PLAYING && n > 3) {
            race_in_n = n - 3 < (int)sizeof(race_in) ? n - 3 : (int)sizeof(race_in);
            memcpy(race_in, b + 3, (size_t)race_in_n);
        }
    }
}

void net_poll(void)
{
    if (sock == BAD_SOCK) return;
    receive();
    if (sock == BAD_SOCK) return;
    uint32_t t = now_ms();
    if (state == NET_JOINING) {
        if (t - last_hello > 250) send_hello();
        if (t - last_recv > 10000) {
            net_stop();
            snprintf(status_text, sizeof(status_text), "No answer - is the host running and the port open?");
        }
        return;
    }
    if (state != NET_PLAYING) return;
    /* input delay from the round trip: half of it plus some room for jitter, in frames,
     * plus one. It goes up at once, and down only after a few calm seconds, so that the
     * game does not keep waiting for buttons on a connection whose ping varies. */
    static uint32_t lower_since;
    int want = (int)((rtt_ms / 2 + 12 + 16) / 17) + 1;
    if (want < 2) want = 2;
    if (want > 12) want = 12;
    if (want >= delay) {
        delay = want;
        lower_since = t;
    } else if (t - lower_since > 4000) {
        delay--;
        lower_since = t;
    }
    if (local_player == 0 && remote_have == 0 && t - last_send > 100)
        send_welcome();
    if (t - last_send >= 15)
        send_inputs();
    if (t - last_recv > 6000) {
        net_stop();
        snprintf(status_text, sizeof(status_text), "Connection to %s lost", peer_name);
    }
}

void net_local_input(uint8_t pad)
{
    if (state != NET_PLAYING) return;
    int sent = 0;
    while (local_next < frame + (uint32_t)delay && local_next < peer_has + RING - 8) {
        in_local[local_next % RING] = pad;
        local_next++;
        sent = 1;
    }
    if (sent) send_inputs();
}

int net_frames_waiting(void)
{
    return state == NET_PLAYING && remote_have > frame ? (int)(remote_have - frame) - delay : 0;
}

int net_inputs_for_next_frame(uint8_t pads[2])
{
    if (state != NET_PLAYING || local_next <= frame || remote_have <= frame)
        return 0;
    pads[local_player] = in_local[frame % RING];
    pads[1 - local_player] = in_remote[frame % RING];
    return 1;
}

void net_frame_done(uint32_t state_hash)
{
    frame++;
    if (frame % HASH_EVERY == 0) {
        my_hash_frame = frame;
        my_hash = state_hash;
        if (their_hash_frame == my_hash_frame && their_hash != my_hash && !desync) {
            desync = 1;
            snprintf(status_text, sizeof(status_text), "Out of sync with %s at frame %u", peer_name, frame);
        }
    }
    if (!desync && frame % 60 == 0)
        snprintf(status_text, sizeof(status_text), "Playing with %s - player %d - ping %u ms, delay %d",
                 peer_name, local_player + 1, rtt_ms, delay);
}

void net_race_send(const uint8_t *d, int n)
{
    if (state != NET_PLAYING) return;
    uint8_t b[3 + 64] = {'B', 'Z', 'R'};
    if (n > 64) n = 64;
    memcpy(b + 3, d, (size_t)n);
    send_to(&peer, b, 3 + n);
}

void net_lan_search(void)
{
    lan_n = 0;
    if (sock == BAD_SOCK && !open_socket(0)) return;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(NET_DEFAULT_PORT);
    a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    send_simple(&a, 'D', NULL);
}

int net_lan_count(void) { return lan_n; }
const char *net_lan_entry(int i) { return i >= 0 && i < lan_n ? lan[i].entry : ""; }
const char *net_lan_address(int i) { return i >= 0 && i < lan_n ? lan[i].addr : ""; }

const char *net_local_address(void)
{
    static char buf[64];
    snprintf(buf, sizeof(buf), "?");
    if (!sockets_up()) return buf;
    /* the address the system would use to reach the internet (nothing is sent) */
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == BAD_SOCK) return buf;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(53);
    a.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0) {
        struct sockaddr_in me;
        socklen_t ml = sizeof(me);
        if (getsockname(s, (struct sockaddr *)&me, &ml) == 0)
            snprintf(buf, sizeof(buf), "%s", inet_ntoa(me.sin_addr));
    }
    close_sock(s);
    return buf;
}
#endif
