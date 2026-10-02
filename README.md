# Mandel Drift GL

A Windows fractal animation inspired by [vgamandel](https://codeberg.org/root42/vgamandel), with OpenGL compute rendering, eight synthwave palettes, and locally generated ambient music.

[Download v0.9.0 for Windows x64](https://github.com/mmatx64/mandel-drift-opengl/releases/download/v0.9.0/MandelDriftGL-0.9.0-Windows-x64.zip) · [Latest release](https://github.com/mmatx64/mandel-drift-opengl/releases/latest)

Extract the ZIP to a writable folder and run `MandelDrift.exe`. Requires an OpenGL 4.3+ graphics driver. The release includes a SHA-256 checksum file.

## Run

Launch `MandelDrift.exe` (the built copy is in `out/`) on Windows with a GPU and driver supporting OpenGL 4.3 or newer. The renderer uses standard OpenGL compute shaders for NVIDIA, AMD, and Intel hardware. SDL3 and the C++ runtime are linked statically; no CUDA toolkit, vendor SDK, or Visual Studio is needed to run it. Hardware validation so far is on the RTX 3080; other vendors still need testing.

The app opens in a 1280×720 window. Each launch opens at a different gentle point in the journey. Endless dives visit the original four detail sites, gradually pull back, and randomly choose a different site for the next dive. The curved tour starts at a random point too. Deep holds have a 55% chance of a Julia excursion when eligible, with at least two intervening visits; reveal and return retain their 125-second ramps. Waves and kaleidoscope retain their fixed 96-second schedule and 3/5/7-fold symmetry sequence. Color waves, kaleidoscope folds, subtle bloom, and music-driven brightness add movement without distorting the controls.

Rendering adapts its internal resolution to a 14.5 ms work budget and is capped at 60 FPS with VSync. Very demanding scenes can run slower or show fine-detail shimmer at reduced resolution.

## Controls

| Key | Action |
| --- | --- |
| F | Toggle windowed / desktop fullscreen |
| P / Shift+P | Next / previous palette |
| A | Toggle automatic palette changes |
| O | Toggle shuffled automatic palette order |
| E | Switch endless dive / curved tour |
| S | Open / close settings |
| ? (Shift+/) | Open / close keyboard help |
| B | Toggle bloom |
| M | Mute / unmute; enable music if disabled |
| - / = | Lower / raise music volume by 5% |
| R / Shift+R | Pause / resume rotation; reverse direction |
| Space | Pause / resume camera movement; colors and music continue |
| F12 | Save a BMP to Pictures/MandelDrift |
| Esc | Close help, then settings; otherwise exit |

Settings also provide clickable switches, a volume slider, and a live FPS / render-time readout in the header. The readout uses the same measurements as the window title and refreshes about once per second. Music is synthesized locally; no audio files or network connection are needed. If audio is unavailable, animation continues. Toggle Music off/on to retry.

Preferences are saved to `settings.ini` beside the executable. Keep the app in a writable folder. Delete that file while the app is closed to reset preferences. Camera position and journey progress restart on launch.

## Build

Requires Windows, Visual Studio 2022 C++ Build Tools, CMake 3.28+, and Git. Initial configuration downloads the pinned SDL 3.4.16 source. No CUDA toolkit is used.

From the project root in PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 8
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix out
```

The installed app is `out/MandelDrift.exe`. No GPU architecture flags are needed. Update the graphics driver if OpenGL 4.3 context creation fails.

## Development

See [PROJECT_HANDOFF.md](PROJECT_HANDOFF.md) for the source map, validation commands, and implementation constraints. [VALIDATION.md](VALIDATION.md) records measured OpenGL/CUDA performance and hardware coverage.

The renderer uses reference-orbit perturbation for deep views, with direct double-precision restarts for exhausted references or detected glitches. This is a finite-depth desktop animation, not an arbitrary-precision infinite zoom. Windows `.scr` integration is not implemented.

The code and palettes were written for this project; the original program's source and palette data are not included. The deep-rendering approach follows [Claude Heiland-Allen's perturbation paper](https://mathr.co.uk/mandelbrot/perturbation.pdf).
