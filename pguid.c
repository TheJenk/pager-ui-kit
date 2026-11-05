/*
 * pguid - a tiny on-screen renderer for the WiFi Pineapple Pager.
 *
 * Lets a payload draw custom screens and menus WITHOUT bringing its own
 * framebuffer renderer. pguid owns the framebuffer, the font, the rotation and
 * the buttons. A payload (in ANY language - bash, python, ...) just sends it
 * simple text commands and reads back button presses. See ui.sh for the
 * friendly bash wrapper.
 *
 * Usage:  pguid <command-fifo> <event-fifo>
 *   reads newline-terminated commands from <command-fifo>:
 *     clear RRGGBB
 *     rect   x y w h RRGGBB          (filled)
 *     frame  x y w h RRGGBB          (1px outline)
 *     line   x y w h RRGGBB          (filled bar; w or h = thickness)
 *     text   x y size RRGGBB  the rest of the line is the string
 *     image  x y path                (blit a .pgi image, top-left at x,y)
 *     present                        (show what's been drawn)
 *     quit
 *   writes one line per event to <event-fifo>:
 *     READY                          (once, on startup)
 *     KEY UP|DOWN|LEFT|RIGHT|A|B|POWER
 *
 * Coordinates are the upright 480x222 view (0,0 = top-left). Colors are RRGGBB
 * hex. Freestanding MIPS (no libc, integer-only - the MT7628 has no FPU).
 */
#include <stdint.h>
#include "font5x7.h"

#define W 480
#define H 222
#define FB_W 222
#define NPIX (W * H)
#define FRAME_BYTES (NPIX * 2)

static uint16_t fb[NPIX];

/* ---- syscalls ---- */
#define SYS_read 4003
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_lseek 4019
#define SYS_nanosleep 4166
#define SYS_poll 4188
#define O_RDONLY 0
#define O_RDWR 2
#define O_NONBLOCK 0x80
#define POLLIN 1
struct pollfd { int fd; short events; short revents; };
static long sys3(long n, long a, long b, long c) {
    register long r2 asm("$2") = n, r4 asm("$4") = a, r5 asm("$5") = b, r6 asm("$6") = c, r7 asm("$7") = 0;
    asm volatile("syscall" : "+r"(r2), "+r"(r7) : "r"(r4), "r"(r5), "r"(r6)
                 : "$1","$3","$8","$9","$10","$11","$12","$13","$14","$15","$24","$25","memory");
    return r7 ? -r2 : r2;
}
asm(".text\n.globl _start\n.ent _start\n_start:\n lw $4,0($sp)\n addiu $5,$sp,4\n jal main\n nop\n move $4,$2\n li $2,4246\n syscall\n nop\n.end _start\n");
struct ts { long sec, nsec; };
static void sleep_ms(long ms) { struct ts t; t.sec = ms / 1000; t.nsec = (ms % 1000) * 1000000; sys3(SYS_nanosleep, (long)&t, 0, 0); }

/* ---- drawing ---- */
static inline uint16_t rgb(int r, int g, int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline void px(int x, int y, uint16_t c) { if ((unsigned)x < W && (unsigned)y < H) fb[x * FB_W + (H - 1 - y)] = c; }
static void fillrect(int x, int y, int w, int h, uint16_t c) { for (int i = 0; i < w; i++) for (int j = 0; j < h; j++) px(x + i, y + j, c); }
static void frame_rect(int x, int y, int w, int h, uint16_t c) { fillrect(x, y, w, 1, c); fillrect(x, y + h - 1, w, 1, c); fillrect(x, y, 1, h, c); fillrect(x + w - 1, y, 1, h, c); }
static void draw_text(int x, int y, const char *s, int n, uint16_t c, int sc) {
    for (int k = 0; k < n && *s; s++, k++) {
        char ch = *s; if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch >= FONT_FIRST && ch < FONT_FIRST + FONT_COUNT) {
            const unsigned char *g = FONT5X7[ch - FONT_FIRST];
            for (int row = 0; row < 7; row++) { unsigned bits = g[row];
                for (int col = 0; col < 5; col++) if (bits & (1u << (4 - col))) fillrect(x + col * sc, y + row * sc, sc, sc, c); } }
        x += 6 * sc;
    }
}

/* ---- tiny parsers (no libc) ---- */
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int hexd(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return 0; }
/* advance *pp past spaces, read a (possibly negative) integer */
static int next_int(char **pp) {
    char *p = *pp; while (*p == ' ') p++;
    int neg = 0; if (*p == '-') { neg = 1; p++; }
    int v = 0; while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
    *pp = p; return neg ? -v : v;
}
/* advance past spaces, read a RRGGBB hex color -> rgb565 */
static uint16_t next_color(char **pp) {
    char *p = *pp; while (*p == ' ') p++;
    int r = hexd(p[0]) * 16 + hexd(p[1]);
    int g = hexd(p[2]) * 16 + hexd(p[3]);
    int b = hexd(p[4]) * 16 + hexd(p[5]);
    p += 6; *pp = p; return rgb(r, g, b);
}

/* ---- input ---- */
static const char *keyname(int code) {
    switch (code) {
        case 103: return "UP"; case 108: return "DOWN"; case 105: return "LEFT"; case 106: return "RIGHT";
        case 305: return "A"; case 304: return "B"; case 116: return "POWER"; }
    return 0;
}

static int fbfd, evfd, cmdfd, outfd;
static void emit(const char *s) { sys3(SYS_write, outfd, (long)s, slen(s)); }

static void present(void) { sys3(SYS_lseek, fbfd, 0, 0); sys3(SYS_write, fbfd, (long)fb, FRAME_BYTES); }

/* Blit a .pgi image (4-byte header: u16 width, u16 height, little-endian,
 * then width*height RGB565LE pixels) with its top-left corner at (x,y).
 * Streamed a row at a time so any size works with a small buffer. */
static void blit_image(int x, int y, const char *path) {
    int fd = (int)sys3(SYS_open, (long)path, O_RDONLY, 0);
    if (fd < 0) return;
    unsigned char hdr[4];
    if (sys3(SYS_read, fd, (long)hdr, 4) == 4) {
        int iw = hdr[0] | (hdr[1] << 8), ih = hdr[2] | (hdr[3] << 8);
        static unsigned char row[W * 2];
        for (int j = 0; j < ih; j++) {
            int need = iw * 2; if (need > (int)sizeof(row)) need = sizeof(row);
            int got = 0;
            while (got < need) { long r = sys3(SYS_read, fd, (long)(row + got), need - got); if (r <= 0) break; got += r; }
            for (int i = 0; i * 2 + 1 < got; i++) px(x + i, y + j, (uint16_t)(row[i * 2] | (row[i * 2 + 1] << 8)));
            if (got < need) break;
        }
    }
    sys3(SYS_close, fd, 0, 0);
}

static void run_cmd(char *line) {
    char *p = line;
    while (*p == ' ') p++;
    if (p[0] == 'c' && p[1] == 'l') {            /* clear RRGGBB */
        p += 5; uint16_t col = next_color(&p);
        for (int i = 0; i < NPIX; i++) fb[i] = col;
    } else if (p[0] == 'r' && p[1] == 'e') {     /* rect x y w h RRGGBB */
        p += 4; int x = next_int(&p), y = next_int(&p), w = next_int(&p), h = next_int(&p); uint16_t c = next_color(&p);
        fillrect(x, y, w, h, c);
    } else if (p[0] == 'f' && p[1] == 'r') {     /* frame x y w h RRGGBB */
        p += 5; int x = next_int(&p), y = next_int(&p), w = next_int(&p), h = next_int(&p); uint16_t c = next_color(&p);
        frame_rect(x, y, w, h, c);
    } else if (p[0] == 'l' && p[1] == 'i') {     /* line x y w h RRGGBB (thin bar) */
        p += 4; int x = next_int(&p), y = next_int(&p), w = next_int(&p), h = next_int(&p); uint16_t c = next_color(&p);
        fillrect(x, y, w ? w : 1, h ? h : 1, c);
    } else if (p[0] == 't' && p[1] == 'e') {     /* text x y size RRGGBB string... */
        p += 4; int x = next_int(&p), y = next_int(&p), sc = next_int(&p); uint16_t c = next_color(&p);
        if (*p == ' ') p++;
        draw_text(x, y, p, 10000, c, sc < 1 ? 1 : sc);
    } else if (p[0] == 'i' && p[1] == 'm') {     /* image x y path  (.pgi) */
        p += 5; int x = next_int(&p), y = next_int(&p);
        while (*p == ' ') p++;                    /* rest of line = file path */
        blit_image(x, y, p);
    } else if (p[0] == 'p' && p[1] == 'r') {     /* present */
        present();
    } else if (p[0] == 'q' && p[1] == 'u') {     /* quit */
        emit("BYE\n"); sys3(SYS_close, fbfd, 0, 0); sys3(4246, 0, 0, 0);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { sys3(SYS_write, 2, (long)"usage: pguid cmdfifo evtfifo\n", 28); return 2; }
    fbfd  = (int)sys3(SYS_open, (long)"/dev/fb0", O_RDWR, 0); if (fbfd < 0) return 1;
    evfd  = (int)sys3(SYS_open, (long)"/dev/input/event0", O_RDONLY | O_NONBLOCK, 0);
    /* O_RDWR on the fifos so opening never blocks and reads never hit EOF */
    cmdfd = (int)sys3(SYS_open, (long)argv[1], O_RDWR | O_NONBLOCK, 0); if (cmdfd < 0) return 1;
    outfd = (int)sys3(SYS_open, (long)argv[2], O_RDWR, 0);             if (outfd < 0) return 1;

    /* drain the keypress that launched us so it isn't reported as input;
       twice with a short gap, since the press can land just after we open */
    { unsigned char j[16];
      for (int pass = 0; pass < 2; pass++) {
          while (evfd >= 0 && sys3(SYS_read, evfd, (long)j, 16) == 16) {}
          sleep_ms(120);
      } }

    for (int i = 0; i < NPIX; i++) fb[i] = rgb(10, 12, 18);
    present();
    emit("READY\n");

    static char lb[1024]; int ln = 0;
    unsigned char cb[512];
    /* Block on the command pipe and the buttons - the renderer uses no CPU at
       all while the payload is idle and nobody is pressing anything. It only
       wakes to run a command or report a keypress. (Far kinder to the battery
       than a polling loop.) */
    struct pollfd pfd[2];
    pfd[0].fd = cmdfd; pfd[0].events = POLLIN;
    pfd[1].fd = evfd;  pfd[1].events = POLLIN;
    while (1) {
        pfd[0].revents = pfd[1].revents = 0;
        sys3(SYS_poll, (long)pfd, evfd >= 0 ? 2 : 1, -1);   /* sleep until something happens */
        /* commands */
        if (pfd[0].revents & POLLIN) {
            long n = sys3(SYS_read, cmdfd, (long)cb, sizeof cb);
            for (long i = 0; i < n; i++) {
                char ch = cb[i];
                if (ch == '\n') { lb[ln] = 0; run_cmd(lb); ln = 0; }
                else if (ln < (int)sizeof(lb) - 1) lb[ln++] = ch;
            }
        }
        /* input */
        if (pfd[1].revents & POLLIN) {
            unsigned char e[16];
            while (sys3(SYS_read, evfd, (long)e, 16) == 16) {
                int type = e[8] | (e[9] << 8), code = e[10] | (e[11] << 8), val = e[12] | (e[13] << 8);
                if (type != 1 || val != 1) continue;
                const char *k = keyname(code);
                if (k) { emit("KEY "); emit(k); emit("\n"); }
            }
        }
    }
    return 0;
}
