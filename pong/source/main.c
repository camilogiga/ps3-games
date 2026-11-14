/*
 * PS3 Pong - a tiny first homebrew game.
 * PSL1GHT + SDL2, drawn at 1280x720.
 *
 * Controls (controller 1 = left paddle):
 *   Left stick / D-pad up-down : move paddle
 *   X (Cross)                  : serve / start / play again
 *   START                      : pause
 *   Controller 2 (optional)    : press X to take over the right paddle
 *   PS button -> Quit Game     : exit to XMB
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <io/pad.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 1280
#define SCREEN_H 720

#define PADDLE_W 20
#define PADDLE_H 120
#define PADDLE_MARGIN 60
#define PADDLE_SPEED 720.0f /* pixels per second */
#define CPU_SPEED 470.0f
#define BALL_SIZE 18
#define BALL_START_SPEED 520.0f
#define BALL_MAX_SPEED 1300.0f
#define WIN_SCORE 7

/* ---------------------------------------------------------------- input */

typedef struct {
    int connected;
    int up, down;         /* held */
    float stick;          /* -1 (up) .. +1 (down) */
    int cross, start;     /* held */
    int cross_hit, start_hit; /* pressed this frame */
} Pad;

static Pad pads[2];
static padData last_data[2];

static void read_pads(void) {
    padInfo info;
    if (ioPadGetInfo(&info) != 0) return;

    for (int i = 0; i < 2; i++) {
        Pad *p = &pads[i];
        int old_cross = p->cross, old_start = p->start;
        p->connected = info.status[i] ? 1 : 0;
        if (!p->connected) {
            memset(p, 0, sizeof(*p));
            memset(&last_data[i], 0, sizeof(last_data[i]));
            continue;
        }
        padData d;
        /* len == 0 means "no change since last read": keep last state */
        if (ioPadGetData(i, &d) == 0 && d.len > 0) last_data[i] = d;
        padData *s = &last_data[i];
        if (s->len == 0) continue;

        p->up = s->BTN_UP;
        p->down = s->BTN_DOWN;
        p->cross = s->BTN_CROSS;
        p->start = s->BTN_START;
        float v = ((int)s->ANA_L_V - 128) / 127.0f;
        if (fabsf(v) < 0.2f) v = 0.0f; /* dead zone */
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        p->stick = v;
        p->cross_hit = p->cross && !old_cross;
        p->start_hit = p->start && !old_start;
    }
}

/* -------------------------------------------------------- tiny 3x5 font */

static const char *glyph(char c) {
    switch (c) {
    case '0': return "111101101101111";
    case '1': return "010110010010111";
    case '2': return "111001111100111";
    case '3': return "111001111001111";
    case '4': return "101101111001001";
    case '5': return "111100111001111";
    case '6': return "111100111101111";
    case '7': return "111001001001001";
    case '8': return "111101111101111";
    case '9': return "111101111001111";
    case 'A': return "010101111101101";
    case 'C': return "111100100100111";
    case 'D': return "110101101101110";
    case 'E': return "111100111100111";
    case 'G': return "111100101101111";
    case 'I': return "111010010010111";
    case 'L': return "100100100100111";
    case 'N': return "101111111111101";
    case 'O': return "111101101101111";
    case 'P': return "111101111100100";
    case 'R': return "110101110101101";
    case 'S': return "111100111001111";
    case 'T': return "111010010010010";
    case 'U': return "101101101101111";
    case 'W': return "101101111111101";
    case 'X': return "101101010101101";
    case 'Y': return "101101010010010";
    case '!': return "010010010000010";
    case '-': return "000000111000000";
    default:  return "000000000000000";
    }
}

static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n > 0 ? n * 4 * scale - scale : 0;
}

static void draw_text(SDL_Renderer *r, const char *s, int x, int y, int scale) {
    for (; *s; s++, x += 4 * scale) {
        const char *g = glyph(*s);
        for (int i = 0; i < 15; i++) {
            if (g[i] == '1') {
                SDL_Rect px = { x + (i % 3) * scale, y + (i / 3) * scale, scale, scale };
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

static void draw_text_centered(SDL_Renderer *r, const char *s, int y, int scale) {
    draw_text(r, s, (SCREEN_W - text_width(s, scale)) / 2, y, scale);
}

/* ----------------------------------------------------------------- game */

typedef enum { STATE_TITLE, STATE_SERVE, STATE_PLAY, STATE_PAUSE, STATE_OVER } State;

typedef struct {
    float y;          /* top edge */
    int score;
    int human;        /* right paddle: 0 = CPU, 1 = controller 2 */
} Paddle;

static Paddle left, right;
static float ball_x, ball_y, ball_vx, ball_vy;
static int serve_dir = 1;
static State state = STATE_TITLE;
static float blink = 0.0f;

static void reset_ball(void) {
    ball_x = (SCREEN_W - BALL_SIZE) / 2.0f;
    ball_y = (SCREEN_H - BALL_SIZE) / 2.0f;
    ball_vx = ball_vy = 0.0f;
}

static void launch_ball(void) {
    float angle = ((rand() % 61) - 30) * (3.14159f / 180.0f);
    ball_vx = cosf(angle) * BALL_START_SPEED * serve_dir;
    ball_vy = sinf(angle) * BALL_START_SPEED;
}

static void new_match(void) {
    left.score = right.score = 0;
    left.y = right.y = (SCREEN_H - PADDLE_H) / 2.0f;
    serve_dir = (rand() & 1) ? 1 : -1;
    reset_ball();
    state = STATE_SERVE;
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void move_human(Paddle *p, const Pad *pad, float dt) {
    float dir = pad->stick;
    if (pad->up) dir = -1.0f;
    if (pad->down) dir = 1.0f;
    p->y = clampf(p->y + dir * PADDLE_SPEED * dt, 0, SCREEN_H - PADDLE_H);
}

static void move_cpu(Paddle *p, float dt) {
    float target = (ball_vx > 0) ? ball_y + BALL_SIZE / 2.0f : SCREEN_H / 2.0f;
    float center = p->y + PADDLE_H / 2.0f;
    float diff = target - center;
    float step = CPU_SPEED * dt;
    if (fabsf(diff) > 10.0f) p->y += (diff > 0 ? step : -step);
    p->y = clampf(p->y, 0, SCREEN_H - PADDLE_H);
}

/* Bounce off a paddle; angle depends on where the ball hits it. */
static void bounce(const Paddle *p, int going_right) {
    float rel = ((ball_y + BALL_SIZE / 2.0f) - (p->y + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
    rel = clampf(rel, -1.0f, 1.0f);
    float speed = sqrtf(ball_vx * ball_vx + ball_vy * ball_vy) * 1.06f;
    if (speed > BALL_MAX_SPEED) speed = BALL_MAX_SPEED;
    float angle = rel * (55.0f * 3.14159f / 180.0f);
    ball_vx = cosf(angle) * speed * (going_right ? 1 : -1);
    ball_vy = sinf(angle) * speed;
}

static void update(float dt) {
    blink += dt;

    /* Second controller can join any time by pressing X */
    if (!right.human && pads[1].connected && pads[1].cross_hit) right.human = 1;
    if (right.human && !pads[1].connected) right.human = 0;

    switch (state) {
    case STATE_TITLE:
        if (pads[0].cross_hit) new_match();
        return;
    case STATE_OVER:
        if (pads[0].cross_hit) new_match();
        return;
    case STATE_PAUSE:
        if (pads[0].start_hit) state = STATE_PLAY;
        return;
    case STATE_SERVE:
        move_human(&left, &pads[0], dt);
        if (right.human) move_human(&right, &pads[1], dt); else move_cpu(&right, dt);
        if (pads[0].cross_hit || (right.human && pads[1].cross_hit)) {
            launch_ball();
            state = STATE_PLAY;
        }
        return;
    case STATE_PLAY:
        break;
    }

    if (pads[0].start_hit) { state = STATE_PAUSE; return; }

    move_human(&left, &pads[0], dt);
    if (right.human) move_human(&right, &pads[1], dt); else move_cpu(&right, dt);

    ball_x += ball_vx * dt;
    ball_y += ball_vy * dt;

    /* top / bottom walls */
    if (ball_y < 0) { ball_y = 0; ball_vy = fabsf(ball_vy); }
    if (ball_y > SCREEN_H - BALL_SIZE) { ball_y = SCREEN_H - BALL_SIZE; ball_vy = -fabsf(ball_vy); }

    /* left paddle */
    float lx = PADDLE_MARGIN;
    if (ball_vx < 0 && ball_x <= lx + PADDLE_W && ball_x + BALL_SIZE >= lx &&
        ball_y + BALL_SIZE >= left.y && ball_y <= left.y + PADDLE_H) {
        ball_x = lx + PADDLE_W;
        bounce(&left, 1);
    }
    /* right paddle */
    float rx = SCREEN_W - PADDLE_MARGIN - PADDLE_W;
    if (ball_vx > 0 && ball_x + BALL_SIZE >= rx && ball_x <= rx + PADDLE_W &&
        ball_y + BALL_SIZE >= right.y && ball_y <= right.y + PADDLE_H) {
        ball_x = rx - BALL_SIZE;
        bounce(&right, 0);
    }

    /* scoring */
    if (ball_x + BALL_SIZE < 0 || ball_x > SCREEN_W) {
        if (ball_x > SCREEN_W) { left.score++; serve_dir = -1; }
        else { right.score++; serve_dir = 1; }
        reset_ball();
        state = (left.score >= WIN_SCORE || right.score >= WIN_SCORE) ? STATE_OVER : STATE_SERVE;
    }
}

static void render(SDL_Renderer *r) {
    SDL_SetRenderDrawColor(r, 10, 12, 30, 255);
    SDL_RenderClear(r);

    if (state == STATE_TITLE) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        draw_text_centered(r, "PONG", 180, 30);
        if (fmodf(blink, 1.0f) < 0.65f)
            draw_text_centered(r, "PRESS X TO START", 480, 8);
        SDL_SetRenderDrawColor(r, 120, 130, 170, 255);
        draw_text_centered(r, "2ND CONTROLLER - PRESS X TO PLAY", 600, 4);
        return;
    }

    /* center dashed line */
    SDL_SetRenderDrawColor(r, 70, 80, 120, 255);
    for (int y = 10; y < SCREEN_H; y += 40) {
        SDL_Rect dash = { SCREEN_W / 2 - 3, y, 6, 22 };
        SDL_RenderFillRect(r, &dash);
    }

    /* scores */
    char buf[8];
    SDL_SetRenderDrawColor(r, 200, 210, 255, 255);
    SDL_snprintf(buf, sizeof(buf), "%d", left.score);
    draw_text(r, buf, SCREEN_W / 2 - 80 - text_width(buf, 14), 40, 14);
    SDL_snprintf(buf, sizeof(buf), "%d", right.score);
    draw_text(r, buf, SCREEN_W / 2 + 80, 40, 14);

    /* paddles */
    SDL_SetRenderDrawColor(r, 80, 200, 255, 255);
    SDL_Rect lp = { PADDLE_MARGIN, (int)left.y, PADDLE_W, PADDLE_H };
    SDL_RenderFillRect(r, &lp);
    SDL_SetRenderDrawColor(r, 255, 110, 140, 255);
    SDL_Rect rp = { SCREEN_W - PADDLE_MARGIN - PADDLE_W, (int)right.y, PADDLE_W, PADDLE_H };
    SDL_RenderFillRect(r, &rp);

    /* ball */
    SDL_SetRenderDrawColor(r, 255, 240, 120, 255);
    SDL_Rect b = { (int)ball_x, (int)ball_y, BALL_SIZE, BALL_SIZE };
    SDL_RenderFillRect(r, &b);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    if (state == STATE_SERVE && fmodf(blink, 1.0f) < 0.65f)
        draw_text_centered(r, "PRESS X TO SERVE", 560, 6);
    if (state == STATE_PAUSE)
        draw_text_centered(r, "PAUSED", 300, 14);
    if (state == STATE_OVER) {
        draw_text_centered(r, left.score > right.score ? "YOU WIN!" : (right.human ? "P2 WINS!" : "CPU WINS!"), 260, 14);
        if (fmodf(blink, 1.0f) < 0.65f)
            draw_text_centered(r, "PRESS X TO PLAY AGAIN", 460, 6);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    ioPadInit(7);

    SDL_Window *win = SDL_CreateWindow("Pong", 0, 0, SCREEN_W, SCREEN_H, SDL_WINDOW_FULLSCREEN);
    if (!win) { SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { SDL_DestroyWindow(win); SDL_Quit(); return 1; }
    /* Draw in 1280x720 coordinates whatever the TV is set to */
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);

    srand(SDL_GetTicks());
    reset_ball();
    left.y = right.y = (SCREEN_H - PADDLE_H) / 2.0f;

    int running = 1;
    Uint32 last = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0; /* PS button -> Quit Game */
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;

        read_pads();
        update(dt);
        render(ren);
        SDL_RenderPresent(ren);
    }

    ioPadEnd();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
