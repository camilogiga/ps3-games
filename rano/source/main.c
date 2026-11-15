/*
 * Rano Saltarin - PS3 platform layer (PSL1GHT).
 * The game draws into a 1280x720 buffer in normal RAM; each frame it is
 * copied (and scaled if the TV is not at 720p) to the RSX framebuffer.
 *
 * Controls:
 *   Left stick / D-pad : move        X      : jump (hold for higher)
 *   Square (hold)      : run         START  : pause
 *   Controller 2       : press X to join in
 *   PS button -> Quit Game : back to the XMB
 */
#include <io/pad.h>
#include <lv2/systime.h>
#include <malloc.h>
#include <ppu-types.h>
#include <rsx/rsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <unistd.h>

#include "game.h"

/* ------------------------------------------------------------- graphics */

static gcmContextData *context;
static u32 *fb[2];
static u32 fb_pitch, fb_w, fb_h;
static int fb_cur = 0;
static int *xmap; /* for scaling: screen column for each framebuffer column */

static int gfx_init(void) {
    void *host = memalign(1024 * 1024, 32 * 1024 * 1024);
    if (!host) return -1;
    if (rsxInit(&context, 0x100000, 32 * 1024 * 1024, host) != 0) return -1;

    videoState vstate;
    if (videoGetState(VIDEO_PRIMARY, 0, &vstate) != 0 || vstate.state != 0) return -1;
    u8 res_id = vstate.displayMode.resolution;
    if (videoGetResolutionAvailability(VIDEO_PRIMARY, VIDEO_RESOLUTION_720, VIDEO_ASPECT_AUTO, 0) == 1)
        res_id = VIDEO_RESOLUTION_720;

    videoResolution res;
    if (videoGetResolution(res_id, &res) != 0) return -1;
    fb_w = res.width;
    fb_h = res.height;
    fb_pitch = fb_w * 4;

    videoConfiguration vcfg;
    memset(&vcfg, 0, sizeof(vcfg));
    vcfg.resolution = res_id;
    vcfg.format = VIDEO_BUFFER_FORMAT_XRGB;
    vcfg.pitch = fb_pitch;
    vcfg.aspect = VIDEO_ASPECT_AUTO;
    rsxFinish(context, 1);
    if (videoConfigure(VIDEO_PRIMARY, &vcfg, NULL, 0) != 0) return -1;

    gcmSetFlipMode(GCM_FLIP_VSYNC);
    for (int i = 0; i < 2; i++) {
        u32 offset;
        fb[i] = (u32 *)rsxMemalign(64, fb_pitch * fb_h);
        if (!fb[i]) return -1;
        memset(fb[i], 0, fb_pitch * fb_h);
        if (rsxAddressToOffset(fb[i], &offset) != 0) return -1;
        if (gcmSetDisplayBuffer(i, offset, fb_pitch, fb_w, fb_h) != 0) return -1;
    }
    gcmResetFlipStatus();

    screen = (uint32_t *)memalign(128, SCREEN_W * SCREEN_H * 4);
    xmap = (int *)malloc(sizeof(int) * fb_w);
    if (!screen || !xmap) return -1;
    for (u32 x = 0; x < fb_w; x++) xmap[x] = x * SCREEN_W / fb_w;
    return 0;
}

/* Copy the finished picture to the framebuffer, then flip on vsync.
 * We only ever write to RSX memory (reading it from the CPU is very slow). */
static void gfx_present(void) {
    u32 *dst = fb[fb_cur];
    if (fb_w == SCREEN_W && fb_h == SCREEN_H) {
        memcpy(dst, screen, SCREEN_W * SCREEN_H * 4);
    } else {
        for (u32 y = 0; y < fb_h; y++) {
            const uint32_t *src = screen + (y * SCREEN_H / fb_h) * SCREEN_W;
            u32 *row = dst + y * fb_w;
            for (u32 x = 0; x < fb_w; x++) row[x] = src[xmap[x]];
        }
    }
    gcmSetFlip(context, fb_cur);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
    fb_cur ^= 1;
}

/* ---------------------------------------------------------------- input */

static Input inputs[MAX_PLAYERS];
static padData last_data[MAX_PLAYERS];

static void read_pads(void) {
    padInfo info;
    if (ioPadGetInfo(&info) != 0) return;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        Input *p = &inputs[i];
        int old_jump = p->jump, old_start = p->start;
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
        if (s->len == 0) {
            p->jump_hit = p->start_hit = 0;
            continue;
        }

        int sx = (int)s->ANA_L_H - 128;
        int sy = (int)s->ANA_L_V - 128;
        p->left = s->BTN_LEFT || sx < -50;
        p->right = s->BTN_RIGHT || sx > 50;
        p->down = s->BTN_DOWN || sy > 50;
        p->jump = s->BTN_CROSS;
        p->run = s->BTN_SQUARE;
        p->start = s->BTN_START;
        p->jump_hit = p->jump && !old_jump;
        p->start_hit = p->start && !old_start;
    }
}

/* ----------------------------------------------------------------- main */

static volatile int running = 1;

static void sysutil_cb(u64 status, u64 param, void *usrdata) {
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) running = 0; /* PS button -> Quit Game */
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (gfx_init() != 0) return 1;
    ioPadInit(7);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);

    game_init();
    srand((unsigned)sysGetSystemTime());

    s64 last = sysGetSystemTime();
    while (running) {
        sysUtilCheckCallback();

        s64 now = sysGetSystemTime();
        float dt = (now - last) / 1000000.0f;
        last = now;

        read_pads();
        game_update(inputs, dt);
        game_render();
        gfx_present();
    }

    ioPadEnd();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    rsxFinish(context, 1);
    return 0;
}
