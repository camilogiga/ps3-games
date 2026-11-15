/*
 * Rano Saltarin - an original 2D platformer about a little frog.
 * Pure C, no PS3 calls: draws into a 1280x720 RAM buffer (see game.h).
 *
 * Up to two players play together (co-op) on the same screen and share
 * their lives. Player 2 joins any time by pressing X on controller 2.
 */
#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "levels.h"

#define TILE 48
#define ROWS 15
#define MAX_W 256

/* physics, in pixels and seconds */
#define GRAVITY 2600.0f
#define JUMP_V 960.0f
#define SPRING_V 1480.0f
#define STOMP_V 700.0f
#define MAX_FALL 1150.0f
#define WALK_SPEED 290.0f
#define RUN_SPEED 450.0f
#define ACCEL 2600.0f
#define AIR_ACCEL 1700.0f
#define FRICTION 2400.0f

#define PW 32 /* player hitbox */
#define PH 40

#define START_LIVES 5
#define MAX_HEARTS 3

/* ============================================================ drawing */

uint32_t *screen;
static uint32_t color;

static void set_color(int r, int g, int b) {
    color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
static void set_rgb(uint32_t c) { color = c; }

static void fill_rect(int x, int y, int w, int h) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    for (int yy = y; yy < y1; yy++) {
        uint32_t *row = screen + yy * SCREEN_W;
        for (int xx = x; xx < x1; xx++) row[xx] = color;
    }
}

static uint32_t mix(uint32_t a, uint32_t b, float t) {
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

/* Pixel art: each char is a palette entry, '.' is transparent */
typedef struct { char key; uint32_t rgb; } PalEntry;

static void draw_sprite(const char **rows, int nrows, int x, int y, int scale, int flip,
                        const PalEntry *pal, int npal) {
    for (int r = 0; r < nrows; r++) {
        const char *s = rows[r];
        int w = (int)strlen(s);
        for (int c = 0; c < w; c++) {
            char k = s[flip ? (w - 1 - c) : c];
            if (k == '.' || k == ' ') continue;
            /* merge runs of the same color into one rect */
            int run = 1;
            while (c + run < w && s[flip ? (w - 1 - c - run) : (c + run)] == k) run++;
            for (int i = 0; i < npal; i++) {
                if (pal[i].key == k) {
                    set_rgb(pal[i].rgb);
                    fill_rect(x + c * scale, y + r * scale, run * scale, scale);
                    break;
                }
            }
            c += run - 1;
        }
    }
}

/* ------------------------------------------------------ 5x7 font */

static const char *glyph(char c) {
    switch (c) {
    case 'A': return "01110100011000111111100011000110001";
    case 'B': return "11110100011000111110100011000111110";
    case 'C': return "01110100011000010000100001000101110";
    case 'D': return "11110100011000110001100011000111110";
    case 'E': return "11111100001000011110100001000011111";
    case 'F': return "11111100001000011110100001000010000";
    case 'G': return "01110100011000010111100011000101111";
    case 'H': return "10001100011000111111100011000110001";
    case 'I': return "01110001000010000100001000010001110";
    case 'J': return "00111000100001000010000101001001100";
    case 'K': return "10001100101010011000101001001010001";
    case 'L': return "10000100001000010000100001000011111";
    case 'M': return "10001110111010110101100011000110001";
    case 'N': return "10001100011100110101100111000110001";
    case 'O': return "01110100011000110001100011000101110";
    case 'P': return "11110100011000111110100001000010000";
    case 'Q': return "01110100011000110001101011001001101";
    case 'R': return "11110100011000111110101001001010001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'U': return "10001100011000110001100011000101110";
    case 'V': return "10001100011000110001100010101000100";
    case 'W': return "10001100011000110101101011010101010";
    case 'X': return "10001100010101000100010101000110001";
    case 'Y': return "10001100010101000100001000010000100";
    case 'Z': return "11111000010001000100010001000011111";
    case '0': return "01110100011001110101110011000101110";
    case '1': return "00100011000010000100001000010001110";
    case '2': return "01110100010000100010001000100011111";
    case '3': return "11111000100010000010000011000101110";
    case '4': return "00010001100101010010111110001000010";
    case '5': return "11111100001111000001000011000101110";
    case '6': return "00110010001000011110100011000101110";
    case '7': return "11111000010001000100010000100001000";
    case '8': return "01110100011000101110100011000101110";
    case '9': return "01110100011000101111000010001001100";
    case '!': return "00100001000010000100001000000000100";
    case '-': return "00000000000000011111000000000000000";
    case ':': return "00000011000110000000011000110000000";
    case '.': return "00000000000000000000000000110001100";
    case 'x': return "00000000001000101010001000101010001";
    case '?': return "01110100010000100010001000000000100";
    case '/': return "00001000100001000100010000100010000";
    case ',': return "00000000000000000000011000010001000";
    default:  return "00000000000000000000000000000000000";
    }
}

static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n > 0 ? n * 6 * scale - scale : 0;
}

static void draw_text_raw(const char *s, int x, int y, int scale) {
    for (; *s; s++, x += 6 * scale) {
        if (*s == ' ') continue;
        const char *g = glyph(*s);
        for (int r = 0; r < 7; r++) {
            for (int c = 0; c < 5; c++) {
                if (g[r * 5 + c] != '1') continue;
                int run = 1;
                while (c + run < 5 && g[r * 5 + c + run] == '1') run++;
                fill_rect(x + c * scale, y + r * scale, run * scale, scale);
                c += run - 1;
            }
        }
    }
}

/* text with a dark drop shadow so it reads on any background */
static void draw_text(const char *s, int x, int y, int scale, uint32_t rgb) {
    int d = scale / 2 > 1 ? scale / 2 : 2;
    set_rgb(0x1a1424);
    draw_text_raw(s, x + d, y + d, scale);
    set_rgb(rgb);
    draw_text_raw(s, x, y, scale);
}

static void draw_text_centered(const char *s, int y, int scale, uint32_t rgb) {
    draw_text(s, (SCREEN_W - text_width(s, scale)) / 2, y, scale, rgb);
}

/* ============================================================ sprites */

/* Rano, facing right. G body, D outline, L belly, W eye, K pupil, R cheek */
static const char *frog_stand[] = {
    "...DDD....DDD...",
    "..DWWWD..DWWWD..",
    "..DWWKD..DWWKD..",
    "..DGGGDDDDGGGD..",
    ".DGGGGGGGGGGGGD.",
    "DGGGGGGGGGGGGGGD",
    "DGRGDGGGGGGDGRGD",
    "DGGGGDDDDDDGGGGD",
    ".DGGLLLLLLLLGGD.",
    ".DGLLLLLLLLLLGD.",
    "..DGLLLLLLLLGD..",
    ".DDGGDDDDDDGGDD.",
    "DGGGD......DGGGD",
    "DDDD........DDDD",
};
static const char *frog_walk[] = {
    "...DDD....DDD...",
    "..DWWWD..DWWWD..",
    "..DWWKD..DWWKD..",
    "..DGGGDDDDGGGD..",
    ".DGGGGGGGGGGGGD.",
    "DGGGGGGGGGGGGGGD",
    "DGRGDGGGGGGDGRGD",
    "DGGGGDDDDDDGGGGD",
    ".DGGLLLLLLLLGGD.",
    ".DGLLLLLLLLLLGD.",
    "..DGLLLLLLLLGD..",
    "..DDGGDDDDGGDD..",
    "..DGGGD..DGGGD..",
    "..DDDD....DDDD..",
};
static const char *frog_jump[] = {
    "...DDD....DDD...",
    "..DWWWD..DWWWD..",
    "..DWWKD..DWWKD..",
    "..DGGGDDDDGGGD..",
    ".DGGGGGGGGGGGGD.",
    "DGGGGGGGGGGGGGGD",
    "DGRGGDDDDDDGGRGD",
    "DGGGGDRRRRDGGGGD",
    ".DGGLLDDDDLLGGD.",
    ".DGLLLLLLLLLLGD.",
    "..DGLLLLLLLLGD..",
    ".DGGDDDDDDDDGGD.",
    "DGGD........DGGD",
    "DDD..........DDD",
};

static const PalEntry frog_pal[2][6] = {
    { {'G', 0x5cc94a}, {'D', 0x1f4a2a}, {'L', 0xd8f0a0}, {'W', 0xffffff}, {'K', 0x101018}, {'R', 0xff8fa0} },
    { {'G', 0x4aa8f0}, {'D', 0x1a2c5a}, {'L', 0xf0e6b0}, {'W', 0xffffff}, {'K', 0x101018}, {'R', 0xff8fa0} },
};

/* Beetle, facing right. R shell, H highlight, K dark */
static const char *beetle_a[] = {
    "....KKKKKK....",
    "..KKRRRRRRKK..",
    ".KRRHHRRRRRRK.",
    ".KRHRRRRKRRRKK",
    "KRRRRRRRKRRKWK",
    "KRRRRRRRKRRKKK",
    "KKKKKKKKKKKKK.",
    ".K..K..K..K...",
    "K..K..K..K....",
};
static const char *beetle_b[] = {
    "....KKKKKK....",
    "..KKRRRRRRKK..",
    ".KRRHHRRRRRRK.",
    ".KRHRRRRKRRRKK",
    "KRRRRRRRKRRKWK",
    "KRRRRRRRKRRKKK",
    "KKKKKKKKKKKKK.",
    "..K..K..K..K..",
    "..K..K..K..K..",
};
static const char *beetle_flat[] = {
    "..KKKKKKKKKK..",
    ".KRRRRRRRRRRK.",
    "KKKKKKKKKKKKKK",
};
static const PalEntry beetle_pal[] = {
    {'R', 0xc8463c}, {'H', 0xf08c6c}, {'K', 0x2a1418}, {'W', 0xffffff},
};

/* Wasp, facing right. Y yellow, K black, W wing */
static const char *wasp_a[] = {
    "...WWW..WWW...",
    "..WWWWWWWWWW..",
    "...WWW..WWW...",
    "..KKYYKKYYKK..",
    ".KYYKKYYKKYYKK",
    "KKYYKKYYKKYKWK",
    ".KYYKKYYKKYYKK",
    "..KKYYKKYYKK..",
    "K.............",
};
static const char *wasp_b[] = {
    "..............",
    "..............",
    "...WWWWWWWW...",
    "..KKYYKKYYKK..",
    ".KYYKKYYKKYYKK",
    "KKYYKKYYKKYKWK",
    ".KYYKKYYKKYYKK",
    "..KKYYKKYYKK..",
    "K.............",
};
static const PalEntry wasp_pal[] = {
    {'Y', 0xf5c43a}, {'K', 0x2a2018}, {'W', 0xdff4ff},
};

static const char *heart_img[] = {
    ".RR.RR.",
    "RWRRRRR",
    "RRRRRRR",
    "RRRRRRR",
    ".RRRRR.",
    "..RRR..",
    "...R...",
};
static const PalEntry heart_pal[] = { {'R', 0xff4a6a}, {'W', 0xffd0d8} };
static const PalEntry heart_empty_pal[] = { {'R', 0x4a3a4a}, {'W', 0x5a4a5a} };

static const char *gem_img[] = {
    "..KKKK..",
    ".KWCCCK.",
    "KWCCCCCK",
    "KCCCCCBK",
    ".KCCCBK.",
    "..KCBK..",
    "...KK...",
};
static const PalEntry gem_pal[] = { {'K', 0x0e4a6a}, {'W', 0xffffff}, {'C', 0x4ae0f0}, {'B', 0x2a9ac8} };

/* ============================================================ world */

typedef struct {
    float x, y;       /* top-left */
    float x0, range;  /* moves between x0 and x0 + range */
    float dx;         /* movement this step */
    int dir;
} Mover;

enum { EN_BEETLE, EN_WASP };

typedef struct {
    int type, alive, dir;
    float x, y, vx, vy, t;
    float home_x, home_y;
    float dead_t; /* > 0: squashed / falling away */
} Enemy;

typedef struct {
    int active;   /* joined the game */
    int alive;    /* in the level right now */
    float x, y, vx, vy;
    int on_ground, face;
    float coyote, jump_buf, inv, anim, respawn_t;
    int hearts;
    int jump_held_prev;
    Mover *riding;
} Player;

typedef struct { float x, y, vx, vy, life; uint32_t rgb; int size; } Particle;

#define MAX_ENEMIES 64
#define MAX_MOVERS 16
#define MAX_PARTICLES 96

static char map[ROWS][MAX_W];
static int map_w;
static int level_index, theme;
static Enemy enemies[MAX_ENEMIES];
static int n_enemies;
static Mover movers[MAX_MOVERS];
static int n_movers;
static Particle parts[MAX_PARTICLES];
static Player players[MAX_PLAYERS];
static float start_x, start_y, check_x, check_y;
static int check_col = -1;
static float cam_x;
static int lives, gems, gems_level, gems_level_total;
static float spring_anim[MAX_W]; /* per column, for the bounce animation */

typedef enum { ST_TITLE, ST_INTRO, ST_PLAY, ST_PAUSE, ST_ALLDEAD, ST_CLEAR, ST_GAMEOVER, ST_WIN } State;
static State state = ST_TITLE;
static float state_t, clock_t_;

/* ------------------------------------------------------------ tiles */

static char tile_at(int tx, int ty) {
    if (ty < 0 || ty >= ROWS) return ' ';
    if (tx < 0 || tx >= map_w) return 'B'; /* invisible walls at level ends */
    return map[ty][tx];
}
static int is_solid(char c) { return c == '#' || c == 'B' || c == 'S'; }
static int solid_at(int tx, int ty) {
    if (ty < 0) return tx < 0 || tx >= map_w;
    return is_solid(tile_at(tx, ty));
}

static int fdiv(float v) { return (int)floorf(v / TILE); }

/* ------------------------------------------------------------ particles */

static void spawn_particles(float x, float y, int n, uint32_t rgb, float speed) {
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (parts[i].life > 0) continue;
            float a = (rand() % 628) / 100.0f;
            float s = speed * (0.4f + (rand() % 60) / 100.0f);
            parts[i].x = x;
            parts[i].y = y;
            parts[i].vx = cosf(a) * s;
            parts[i].vy = sinf(a) * s - speed * 0.5f;
            parts[i].life = 0.5f + (rand() % 40) / 100.0f;
            parts[i].rgb = rgb;
            parts[i].size = 6 + rand() % 5;
            break;
        }
    }
}

/* ------------------------------------------------------------ level load */

static void reset_enemies(void) {
    for (int i = 0; i < n_enemies; i++) {
        Enemy *e = &enemies[i];
        e->alive = 1;
        e->x = e->home_x;
        e->y = e->home_y;
        e->vx = e->vy = 0;
        e->dir = -1;
        e->t = (float)(i * 0.7);
        e->dead_t = 0;
    }
    for (int i = 0; i < n_movers; i++) {
        Mover *m = &movers[i];
        m->x = m->dir > 0 ? m->x0 : m->x0 + m->range;
    }
}

static void place_player(Player *p, float x, float y) {
    p->alive = 1;
    p->x = x;
    p->y = y;
    p->vx = p->vy = 0;
    p->on_ground = 0;
    p->face = 1;
    p->coyote = p->jump_buf = 0;
    p->inv = 1.0f;
    p->hearts = MAX_HEARTS;
    p->riding = NULL;
    p->respawn_t = 0;
}

static void spawn_all_players(float x, float y) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;
        place_player(&players[i], x + i * 44, y);
    }
}

static void load_level(int idx) {
    const LevelDef *L = &level_defs[idx];
    level_index = idx;
    theme = L->theme;
    map_w = L->width;
    n_enemies = n_movers = 0;
    gems_level = gems_level_total = 0;
    check_col = -1;
    memset(parts, 0, sizeof(parts));
    memset(spring_anim, 0, sizeof(spring_anim));
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < map_w; x++) {
            char c = L->rows[y][x];
            float px = x * TILE, py = y * TILE;
            switch (c) {
            case 'P':
                start_x = px + 8;
                start_y = py + TILE - PH;
                c = ' ';
                break;
            case 'e':
            case 'w':
                if (n_enemies < MAX_ENEMIES) {
                    Enemy *e = &enemies[n_enemies++];
                    e->type = c == 'e' ? EN_BEETLE : EN_WASP;
                    e->home_x = px + 4;
                    e->home_y = c == 'e' ? py + TILE - 28 : py + 8;
                }
                c = ' ';
                break;
            case 'M':
            case 'N':
                if (n_movers < MAX_MOVERS) {
                    Mover *m = &movers[n_movers++];
                    m->x0 = px;
                    m->y = py;
                    m->range = 5 * TILE;
                    m->dir = c == 'M' ? 1 : -1;
                }
                c = ' ';
                break;
            case 'o':
                gems_level_total++;
                break;
            }
            map[y][x] = c;
        }
    }
    check_x = start_x;
    check_y = start_y;
    reset_enemies();
    spawn_all_players(start_x, start_y);
    cam_x = 0;
}

/* ------------------------------------------------------------ game flow */

static void new_game(void) {
    lives = START_LIVES;
    gems = 0;
    players[0].active = 1;
    load_level(0);
    state = ST_INTRO;
    state_t = 0;
}

static void hurt_player(Player *p, float from_x);

static void kill_player(Player *p) {
    if (!p->alive) return;
    p->alive = 0;
    p->riding = NULL;
    spawn_particles(p->x + PW / 2, p->y + PH / 2, 14, p == &players[0] ? 0x5cc94a : 0x4aa8f0, 380);
    lives--;
    int others = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (players[i].active && players[i].alive) others++;
    if (lives < 0) {
        lives = 0;
        state = ST_GAMEOVER;
        state_t = 0;
    } else if (others) {
        p->respawn_t = 2.0f; /* comes back next to the friend */
    } else {
        state = ST_ALLDEAD;
        state_t = 0;
    }
}

static void hurt_player(Player *p, float from_x) {
    if (p->inv > 0 || !p->alive) return;
    p->hearts--;
    if (p->hearts <= 0) {
        kill_player(p);
        return;
    }
    p->inv = 1.6f;
    p->vy = -520;
    p->vx = (p->x + PW / 2 < from_x) ? -300 : 300;
    p->riding = NULL;
    spawn_particles(p->x + PW / 2, p->y + PH / 2, 6, 0xff4a6a, 260);
}

/* ------------------------------------------------------------ update */

static void collect_tiles(Player *p) {
    int tx0 = fdiv(p->x), tx1 = fdiv(p->x + PW - 1);
    int ty0 = fdiv(p->y), ty1 = fdiv(p->y + PH - 1);
    for (int ty = ty0; ty <= ty1; ty++) {
        for (int tx = tx0; tx <= tx1; tx++) {
            if (ty < 0 || ty >= ROWS || tx < 0 || tx >= map_w) continue;
            char c = map[ty][tx];
            float cx = tx * TILE + TILE / 2, cy = ty * TILE + TILE / 2;
            if (c == 'o') {
                map[ty][tx] = ' ';
                gems++;
                gems_level++;
                spawn_particles(cx, cy, 6, 0x9af4ff, 220);
                if (gems >= 100) {
                    gems -= 100;
                    lives++;
                }
            } else if (c == 'h') {
                map[ty][tx] = ' ';
                for (int i = 0; i < MAX_PLAYERS; i++)
                    if (players[i].active && players[i].alive) players[i].hearts = MAX_HEARTS;
                spawn_particles(cx, cy, 8, 0xff4a6a, 220);
            } else if (c == 'C' && tx > check_col) {
                check_col = tx;
                check_x = tx * TILE + 8;
                check_y = ty * TILE + TILE - PH;
                spawn_particles(cx, cy - 20, 12, 0xffe070, 300);
            } else if (c == 'G' && state == ST_PLAY) {
                state = ST_CLEAR;
                state_t = 0;
                spawn_particles(cx, cy - 30, 30, 0xffd84a, 500);
            } else if (c == '^') {
                /* spikes only hurt in the lower part of the tile */
                if (p->y + PH > ty * TILE + 20) hurt_player(p, cx);
            }
        }
    }
}

static void move_player_x(Player *p, float dx) {
    p->x += dx;
    int ty0 = fdiv(p->y + 2), ty1 = fdiv(p->y + PH - 2);
    if (dx > 0) {
        int tx = fdiv(p->x + PW);
        for (int ty = ty0; ty <= ty1; ty++)
            if (solid_at(tx, ty)) { p->x = tx * TILE - PW; p->vx = 0; break; }
    } else if (dx < 0) {
        int tx = fdiv(p->x);
        for (int ty = ty0; ty <= ty1; ty++)
            if (solid_at(tx, ty)) { p->x = (tx + 1) * TILE; p->vx = 0; break; }
    }
}

static void move_player_y(Player *p, float dy) {
    float old_bottom = p->y + PH;
    p->y += dy;
    p->on_ground = 0;
    int tx0 = fdiv(p->x + 2), tx1 = fdiv(p->x + PW - 3);
    if (dy > 0) {
        int ty = fdiv(p->y + PH);
        for (int tx = tx0; tx <= tx1; tx++) {
            char c = tile_at(tx, ty);
            int one_way = (c == '-') && old_bottom <= ty * TILE + 1;
            if (is_solid(c) || one_way) {
                p->y = ty * TILE - PH;
                p->vy = 0;
                p->on_ground = 1;
                p->riding = NULL;
                if (c == 'S') {
                    p->vy = -SPRING_V;
                    p->on_ground = 0;
                    spring_anim[tx] = 0.25f;
                }
                break;
            }
        }
        if (!p->on_ground) {
            for (int i = 0; i < n_movers; i++) {
                Mover *m = &movers[i];
                if (p->x + PW > m->x + 2 && p->x < m->x + 3 * TILE - 2 &&
                    old_bottom <= m->y + 1 && p->y + PH >= m->y) {
                    p->y = m->y - PH;
                    p->vy = 0;
                    p->on_ground = 1;
                    p->riding = m;
                    break;
                }
            }
        }
        if (!p->on_ground) p->riding = NULL;
    } else if (dy < 0) {
        int ty = fdiv(p->y);
        for (int tx = tx0; tx <= tx1; tx++)
            if (solid_at(tx, ty)) { p->y = (ty + 1) * TILE; p->vy = 0; break; }
        p->riding = NULL;
    }
}

static void update_player(Player *p, const Input *in, int first_step, float dt) {
    if (!p->alive) {
        if (p->respawn_t > 0) {
            p->respawn_t -= dt;
            if (p->respawn_t <= 0) {
                /* drop in next to a living friend */
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    Player *f = &players[i];
                    if (f != p && f->active && f->alive) {
                        place_player(p, f->x, f->y - 70);
                        p->inv = 2.0f;
                        break;
                    }
                }
            }
        }
        return;
    }

    p->anim += dt;
    if (p->inv > 0) p->inv -= dt;

    /* carried by a moving ledge */
    if (p->riding) {
        move_player_x(p, p->riding->dx);
        p->y = p->riding->y - PH;
    }

    int dir = (in->right ? 1 : 0) - (in->left ? 1 : 0);
    float top = in->run ? RUN_SPEED : WALK_SPEED;
    float acc = p->on_ground ? ACCEL : AIR_ACCEL;
    if (dir != 0) {
        p->face = dir;
        p->vx += dir * acc * dt;
        if (p->vx > top) p->vx = fmaxf(top, p->vx - FRICTION * dt);
        if (p->vx < -top) p->vx = fminf(-top, p->vx + FRICTION * dt);
    } else if (p->on_ground) {
        if (p->vx > 0) p->vx = fmaxf(0, p->vx - FRICTION * dt);
        else p->vx = fminf(0, p->vx + FRICTION * dt);
    }

    /* jumping: buffered presses and a little "coyote time" after ledges */
    if (first_step && in->jump_hit) p->jump_buf = 0.12f;
    else if (p->jump_buf > 0) p->jump_buf -= dt;
    if (p->on_ground) p->coyote = 0.09f;
    else if (p->coyote > 0) p->coyote -= dt;

    if (p->jump_buf > 0 && p->coyote > 0) {
        p->vy = -(JUMP_V + fabsf(p->vx) * 0.12f);
        p->jump_buf = 0;
        p->coyote = 0;
        p->on_ground = 0;
        p->riding = NULL;
    }
    /* letting go of X early makes a short hop */
    if (!in->jump && p->vy < -380) p->vy = -380;

    p->vy += GRAVITY * dt;
    if (p->vy > MAX_FALL) p->vy = MAX_FALL;

    move_player_x(p, p->vx * dt);
    move_player_y(p, p->vy * dt);

    /* keep everybody on screen together */
    if (p->x < cam_x) p->x = cam_x;
    if (p->x > cam_x + SCREEN_W - PW) p->x = cam_x + SCREEN_W - PW;

    collect_tiles(p);

    if (p->y > SCREEN_H + 40) kill_player(p);
}

static void update_enemy(Enemy *e, float dt) {
    if (!e->alive) return;
    if (e->dead_t > 0) {
        e->dead_t -= dt;
        if (e->type == EN_WASP) {
            e->vy += GRAVITY * dt;
            e->y += e->vy * dt;
        }
        if (e->dead_t <= 0) e->alive = 0;
        return;
    }
    /* sleep while far off screen */
    if (e->x < cam_x - 300 || e->x > cam_x + SCREEN_W + 300) return;
    e->t += dt;

    if (e->type == EN_BEETLE) {
        float speed = 80.0f;
        float nx = e->x + e->dir * speed * dt;
        int foot_ty = fdiv(e->y + 28 + 4);
        int ahead_tx = e->dir > 0 ? fdiv(nx + 40) : fdiv(nx);
        int body_ty = fdiv(e->y + 14);
        /* turn at walls and at ledges */
        if (solid_at(ahead_tx, body_ty) || tile_at(ahead_tx, body_ty) == '^' ||
            !(solid_at(ahead_tx, foot_ty) || tile_at(ahead_tx, foot_ty) == '-')) {
            e->dir = -e->dir;
        } else {
            e->x = nx;
        }
    } else {
        /* wasps patrol left and right, bobbing up and down */
        float speed = 110.0f;
        e->x += e->dir * speed * dt;
        if (e->x < e->home_x - 2.5f * TILE) e->dir = 1;
        if (e->x > e->home_x + 2.5f * TILE) e->dir = -1;
        e->y = e->home_y + sinf(e->t * 3.0f) * 30.0f;
    }
}

static void enemy_box(const Enemy *e, float *w, float *h) {
    *w = 40;
    *h = e->type == EN_BEETLE ? 28 : 30;
}

static void check_enemy_hits(Player *p, const Input *in) {
    if (!p->alive) return;
    for (int i = 0; i < n_enemies; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive || e->dead_t > 0) continue;
        float ew, eh;
        enemy_box(e, &ew, &eh);
        if (p->x + PW <= e->x || p->x >= e->x + ew || p->y + PH <= e->y || p->y >= e->y + eh) continue;
        if (p->vy > 0 && p->y + PH - p->vy * (1.0f / 60) <= e->y + 14) {
            /* stomp! */
            e->dead_t = e->type == EN_BEETLE ? 0.5f : 1.5f;
            e->vy = -200;
            p->vy = in->jump ? -JUMP_V : -STOMP_V;
            p->y = e->y - PH;
            spawn_particles(e->x + 20, e->y + 10, 8, 0xfff0d0, 260);
        } else {
            hurt_player(p, e->x + ew / 2);
        }
    }
}

static void update_camera(float dt) {
    float sum = 0, mx = -1e9f, mn = 1e9f;
    int n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &players[i];
        if (!p->active || !p->alive) continue;
        sum += p->x;
        if (p->x > mx) mx = p->x;
        if (p->x < mn) mn = p->x;
        n++;
    }
    if (n == 0) return;
    float target = sum / n - SCREEN_W * 0.42f;
    float max_cam = map_w * TILE - SCREEN_W;
    if (target < 0) target = 0;
    if (target > max_cam) target = max_cam;
    float k = 1.0f - expf(-dt * 6.0f);
    cam_x += (target - cam_x) * k;
    /* never push a player out of the picture */
    if (n > 1) {
        if (cam_x > mn) cam_x = mn;
        if (cam_x + SCREEN_W - PW < mx) cam_x = mx - SCREEN_W + PW;
    }
    if (cam_x < 0) cam_x = 0;
    if (cam_x > max_cam) cam_x = max_cam;
}

static void step_world(const Input in[MAX_PLAYERS], int first, float dt) {
    for (int i = 0; i < n_movers; i++) {
        Mover *m = &movers[i];
        float old = m->x;
        m->x += m->dir * 100.0f * dt;
        if (m->x > m->x0 + m->range) { m->x = m->x0 + m->range; m->dir = -1; }
        if (m->x < m->x0) { m->x = m->x0; m->dir = 1; }
        m->dx = m->x - old;
    }
    for (int i = 0; i < n_enemies; i++) update_enemy(&enemies[i], dt);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;
        update_player(&players[i], &in[i], first, dt);
        check_enemy_hits(&players[i], &in[i]);
        if (state != ST_PLAY) return;
    }
    update_camera(dt);
}

static void update_particles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *q = &parts[i];
        if (q->life <= 0) continue;
        q->life -= dt;
        q->vy += 900 * dt;
        q->x += q->vx * dt;
        q->y += q->vy * dt;
    }
    for (int x = 0; x < MAX_W; x++)
        if (spring_anim[x] > 0) spring_anim[x] -= dt;
}

static void try_join_p2(const Input in[MAX_PLAYERS]) {
    Player *p2 = &players[1];
    if (!in[1].connected) {
        if (p2->active) p2->active = 0;
        return;
    }
    if (p2->active || !in[1].jump_hit) return;
    p2->active = 1;
    Player *p1 = &players[0];
    if (state == ST_PLAY && p1->alive) {
        place_player(p2, p1->x, p1->y - 70);
        p2->inv = 2.0f;
    } else {
        place_player(p2, check_x + 44, check_y);
    }
}

void game_init(void) {
    srand(12345);
    memset(players, 0, sizeof(players));
    load_level(0);
    state = ST_TITLE;
}

void game_update(const Input in[MAX_PLAYERS], float dt) {
    if (dt > 0.05f) dt = 0.05f;
    state_t += dt;
    clock_t_ += dt;
    update_particles(dt);

    int any_start = in[0].start_hit || in[1].start_hit;

    switch (state) {
    case ST_TITLE:
        players[1].active = in[1].connected && players[1].active;
        if (in[1].connected && in[1].jump_hit) players[1].active = 1;
        if (in[0].jump_hit || in[0].start_hit) new_game();
        return;
    case ST_INTRO:
        try_join_p2(in);
        if (state_t > 2.2f || (state_t > 0.5f && (in[0].jump_hit || any_start))) {
            state = ST_PLAY;
            state_t = 0;
        }
        return;
    case ST_PAUSE:
        if (any_start) state = ST_PLAY;
        return;
    case ST_ALLDEAD:
        if (state_t > 1.6f) {
            reset_enemies();
            spawn_all_players(check_x, check_y);
            cam_x = check_x - SCREEN_W * 0.42f;
            if (cam_x < 0) cam_x = 0;
            state = ST_PLAY;
            state_t = 0;
        }
        return;
    case ST_CLEAR:
        if (state_t > 3.0f) {
            if (level_index + 1 >= NUM_LEVELS) {
                state = ST_WIN;
            } else {
                load_level(level_index + 1);
                state = ST_INTRO;
            }
            state_t = 0;
        }
        return;
    case ST_GAMEOVER:
    case ST_WIN:
        if (state_t > 1.5f && (in[0].jump_hit || in[0].start_hit)) {
            state = ST_TITLE;
            state_t = 0;
            load_level(0);
        }
        return;
    case ST_PLAY:
        break;
    }

    if (any_start) { state = ST_PAUSE; return; }
    try_join_p2(in);

    /* small fixed steps keep the jumps the same at any frame rate */
    int steps = (int)ceilf(dt / (1.0f / 120.0f));
    if (steps < 1) steps = 1;
    float h = dt / steps;
    for (int s = 0; s < steps && state == ST_PLAY; s++) step_world(in, s == 0, h);
}

/* ============================================================ render */

typedef struct {
    uint32_t sky_top, sky_bottom;
    uint32_t far_hill, near_hill;
    uint32_t dirt, dirt_dark, grass, grass_light;
    uint32_t stone, stone_light, stone_dark;
    int cave;
} Theme;

static const Theme themes[3] = {
    /* meadow pond, daytime */
    { 0x5ab4f0, 0xc8ecff, 0x8ed0b0, 0x5cb478, 0x9a6438, 0x7a4a28, 0x4ec04a, 0x8ee070,
      0xa0a0b4, 0xd0d0e0, 0x6a6a80, 0 },
    /* crystal cave */
    { 0x0e0a1e, 0x2a1e48, 0x241a3c, 0x342650, 0x5a4a6a, 0x3e3250, 0x7a6ab0, 0xa898e0,
      0x6a6488, 0x9a94b8, 0x40385a, 1 },
    /* sky islands at sunset */
    { 0x3a2a6a, 0xff9a6a, 0xb06a8a, 0x80507a, 0xb87a48, 0x8a5430, 0xf0b44a, 0xffe08a,
      0xc8b0c0, 0xf0e0ec, 0x806078, 0 },
};

static void draw_sky(const Theme *th) {
    const int bands = 24;
    int bh = SCREEN_H / bands;
    for (int i = 0; i < bands; i++) {
        set_rgb(mix(th->sky_top, th->sky_bottom, (float)i / (bands - 1)));
        fill_rect(0, i * bh, SCREEN_W, bh + 1);
    }
}

static float hill_h(float x, float f1, float f2) {
    return sinf(x * f1) * 60 + sinf(x * f2 + 1.3f) * 35;
}

static void draw_hills(float scroll, uint32_t rgb, int base, float f1, float f2, int from_top) {
    set_rgb(rgb);
    for (int sx = 0; sx < SCREEN_W; sx += 8) {
        float wx = sx + cam_x * scroll;
        int h = base + (int)hill_h(wx, f1, f2);
        if (from_top) fill_rect(sx, 0, 8, h);
        else fill_rect(sx, SCREEN_H - h, 8, h);
    }
}

static void draw_cloud(int x, int y, int s, uint32_t rgb) {
    set_rgb(rgb);
    fill_rect(x + s, y, s * 3, s);
    fill_rect(x, y + s, s * 6, s);
    fill_rect(x + s * 4, y + s / 2, s * 2, s / 2 + 1);
}

static void draw_background(const Theme *th) {
    draw_sky(th);
    if (th->cave) {
        /* glowing crystals far away */
        for (int i = 0; i < 18; i++) {
            float wx = i * 173.0f - cam_x * 0.2f;
            int sx = (int)fmodf(wx + 4000, 1500) - 100;
            int sy = 140 + (i * 97) % 340;
            float glow = 0.5f + 0.5f * sinf(clock_t_ * 2 + i);
            set_rgb(mix(0x3a2a6a, 0x9a7af0, glow));
            fill_rect(sx, sy, 6, 14);
            fill_rect(sx - 4, sy + 4, 14, 6);
        }
        draw_hills(0.3f, th->far_hill, 150, 0.006f, 0.017f, 1);
        draw_hills(0.3f, th->far_hill, 220, 0.005f, 0.013f, 0);
        draw_hills(0.55f, th->near_hill, 90, 0.009f, 0.021f, 1);
        draw_hills(0.55f, th->near_hill, 150, 0.008f, 0.019f, 0);
    } else {
        if (theme == 2) {
            set_rgb(0xffe0a0); /* setting sun */
            fill_rect(820 - (int)(cam_x * 0.05f), 380, 160, 120);
            fill_rect(800 - (int)(cam_x * 0.05f), 400, 200, 80);
        }
        uint32_t cloud = theme == 2 ? 0xffc0b0 : 0xffffff;
        for (int i = 0; i < 7; i++) {
            float wx = i * 260.0f - cam_x * 0.15f + clock_t_ * 8;
            int sx = (int)fmodf(wx + 5000, 1820) - 200;
            draw_cloud(sx, 60 + (i * 53) % 180, 16 + (i % 3) * 6, cloud);
        }
        draw_hills(0.25f, th->far_hill, 300, 0.004f, 0.011f, 0);
        draw_hills(0.5f, th->near_hill, 210, 0.007f, 0.016f, 0);
    }
}

static void draw_tile(int tx, int ty, char c, const Theme *th) {
    int x = tx * TILE - (int)cam_x, y = ty * TILE;
    unsigned hsh = (unsigned)(tx * 73856093u) ^ (unsigned)(ty * 19349663u);
    switch (c) {
    case '#': {
        set_rgb(th->dirt);
        fill_rect(x, y, TILE, TILE);
        set_rgb(th->dirt_dark);
        fill_rect(x + 6 + hsh % 20, y + 18 + (hsh >> 5) % 18, 6, 6);
        fill_rect(x + 26 + (hsh >> 9) % 14, y + 8 + (hsh >> 13) % 26, 6, 6);
        if (!is_solid(tile_at(tx, ty - 1)) || ty == 0) {
            set_rgb(th->grass);
            fill_rect(x, y, TILE, 14);
            fill_rect(x + 4, y + 14, 8, 4);
            fill_rect(x + 22, y + 14, 6, 6);
            fill_rect(x + 38, y + 14, 8, 4);
            set_rgb(th->grass_light);
            fill_rect(x, y, TILE, 4);
        }
        break;
    }
    case 'B':
        set_rgb(th->stone);
        fill_rect(x, y, TILE, TILE);
        set_rgb(th->stone_light);
        fill_rect(x, y, TILE, 5);
        fill_rect(x, y, 5, TILE);
        set_rgb(th->stone_dark);
        fill_rect(x, y + TILE - 5, TILE, 5);
        fill_rect(x + TILE - 5, y, 5, TILE);
        fill_rect(x + 14 + hsh % 10, y + 16, 4, 14);
        break;
    case '-':
        set_rgb(0x8a5a30);
        fill_rect(x, y, TILE, 16);
        set_rgb(0xc08850);
        fill_rect(x, y, TILE, 5);
        set_rgb(0x5a3418);
        fill_rect(x + TILE - 3, y, 3, 16);
        fill_rect(x + 10, y + 20, 6, 6);
        fill_rect(x + 32, y + 20, 6, 6);
        break;
    case 'S': {
        int squash = spring_anim[tx] > 0 ? 12 : 0;
        set_rgb(0x50506a);
        fill_rect(x + 4, y + TILE - 10, TILE - 8, 10);
        set_rgb(0xd0d0e0);
        for (int i = 0; i < 3; i++) fill_rect(x + 12, y + 18 + squash / 2 + i * (8 - squash / 4), TILE - 24, 4);
        set_rgb(0xff6a4a);
        fill_rect(x + 2, y + 8 + squash, TILE - 4, 10);
        set_rgb(0xffb08a);
        fill_rect(x + 2, y + 8 + squash, TILE - 4, 3);
        break;
    }
    case '^':
        for (int i = 0; i < 3; i++) {
            int sx = x + i * 16;
            set_rgb(0xc8d0e0);
            for (int k = 0; k < 6; k++) fill_rect(sx + 7 - k, y + 18 + k * 5, 2 + k * 2, 5);
            set_rgb(0x7880a0);
            fill_rect(sx, y + TILE - 3, 16, 3);
        }
        break;
    case 'o': {
        int bob = (int)(sinf(clock_t_ * 4 + tx * 0.7f) * 4);
        draw_sprite(gem_img, 7, x + 12, y + 10 + bob, 3, 0, gem_pal, 4);
        if (fmodf(clock_t_ + tx * 0.37f, 2.0f) < 0.15f) {
            set_rgb(0xffffff);
            fill_rect(x + 16, y + 12 + bob, 4, 4);
        }
        break;
    }
    case 'h': {
        int bob = (int)(sinf(clock_t_ * 3) * 4);
        draw_sprite(heart_img, 7, x + 10, y + 10 + bob, 4, 0, heart_pal, 2);
        break;
    }
    case 'C': {
        int lit = tx <= check_col;
        set_rgb(0x4a3a30);
        fill_rect(x + 21, y + 10, 6, TILE - 10);
        fill_rect(x + 12, y + TILE - 4, 24, 4);
        set_rgb(0x2a2030);
        fill_rect(x + 12, y - 22, 24, 32);
        set_rgb(lit ? mix(0xffd84a, 0xfff4c0, 0.5f + 0.5f * sinf(clock_t_ * 6)) : 0x6a6a7a);
        fill_rect(x + 16, y - 18, 16, 24);
        set_rgb(0x2a2030);
        fill_rect(x + 18, y - 28, 12, 6);
        break;
    }
    case 'G': {
        /* the golden lotus: two tiles tall, glowing */
        float g = 0.5f + 0.5f * sinf(clock_t_ * 3);
        set_rgb(0x3a8a4a);
        fill_rect(x + 21, y - 10, 6, TILE + 10);
        set_rgb(0x2e7a3e);
        fill_rect(x + 4, y + TILE - 10, 40, 10);
        int cy = y - 50;
        set_rgb(mix(0xff7ab0, 0xffb0d0, g));
        fill_rect(x - 6, cy + 18, 20, 18);
        fill_rect(x + 34, cy + 18, 20, 18);
        set_rgb(mix(0xff90c0, 0xffd0e4, g));
        fill_rect(x + 4, cy + 4, 16, 32);
        fill_rect(x + 28, cy + 4, 16, 32);
        set_rgb(mix(0xffd84a, 0xfff6b0, g));
        fill_rect(x + 16, cy - 6, 16, 42);
        set_rgb(0xfff8d0);
        fill_rect(x + 20, cy + 18, 8, 8);
        if (fmodf(clock_t_, 1.0f) < 0.5f) {
            fill_rect(x - 10, cy - 10, 4, 4);
            fill_rect(x + 56, cy + 2, 4, 4);
        } else {
            fill_rect(x + 50, cy - 16, 4, 4);
            fill_rect(x - 4, cy + 8, 4, 4);
        }
        break;
    }
    }
}

static void draw_world(void) {
    const Theme *th = &themes[theme];
    draw_background(th);

    int tx0 = fdiv(cam_x) - 1, tx1 = fdiv(cam_x + SCREEN_W) + 1;
    for (int ty = 0; ty < ROWS; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            char c = tile_at(tx, ty);
            if (tx >= 0 && tx < map_w && c != ' ') draw_tile(tx, ty, c, th);
        }

    /* moving ledges */
    for (int i = 0; i < n_movers; i++) {
        int x = (int)(movers[i].x - cam_x), y = (int)movers[i].y;
        set_rgb(0x6a4a8a);
        fill_rect(x, y, 3 * TILE, 18);
        set_rgb(0xa88ad0);
        fill_rect(x, y, 3 * TILE, 5);
        set_rgb(0xffe070);
        fill_rect(x + 10, y + 8, 6, 6);
        fill_rect(x + 3 * TILE - 16, y + 8, 6, 6);
    }

    /* enemies */
    for (int i = 0; i < n_enemies; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive) continue;
        int x = (int)(e->x - cam_x) - 1, y = (int)e->y;
        if (x < -80 || x > SCREEN_W + 20) continue;
        int flip = e->dir < 0;
        if (e->type == EN_BEETLE) {
            if (e->dead_t > 0) draw_sprite(beetle_flat, 3, x, y + 19, 3, flip, beetle_pal, 4);
            else draw_sprite(fmodf(e->t, 0.3f) < 0.15f ? beetle_a : beetle_b, 9, x, y + 1, 3, flip, beetle_pal, 4);
        } else {
            draw_sprite(fmodf(e->t, 0.1f) < 0.05f ? wasp_a : wasp_b, 9, x, y + 2, 3, flip, wasp_pal, 3);
        }
    }

    /* players */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &players[i];
        if (!p->active || !p->alive) continue;
        if (p->inv > 0 && fmodf(p->inv, 0.16f) < 0.08f) continue; /* blink */
        const char **img = frog_stand;
        if (!p->on_ground) img = frog_jump;
        else if (fabsf(p->vx) > 20 && fmodf(p->anim, 0.24f) < 0.12f) img = frog_walk;
        int x = (int)(p->x - cam_x) - 8, y = (int)p->y - 2;
        draw_sprite(img, 14, x, y, 3, p->face < 0, frog_pal[i], 6);
        if (players[1].active) { /* little marker so you know who is who */
            set_rgb(i == 0 ? 0x5cc94a : 0x4aa8f0);
            draw_text_raw(i == 0 ? "1" : "2", x + 18, y - 26, 2);
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *q = &parts[i];
        if (q->life <= 0) continue;
        set_rgb(q->rgb);
        fill_rect((int)(q->x - cam_x), (int)q->y, q->size, q->size);
    }
}

static void draw_hud(void) {
    char buf[32];
    /* hearts for each player */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &players[i];
        if (!p->active) continue;
        int y = 20 + i * 44;
        draw_text(i == 0 ? "J1" : "J2", 24, y + 4, 4, i == 0 ? 0x8ef07a : 0x8ac8ff);
        for (int h = 0; h < MAX_HEARTS; h++) {
            int filled = p->alive && h < p->hearts;
            draw_sprite(heart_img, 7, 84 + h * 36, y, 4, 0, filled ? heart_pal : heart_empty_pal, 2);
        }
    }
    draw_sprite(gem_img, 7, 230, 22, 4, 0, gem_pal, 4);
    snprintf(buf, sizeof(buf), "x%d", gems);
    draw_text(buf, 270, 26, 4, 0xffffff);

    draw_sprite(frog_stand, 14, 380, 16, 2, 0, frog_pal[0], 6);
    snprintf(buf, sizeof(buf), "x%d", lives);
    draw_text(buf, 418, 26, 4, 0xffffff);

    const char *name = level_defs[level_index].name;
    draw_text(name, SCREEN_W - 24 - text_width(name, 4), 26, 4, 0xfff0b0);

    if (!players[1].active && fmodf(clock_t_, 4.0f) < 2.5f)
        draw_text("J2: PULSA X PARA UNIRTE", SCREEN_W - 24 - text_width("J2: PULSA X PARA UNIRTE", 3), 70, 3, 0xd0d8ff);
}

static void draw_title(void) {
    theme = 0;
    cam_x = clock_t_ * 40;
    const Theme *th = &themes[0];
    draw_background(th);
    /* scrolling ground strip */
    for (int tx = 0; tx < 30; tx++) {
        int x = tx * TILE - ((int)cam_x % TILE);
        set_rgb(th->dirt);
        fill_rect(x, 12 * TILE, TILE, 3 * TILE);
        set_rgb(th->grass);
        fill_rect(x, 12 * TILE, TILE, 14);
        set_rgb(th->grass_light);
        fill_rect(x, 12 * TILE, TILE, 4);
    }

    int hop = (int)fabsf(sinf(clock_t_ * 3.0f) * 70);
    draw_sprite(hop > 10 ? frog_jump : frog_stand, 14, 280, 12 * TILE - 70 - hop, 5, 0, frog_pal[0], 6);
    if (players[1].active) {
        int hop2 = (int)fabsf(sinf(clock_t_ * 3.0f + 1.2f) * 70);
        draw_sprite(hop2 > 10 ? frog_jump : frog_stand, 14, 920, 12 * TILE - 70 - hop2, 5, 1, frog_pal[1], 6);
    }
    draw_sprite(gem_img, 7, 640 - 16, 470 + (int)(sinf(clock_t_ * 4) * 6), 4, 0, gem_pal, 4);

    draw_text_centered("RANO", 70, 26, 0x8ef07a);
    draw_text_centered("SALTARIN", 270, 10, 0xfff0b0);
    if (fmodf(clock_t_, 1.0f) < 0.65f) draw_text_centered("PULSA X PARA JUGAR", 380, 6, 0xffffff);
    draw_text_centered(players[1].active ? "JUGADOR 2 LISTO!" : "JUGADOR 2: PULSA X EN EL SEGUNDO CONTROL",
                       640, 3, players[1].active ? 0x8ac8ff : 0xe0e8ff);
    draw_text_centered("X SALTAR    CUADRADO CORRER    START PAUSA", 680, 3, 0xe0e8ff);
}

static void draw_panel(int y, int h) {
    set_rgb(0x1a1430);
    fill_rect(0, y, SCREEN_W, h);
    set_rgb(0x8ef07a);
    fill_rect(0, y, SCREEN_W, 4);
    fill_rect(0, y + h - 4, SCREEN_W, 4);
}

void game_render(void) {
    char buf[48];
    if (state == ST_TITLE) {
        draw_title();
        return;
    }
    if (state == ST_WIN) {
        theme = 2;
        draw_background(&themes[2]);
        draw_text_centered("HAS GANADO!", 140, 14, 0xffd84a);
        draw_sprite(frog_jump, 14, 560, 300 + (int)(sinf(clock_t_ * 5) * 20), 5, 0, frog_pal[0], 6);
        if (players[1].active)
            draw_sprite(frog_jump, 14, 660, 300 + (int)(sinf(clock_t_ * 5 + 1) * 20), 5, 1, frog_pal[1], 6);
        draw_text_centered("RANO ENCONTRO TODOS LOS LOTOS DORADOS", 450, 4, 0xffffff);
        snprintf(buf, sizeof(buf), "GEMAS: %d    VIDAS: %d", gems, lives);
        draw_text_centered(buf, 520, 5, 0x9af4ff);
        if (state_t > 1.5f && fmodf(clock_t_, 1.0f) < 0.65f) draw_text_centered("PULSA X", 620, 5, 0xffffff);
        return;
    }

    draw_world();
    draw_hud();

    switch (state) {
    case ST_INTRO:
        draw_panel(250, 220);
        snprintf(buf, sizeof(buf), "NIVEL %d", level_index + 1);
        draw_text_centered(buf, 285, 10, 0xfff0b0);
        draw_text_centered(level_defs[level_index].name, 385, 7, 0x8ef07a);
        break;
    case ST_PAUSE:
        draw_panel(270, 180);
        draw_text_centered("PAUSA", 300, 12, 0xffffff);
        draw_text_centered("START PARA SEGUIR", 400, 4, 0xd0d8ff);
        break;
    case ST_CLEAR:
        draw_panel(250, 220);
        draw_text_centered("NIVEL COMPLETADO!", 290, 8, 0xffd84a);
        snprintf(buf, sizeof(buf), "GEMAS %d / %d", gems_level, gems_level_total);
        draw_text_centered(buf, 390, 5, 0x9af4ff);
        break;
    case ST_GAMEOVER:
        draw_panel(250, 220);
        draw_text_centered("FIN DEL JUEGO", 290, 10, 0xff6a7a);
        if (state_t > 1.5f && fmodf(clock_t_, 1.0f) < 0.65f) draw_text_centered("PULSA X", 400, 5, 0xffffff);
        break;
    default:
        break;
    }
}

#ifdef PC_TEST
/* hooks for the desktop test harness */
void test_load_level(int idx) { players[0].active = 1; load_level(idx); state = ST_PLAY; }
void test_set_camera(float x) { cam_x = x; }
int test_level_width(void) { return map_w * TILE; }
int test_state(void) { return (int)state; }
float test_player_x(int i) { return players[i].x; }
float test_player_y(int i) { return players[i].y; }
void test_set_player(int i, float x, float y) { players[i].x = x; players[i].y = y; players[i].vx = players[i].vy = 0; }
#endif
