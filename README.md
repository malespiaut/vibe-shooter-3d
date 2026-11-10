# IRONHOLD (vibe-shooter-3d)

A first-person shooter built from scratch with early-90s techniques: 320x240, 256 colors,
a software raycaster, hand-mixed audio, and fixed-point game logic. Every texture, sprite,
sound, song, level and font is made by code or by hand in this repo. See CLAUDE.md for the rules.

**Episode 1: The Breach** — eight levels from the outpost gate to the Warden's keep.

## Build and play

Needs a C11 compiler, CMake and SDL3.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/shooter            # --warp N starts at level N
ctest --test-dir build     # determinism, level validation, soak test
```

## Controls (rebindable in Options > Controls)

| Action | Keys |
|---|---|
| Move / strafe | W A S D, arrow keys |
| Turn | mouse, Left/Right |
| Run | Shift |
| Fire | Mouse 1, Ctrl |
| Use (doors, switches, secrets) | Space, E |
| Weapons | 1 knife, 2 pistol, 3 shotgun, 4 chaingun, 5 rocket launcher |
| Automap | Tab |
| Menu / pause | Esc |

Secret walls look like ordinary walls: press Use on them.

## Enemies

- **Grunt** — rifleman. Raises his rifle before firing; keep moving and he misses more.
- **Hound** — fast and low. Rears up with its jaws open just before it bites.
- **Spitter** — hooded fire-thrower. Its hand glows before it throws; the fireball is slow, so sidestep it.
- **Brute** — armored, slow, tough. Long windup, then a three-round burst. Find cover.
- **The Warden** — you'll know.
