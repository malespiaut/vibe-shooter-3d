# CLAUDE.md

## Goal

Build a **complete, shippable, fun first-person shooter** using early-90s techniques
(think 1992–1994 PC era), with a modern compiler and a thin platform layer.
This is a game, not a tech demo: every feature is judged by whether it makes
the finished game better to play.

## Hard constraints (never break these)

1. **Everything from scratch**, including all assets (textures, sprites, sounds,
   music, levels, fonts). No third-party assets, no third-party libraries except
   the platform layer. Assets are produced by our own code/tools or authored by
   hand in this repo.
2. **All rendering by hand**: we write every pixel into an 8-bit framebuffer.
   No GPU APIs, no SDL rendering helpers (no `SDL_RenderLine`, `SDL_BlitSurface`,
   scaling, etc.) beyond presenting the final frame.
3. **All sound mixing by hand**: we mix every sample ourselves into the audio buffer.
   No SDL_mixer, no SDL audio conversion/mixing helpers.
4. **320×240** internal resolution.
5. **256 colors**: framebuffer is `uint8_t[320*240]` indices into one 256-entry
   palette. Lighting, fog, translucency, and effects go through precomputed
   lookup tables (colormaps), like the era did. Palette swaps for damage/pickup
   flashes are allowed.
6. **Numbers**:
   - **Game logic uses fixed point only** (16.16 `int32_t`, type `fixed_t`).
     No `float`/`double` anywhere in simulation code: movement, collision,
     AI, weapons, damage, RNG, timers. This guarantees determinism across
     platforms and compilers.
   - **Rendering may use floating point**; determinism there is not required.
     Rendering must never write back into game state.
   - Game RNG is our own deterministic PRNG (table or LCG), seeded explicitly.
     Never `rand()`.
7. **Platform layer is pretend-limited.** The game may only use:
   - a framebuffer to write pixels into (+ setting the 256-color palette)
   - keyboard/mouse input
   - an audio buffer to write samples into
   - filesystem I/O (read/write whole files)
   - a monotonic clock (needed to pace the fixed tick)

   Everything else (text, UI, menus, video modes, config, save games) is ours.

## Architecture

```
src/
  platform.h        the ONLY interface between game and platform (see below)
  platform_sdl3.c   the ONLY file allowed to include SDL
  main.c            fixed-timestep loop
  fixed.h           fixed_t, mul/div, trig tables (sin/cos LUT), sqrt
  ...               game modules (render, sound, world, actors, ui, ...)
tools/              our own asset generators/converters (built natively, may use float)
assets/             source assets in human-editable formats, committed
```

- **Platform backend:** SDL3 (installed). `platform_sdl3.c` converts our
  8-bit framebuffer + palette to whatever SDL needs, scales it to the window
  (integer scale, aspect-correct), and feeds our already-mixed audio to SDL.
  That conversion/upscale is the "video card", not our renderer, so it is allowed.
- **No game code may include SDL headers.** Grep to verify:
  `grep -l SDL src/*.c src/*.h` must list only `platform_sdl3.c`.
- **Main loop:** fixed simulation tick (35 Hz, Doom-style) in fixed point;
  render as often as possible, without interpolation unless it's float-only on the
  render side. Input is sampled into a per-tick command struct (`ticcmd`-style)
  so a demo = seed + list of commands, and replays must reproduce exactly.
- **Audio:** game mixes N channels of 8- or 16-bit mono samples into a signed
  16-bit stereo buffer at 22050 or 44100 Hz, with our own volume/pan/pitch, clipping,
  and music synth (tracker-style or simple FM/wavetable, our choice).
- **Assets:** either generated at build time by `tools/` or loaded from
  our own file formats in `assets/`. Prefer procedural generation and small
  hand-written text formats (e.g., palette as hex, levels as text/grid, sprites
  as indexed PPM/our own format) so diffs stay readable.

Suggested `platform.h` contract (keep it this small):

```c
bool  plat_init(const char *title);
void  plat_shutdown(void);
void  plat_set_palette(const uint8_t rgb[256*3]);
void  plat_present(const uint8_t fb[320*240]);
bool  plat_poll(plat_input *in);          // keys down/pressed, mouse dx/dy/buttons; false = quit
void  plat_audio_lock(void); void plat_audio_unlock(void);
void  plat_set_audio_callback(void (*mix)(int16_t *out, int frames), int rate);
uint64_t plat_ticks_us(void);
long  plat_read_file(const char *path, void **data);   // caller frees
bool  plat_write_file(const char *path, const void *data, long size);
```

## Rendering approach

Pick the technique that serves the game, staying within era norms:
raycaster (Wolfenstein-style) → sector/portal renderer (Doom/Build-style) is the
expected range. Textured walls/floors/ceilings, billboard sprites, distance
shading through colormaps, a status bar/HUD, a hand-drawn bitmap font.
Avoid anything that needs true-color or a z-buffer per pixel unless it's cheap
and in-character (a per-column depth buffer for sprite clipping is fine).

## Scope / definition of done

A finished game has, at minimum:
- title screen, main menu, options (mouse sensitivity, volume, key bindings),
  pause, quit confirmation
- multiple distinct levels (aim: one episode of ~6–9 levels) with a clear
  progression, keys/doors, secrets, and an end screen
- several weapons that feel different, several enemy types with readable
  behaviors, pickups (health, armor, ammo)
- sound effects for every action and music
- difficulty levels, save/load, per-level stats (kills/items/secrets/time)
- no crashes, no softlocks, consistent framerate on modest hardware

Prioritize a playable vertical slice early (one level, one weapon, one enemy,
sound), then widen. Play-feel (movement, weapon feedback, enemy readability)
beats feature count.

## Code conventions

- C11, compiled with `-std=c11 -Wall -Wextra -Wpedantic -Werror` on gcc and clang.
- Plain C, no dynamic allocation in the per-frame/per-tick path; allocate at
  level load.
- Keep modules small and data-oriented; no abstraction layers "for later".
- Fixed-point helpers live in `fixed.h`; never hand-roll `>> 16` math elsewhere.
- Determinism self-check: a test that runs a recorded demo headless (no platform)
  and compares a checksum of game state against a committed value. Run it after
  any change to simulation code. Run it under both gcc and clang to catch
  compiler-dependent behavior.
- Avoid UB in fixed math: use `int64_t` intermediates for mul/div, no signed
  overflow, no shifting negative numbers left.

## Build & run

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release            # gcc (default)
cmake -B build-clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
cmake --build build -j && cmake --build build-clang -j
./build/shooter                    # --warp N / --record FILE / --playdemo FILE (paths relative to the exe dir)
ctest --test-dir build && ctest --test-dir build-clang   # determinism, levelcheck, soak
./build/levelcheck assets/levels/*.txt                 # closed maps, door axes, keys before doors, exit reachable
./build/snapshot LEVEL OUT.ppm [x y angle [tics]]   # headless screenshot
./build/snapshot --sprites OUT.ppm [first] | --textures OUT.ppm   # art contact sheets
```

- Determinism test replays `assets/demos/demo1.txt`. After an intentional sim change,
  re-record (drive the game with `--record`) or keep the demo and update `EXPECTED`
  in `tests/demo_test.c` from `./build/demo_test --print` — only after checking that
  gcc and clang print the same value.
- Sanitizer pass after risky changes:
  `cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1"`
  then `./build-asan/soak 3000` (bot plays every level/skill while rendering).
- To check on screen without grabbing the desktop: run under `Xvfb :77` with
  `SDL_AUDIO_DRIVER=dummy`, drive with `xdotool`, capture with `import -window root`.

## Decisions

- Game name: **IRONHOLD**. Episode 1 "The Breach".
- Renderer: Wolfenstein-style grid raycaster (sliding doors on the tile mid-plane,
  pushwall secrets, door-frame textures), textured floor/ceiling, 75° FOV, view
  320x208 + 32px status bar. 32 light levels via colormaps.
- Palette: 16 ramps x 16 shades generated in `gfx.c`; index 255 = transparent.
- All art (textures, sprites, weapons) is procedural in `gfx.c`; font hand-drawn in `font.c`.
- Sim actors use a Doom-like state table (`sdef` in `game.c`): `action` on entry,
  `think` every tic. `game_t g` holds no pointers, so it can be saved by memcpy.
- Sim talks to sound only through the `g.snd` event queue (positional mixing in `main.c`).
- Audio: 22050 Hz stereo; SFX synthesized at startup; music is our text tracker
  format in `assets/music/*.txt` (4 channels: bass, lead, pad, drums).
- Levels: text grids in `assets/levels/`; legend at the top of `e1m1.txt`. Optional
  `music:` header and `skill:` overlay grid (1 easy only, 2 medium+, 3 hard only).
  A level with the Warden (`w`) ends when it dies and needs no exit switch.
- Weapon refire runs on entry to the last weapon state (Doom-style), skipping its
  tics: put a weapon's cooldown in the firing state, not the refire state.
- Save games: header + raw `game_t`; size check rejects other builds; `g_validate()`
  must accept the state before it goes live. Add new indices to it.
- Key taps are latched until the next tic (`latch_taps` in main.c).

## Workflow

- Repo: https://github.com/malespiaut/vibe-shooter-3d (public).
- Work in small, playable increments; commit after each one with a clear message.
- Before committing: build with zero warnings on gcc and clang, run tests,
  run the SDL-containment grep above, and actually launch the game to check
  the change on screen.
- Track the plan and open problems in GitHub issues (`gh issue`), not in this file.
  Keep this file for rules and decisions; record new decisions here when made.
