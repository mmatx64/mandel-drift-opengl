# Validation and performance

Measured on 2026-10-02: Windows, NVIDIA RTX 3080, driver 616.64, OpenGL 4.3 core, MSVC 19.44, Release, SDL 3.4.16. These measurements do not predict AMD, Intel, integrated, or older GPU performance. An OpenGL 4.3+ driver is required; OpenGL support alone at a lower version is insufficient.

## Compute comparison

Both backends used 1921×1081, matching camera coordinates, span, rotation, and iteration budgets. Values are medians of 31 warmed GPU timing samples. CUDA values are the current optimized renderer, not its older frozen baseline. OpenGL includes its image barriers and queue dispatch overhead. These are compute timings, not application FPS.

| Scene | CUDA ms | OpenGL ms | Difference |
| --- | ---: | ---: | ---: |
| Wide | 0.0645 | 0.070 | +9% |
| Deep entry, span .004 | 0.8179 | 0.914 | +12% |
| Deep middle, span .0006 | 1.7080 | 1.804 | +6% |
| Deep hold, span .00003 | 2.0070 | 2.319 | +16% |
| Mirrored deep hold | 1.9916 | 2.324 | +17% |

The OpenGL port is close to the optimized CUDA compute performance on this GPU, with all listed scenes below the 14.5 ms work budget before post-processing. It preserves adaptive resolution and the original 60 FPS ceiling. Equal performance on every graphics card is not established.

## Numerical and visual checks

Six CTest suites cover GPU numerics, Julia motion bounds, settings, frame budget, visual effect envelopes, and procedural audio. GPU tests strictly check stable exterior/interior scenes against an independent scalar-double oracle, require finite values and non-flat detail, and compare the queued/long-reference output exactly against the checked shader path. Empty/short references, wide/narrow Julia, drift, mirrored views, odd edge blocks, retained capacity strides, and resize/reallocation are exercised.

Chaotic boundaries are diagnostic: float iteration and perturbation can diverge from scalar double, including escape classification. Reported boundary outliers are not certified-correct pixels. The CUDA renderer has the same numerical limitation; neither backend provides arbitrary-precision infinite zoom.

The installed-app smoke captures were inspected for deep detail, Julia reveal/hold/return, 3/5/7-fold symmetry, waves, bloom, help, and settings. Fullscreen smoke uses 2560×1440 output and exercises 1920×1080 internal rendering at deep stages. Test capture I/O and deliberately accelerated scene changes affect smoke timing; its average rate is not a steady-state gameplay measurement.

The latest installed-app smoke exited 0. The complete real-time interactive cycle finished with 27,460 measured frame intervals: 59.62 FPS average, 16.67 ms median, 16.70 ms p95, 17.32 ms p99, and 0.50% above 20 ms. Output was 2560×1440 fullscreen initially and 1280×720 windowed later in the run; these results include capture I/O and interaction rather than representing a fixed-resolution benchmark.

Normal-launch audio was separately checked through the real WASAPI playback device, `Headphones (WH-1000XM5)`: the callback supplied 48,480 frames, published a nonzero envelope of .004050 after startup, and ran unmuted at 35% volume. This verifies delivery to the selected Windows playback stream; physical audibility is user-verifiable. Automated smoke/cycle/motion modes intentionally do not start audio.

The shortcut menu retains its compact three-column layout, with aligned footer groups and a more opaque dark background. Its updated windowed capture was inspected after installation.

The settings header displays `SETTINGS | FPS | render ms`, using the same measurements as the window title, refreshed about once per second. Only a small header texture patch is updated. The final header layout passed installed-app smoke validation and visual inspection.

Raw logs, regenerated BMPs, and a visual contact sheet are under `verification/`, which is ignored by version control. Do not substitute historical output for a new GPU/backend test.

## Remaining coverage

- AMD and Intel hardware and drivers require real runtime, visual, and performance validation.
- Sustained performance on low-power or integrated cards is unknown; adaptive resolution may reach its .32 minimum and still miss the frame budget.
- Driver floating-point decisions can change fine fractal boundary pixels. Multi-vendor pixel identity is not promised.
- Windows `.scr` integration remains unimplemented, as in the original.
