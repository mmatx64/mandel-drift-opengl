# Mandel Drift GL handoff

Windows C++17 / SDL3 / OpenGL 4.3+ port, version 0.9.0. Source repository: [mmatx64/mandel-drift-opengl](https://github.com/mmatx64/mandel-drift-opengl). Derived from [mandel-drift](https://github.com/mmatx64/mandel-drift) at commit `71c70f10bfc2090f75ecb766561a3ba843a8cb3a`. This folder builds and runs independently; no CUDA compiler, runtime, or NVIDIA SDK is linked.

Start with [README.md](README.md) for behavior, controls, and build commands. OpenGL 3.3 and macOS are outside this port's scope. Standard OpenGL APIs are used across GPU vendors; hardware validation has only been performed on an RTX 3080 so far.

## Build and validate

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 8
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix out

$smoke = Start-Process .\out\MandelDrift.exe -ArgumentList '--smoke-test' `
  -PassThru -Wait -WindowStyle Hidden -RedirectStandardError smoke-error.txt
$smoke.ExitCode
build\Release\fractal_gl_test.exe --benchmark
```

The GPU numerical test creates a hidden real OpenGL context. Missing OpenGL support fails validation rather than skipping it. Benchmark mode validates small odd-sized images against the CPU oracle, then measures 31 GPU query samples at 1921×1081 after warmup. Timings include dispatch barriers and queued restart work, but exclude presentation.

`--cycle-test` runs a complete fullscreen dive/Julia/pullback cycle in real time, roughly 460 seconds. `--motion-test` and `--motion-test return` retain the original 120-second fullscreen excerpts and camera traces. Test modes remain silent, deterministic, and do not load or save personal settings. Run each in its own working directory to preserve captures.

## Source map

| File | Responsibility |
| --- | --- |
| `src/fractal_gl.cpp`, `.h` | Context-owned compute programs, escape texture, orbit uploads, fallback queue, reduction/readbacks |
| `src/fractal_shaders.h` | Wide Mandelbrot/Julia, checked/long-reference perturbation, double restarts, fold reduction |
| `src/main.cpp` | Window, input, journeys, palette/temporal rendering, bloom, four-slot asynchronous GPU timings, test modes |
| `src/julia_transition.h`, `src/trip_effects.h` | Unchanged slow Julia ramps and visual choreography |
| `src/frame_budget.h` | 14.5 ms adaptive work budget, 60 FPS cap implemented in main |
| `src/app_settings.*`, `src/settings_overlay.*` | Preferences and settings/help UI |
| `src/ambient_synth.*`, `src/ambient_music.*` | Unchanged procedural audio and SDL playback |
| `tests/fractal_gl_test.cpp` | Independent scalar-double oracle, finite/detail checks, exact queued/checked equivalence, odd dimensions, resize, benchmarks |
| `tests/julia_motion_test.cpp` | Original CPU envelope and visible speed bounds |
| Other `tests/` | Original settings, audio, trip effects, and frame-budget suites |

## Constraints

- Escape output is an R32F image; color before interpolation. Overlays stay outside temporal history and bloom. Keep the original eight palettes, four sites, and journey timing.
- Below span `.0045`, use a cached CPU-double-generated reference and GPU-float perturbation. Wide Julia uses float iteration. Exhausted references and detected glitches restart in GPU double precision. Julia seeds cannot use Mandelbrot cardioid/bulb shortcuts.
- The texture retains output-sized storage across adaptive quality changes. Only the active rectangle is written/sampled. Readbacks must use capacity row stride, or `FractalGL::read`, which returns packed active pixels.
- Queued long-reference fallbacks use standard SSBO atomic counters. Capacity is one index per output pixel. A fixed dispatch consumes the count without CPU readback. Checked/short references restart inline.
- Respect OpenGL memory barriers between image writes, texture fetches, SSBO passes, and API updates/readbacks. Telemetry queries are polled without per-frame waits; ignore stale dimensions. Infrequent fold summaries and test/capture readbacks are deliberately synchronous.
- Preserve slow Julia reveal/return, parameter cache keys, history rejection during morphs, and randomized production openings. Changes to resolution must not reduce iteration limits or numerical precision.
- Shader source is embedded in the executable. Installed runtime consists of the executable, README, handoff/validation notes, and SDL license. Settings resolve beside the executable. Release ZIPs must exclude personal `settings.ini`, build caches, and verification captures.

See [VALIDATION.md](VALIDATION.md) for measured results and remaining hardware limits.
