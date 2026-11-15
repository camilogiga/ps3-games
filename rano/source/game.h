/* Rano Saltarin - shared interface between the game and the platform layer */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define MAX_PLAYERS 2

typedef struct {
    int connected;
    int left, right, down;  /* held (D-pad or left stick) */
    int jump, run, start;   /* held: X, Square, START */
    int jump_hit, start_hit; /* pressed this frame */
} Input;

/* The game draws into this 1280x720 0x00RRGGBB buffer; the platform shows it */
extern uint32_t *screen;

void game_init(void);
void game_update(const Input in[MAX_PLAYERS], float dt);
void game_render(void);

#endif
