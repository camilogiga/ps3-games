# PS3 games

Simple 2D homebrew games for PS3 (PS3HEN on HFW 4.93), written in C with PSL1GHT.

Every push to `main` builds all games on GitHub Actions. The newest `.pkg` files are on the **Releases** page (release called "Latest PS3 games").

## Install on PS3
1. Download the `.pkg` from Releases.
2. Copy it to the root of a FAT32 USB stick.
3. On the PS3, enable HEN, then open **Package Manager → Install Package Files → Standard Package Location** and pick the game.

## Games
- **pong/** — Pong. Left stick or D-pad moves, X serves, START pauses. A second controller can press X to take the right paddle.

## Adding a game
Make a new folder (copy `pong/` as a template). Each folder with a `Makefile` is built automatically; the `.pkg` is named after the folder. Give each game a unique 9-character `APPID`.

## Notes for future games
- Don't use the SDL2 renderer (`SDL_CreateRenderer`) from the PSL1GHT SDL2 port: it corrupts rectangle coordinates and only applies the last draw color. Draw straight to the RSX framebuffer like `pong/source/main.c` does (`gfx_init`, `fill_rect`, `gfx_flip`).
- Read the controller with PSL1GHT's `ioPad*` directly. `ioPadGetData` returns `len == 0` when nothing changed, so keep the previous state.
- Tested working on a CECH-4211C Super Slim with HFW 4.93.1 + PS3HEN.
