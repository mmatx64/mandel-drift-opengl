# Mandel Drift GL

Sit back and enjoy slow fractal journeys, colorful palettes, soft glow, and locally generated ambient music. Built with C++17, SDL3, and OpenGL - no CUDA required.

The procedural soundtrack carries a recurring theme through warm synth pads, slow harmonic changes, and spacious echoes. Each launch varies the opening harmony, voicings, phrase spacing, and answering notes while keeping the theme. Layers gradually emerge with zoom depth and settle as the view pulls back; Julia transformations add a floating upper texture. All sounds are synthesized locally.

Kaleidoscope fades preserve intact 3/5/7-fold symmetry, with full-strength holds in detailed mid-dive regions. Colors and glow breathe with musical swells independently of listening volume, including with bloom off. Breathing fades away when music is muted or the volume reaches zero; camera movement stays gradual.

[Download for Windows](https://github.com/mmatx64/mandel-drift-opengl/releases/latest)

Unzip the Windows download into a writable folder and run `MandelDrift.exe`.

Tested on **Windows 11 with an NVIDIA RTX 3090**. Because it uses OpenGL, it should work with any modern NVIDIA, AMD, or Intel graphics card that supports **OpenGL 4.3+**. Performance depends on your card and resolution.

## Controls

Press **S** for settings, including live FPS and render time, or **?** for all keyboard shortcuts. **F** toggles fullscreen, **P** changes palettes, **M** mutes music, and **Space** pauses the journey. Preferences are saved beside the app.

## Build

Requires Windows, Visual Studio 2022 C++ Build Tools, CMake 3.28+, and Git. SDL3 is downloaded automatically, or included in the release's source ZIP.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 8
cmake --install build --config Release --prefix out
```

The optional audio review tool checks synthesis and callback behavior. Run `build\Release\music_review.exe --preview music-preview.wav` to render a full travel audition, or add `julia` for the longer Julia passage. Configure with `-DBUILD_TESTING=OFF` to build only the app.

## Credits & license

- Inspired by [root42's vgamandel](https://codeberg.org/root42/vgamandel).
- Deep zoom draws on [Claude Heiland-Allen's perturbation paper](https://mathr.co.uk/mandelbrot/perturbation.pdf).
- [SDL3](https://libsdl.org/) handles windows, input, and audio, under its zlib license.
- Developed with extensive implementation, debugging, and development support from OpenAI Codex.

Open source under the [GNU GPLv3 or later](LICENSE) (`GPL-3.0-or-later`). You're welcome to use, modify, and share it; distributed derivatives must preserve the same freedoms. Provided without warranty.
