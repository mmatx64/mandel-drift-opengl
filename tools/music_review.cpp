// SPDX-License-Identifier: GPL-3.0-or-later
#include "ambient_music.h"
#include "julia_transition.h"
#include "music_travel.h"
#include "music_reactivity.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int rate = AmbientSynth::sample_rate;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
struct Metrics {
  double peak = 0, adjacent = 0, energy = 0, side = 0;
  std::array<float, 2> previous{};
  std::uint64_t frames = 0;
  void add(const float* samples, int count) {
    for (int i = 0; i < count; ++i) {
      for (int channel = 0; channel < 2; ++channel) {
        const float value = samples[i * 2 + channel];
        require(std::isfinite(value), "Non-finite audio sample");
        peak = std::max(peak, std::abs(static_cast<double>(value)));
        adjacent = std::max(adjacent, std::abs(static_cast<double>(value - previous[channel])));
        previous[channel] = value;
        energy += value * value;
      }
      const double difference = samples[i * 2] - samples[i * 2 + 1];
      side += difference * difference;
    }
    frames += count;
  }
  void report(const char* label) const {
    std::printf("%s: %.2fs, peak %.6f, adjacent %.6f, RMS %.6f, stereo difference %.6f\n",
        label, frames / static_cast<double>(rate), peak, adjacent,
        std::sqrt(energy / (frames * 2)), std::sqrt(side / frames));
  }
  void check() const {
    require(peak < 0.85, "Insufficient audio headroom");
    require(adjacent < 0.025, "Abrupt audio transition");
    require(energy / (frames * 2) > 0.0001, "Music is unexpectedly quiet");
    require(side / frames > 0.00001, "Stereo image collapsed");
  }
};

std::array<float, 2> travel(double seconds, bool julia) {
  constexpr double minimum = .00003;
  const double dive = 8.2 * std::log(3.2 / minimum);
  const double hold = julia ? kJuliaHoldSeconds : 10;
  double depth = 1;
  float amount = 0;
  double scale = 1;
  const auto ease = [](double t) { return t * t * (3 - 2 * t); };
  if (seconds < dive) depth = ease(seconds / dive);
  else if (seconds >= dive + hold) depth = 1 - ease(std::min((seconds - dive - hold) / dive, 1.0));
  else if (julia) {
    amount = static_cast<float>(julia_hold_amount(seconds - dive));
    scale = std::exp(std::log(kJuliaWideSpan / minimum) * amount);
  }
  const double span = std::exp(std::log(3.2) + std::log(minimum / 3.2) * depth);
  return {{music_travel_depth(span, scale, minimum), amount}};
}

void write_integer(std::ofstream& file, std::uint32_t value, int bytes) {
  for (int i = 0; i < bytes; ++i) file.put(static_cast<char>((value >> (i * 8)) & 255));
}

void preview(const std::string& path, bool julia) {
  // Actual camera pacing, including Julia's full 125-second reveal/return ramps.
  const double dive = 8.2 * std::log(3.2 / .00003);
  const int seconds = julia ? static_cast<int>(std::ceil(dive * 2 + kJuliaHoldSeconds + 32)) : 240;
  const int total = seconds * rate;
  std::ofstream file(path, std::ios::binary);
  require(static_cast<bool>(file), "Cannot open preview file");
  file.write("RIFF", 4); write_integer(file, 36 + total * 4, 4);
  file.write("WAVEfmt ", 8); write_integer(file, 16, 4);
  write_integer(file, 1, 2); write_integer(file, 2, 2);
  write_integer(file, rate, 4); write_integer(file, rate * 4, 4);
  write_integer(file, 4, 2); write_integer(file, 16, 2);
  file.write("data", 4); write_integer(file, total * 4, 4);
  AmbientSynth synth;
  Metrics metrics;
  std::array<float, 960> samples{};
  std::array<std::int16_t, 960> pcm{};
  for (int offset = 0; offset < total;) {
    const int count = std::min(480, total - offset);
    const double t = offset / static_cast<double>(rate);
    const auto targets = travel(t, julia);
    const float gain = static_cast<float>(std::clamp((seconds - t) / 6, 0.0, 1.0));
    synth.render(samples.data(), count, gain, targets[0], targets[1]);
    metrics.add(samples.data(), count);
    for (int i = 0; i < count * 2; ++i)
      pcm[i] = static_cast<std::int16_t>(std::lround(std::clamp(samples[i], -1.0f, 1.0f) * 32767));
    file.write(reinterpret_cast<const char*>(pcm.data()), count * 4);
    offset += count;
  }
  require(static_cast<bool>(file), "Cannot write preview file");
  metrics.report(julia ? "Julia audition" : "Travel audition");
  metrics.check();
  const double hold_end = dive + (julia ? kJuliaHoldSeconds : 10);
  std::printf("Wrote %s (descent 0-%.1fs; %s %.1f-%.1fs; pullback %.1f-%.1fs)\n",
      path.c_str(), dive, julia ? "Julia" : "hold", dive, hold_end, hold_end, hold_end + dive);
}

void device_smoke() {
  AmbientMusic music;
  music.set_controls(true, false, 35);
  music.set_travel(.7f, .3f);
  require(music.start(), "Default audio device unavailable");
  const Uint64 deadline = SDL_GetTicks() + 5000;
  while (music.rendered_frames() < rate * 2 && SDL_GetTicks() < deadline) SDL_Delay(10);
  require(music.available() && music.rendered_frames() >= rate * 2 && music.envelope() > 0,
          "Default device callback failed");
  const char* name = music.device_name();
  std::printf("Default device: %s; rendered %llu frames; RMS %.6f\n",
      name ? name : "(unknown)", music.rendered_frames(), music.envelope());
  music.set_controls(true, true, 35);
  SDL_Delay(700);
  require(music.envelope() == 0, "Default device mute failed");
}

void tests() {
  for (const double minimum : {.00003, .012}) {
    require(music_travel_depth(3.2, 1, minimum) == 0, "Wide view depth");
    require(std::abs(music_travel_depth(minimum, 1, minimum) - 1) < 1e-6, "Destination depth");
    require(std::abs(music_travel_depth(std::sqrt(3.2 * minimum), 1, minimum) - .5) < 1e-6,
            "Logarithmic half-depth");
    require(music_travel_depth(minimum, kJuliaWideSpan / minimum, minimum) == 0,
            "Julia visible pullback");
  }
  require(music_travel_depth(0, 1, .00003) == 0, "Invalid span");
  require(music_travel_depth(1, 1, 3.2) == 0, "Invalid destination");
  require(music_travel_depth(std::numeric_limits<double>::quiet_NaN(), 1, .012) == 0,
          "Non-finite span");

  // Callback partitioning must not change the phrase clock or control-rate DSP.
  AmbientSynth whole, split;
  const int count = rate * 2 + 137;
  std::vector<float> reference(count * 2), partitioned(count * 2);
  whole.render(reference.data(), count, .7f, .8f, .4f);
  constexpr int sizes[] = {1, 17, 511, 1309, 64, 997};
  int offset = 0, index = 0;
  while (offset < count) {
    const int frames = std::min(sizes[index++ % 6], count - offset);
    split.render(partitioned.data() + offset * 2, frames, .7f, .8f, .4f);
    offset += frames;
  }
  require(reference == partitioned, "Callback partition changes audio");
  AmbientSynth silent, varied(3081);
  silent.render(partitioned.data(), count, 0, .8f, .4f);
  require(silent.musical_level() == whole.musical_level(), "Musical energy depends on listening volume");
  require(std::all_of(partitioned.begin(), partitioned.end(), [](float value) { return value == 0; }),
          "Zero volume is not silent");
  varied.render(partitioned.data(), count, .7f, .8f, .4f);
  double variation = 0;
  for (size_t i = 0; i < reference.size(); ++i) variation += std::abs(reference[i] - partitioned[i]);
  require(variation / reference.size() > .001, "Different seeds did not vary the launch music");

  AmbientSynth running, muted;
  std::array<float, 2048> a{}, b{};
  for (int step = 0; step < 32 * rate / 1024; ++step) {
    const float gain = step > 100 && step < 1000 ? 0.0f : 1.0f;
    running.render(a.data(), 1024, 1, .8f, .2f);
    muted.render(b.data(), 1024, gain, .8f, .2f);
    if (step == 999)
      require(std::all_of(b.begin(), b.end(), [](float v) { return v == 0; }), "Mute not silent");
  }
  for (int i = 0; i < 2048; ++i)
    require(std::abs(a[i] - b[i]) < .00003f, "Mute interrupts phrase/reverb timeline");
  require(running.musical_level() == muted.musical_level(), "Mute changes musical energy tracking");

  AudioBreath breath;
  float steady = 0, swell = 0, fade = 0;
  for (int i = 0; i < 600; ++i) steady = breath.advance(.08f, 1.0 / 60, true);
  for (int i = 0; i < 60; ++i) swell = breath.advance(.12f, 1.0 / 60, true);
  require(swell > steady + .35f, "Breathing did not follow a musical swell");
  for (int i = 0; i < 600; ++i) fade = breath.advance(.12f, 1.0 / 60, false);
  require(fade == 0, "Muted breathing did not fade away");
  AudioBreath silence;
  require(silence.advance(0, .1, true) == 0, "Silence lights the scene");

  AmbientSynth synth;
  Metrics metrics;
  const auto start = std::chrono::steady_clock::now();
  AudioBreath musical_breath;
  float breath_minimum = 1, breath_maximum = 0;
  for (int step = 0; step < 288 * rate / 1024; ++step) {
    const double t = step * 1024.0 / rate;
    const auto targets = travel(t, false);
    // Include sudden controls, long holds, return, and a full harmony wrap.
    synth.render(a.data(), 1024, 1.0f, t > 240 ? 1.0f : targets[0], t > 260 ? 1.0f : 0.0f);
    metrics.add(a.data(), 1024);
    const float intensity = musical_breath.advance(synth.musical_level(), 1024.0 / rate, true);
    if (t > 10) {
      breath_minimum = std::min(breath_minimum, intensity);
      breath_maximum = std::max(breath_maximum, intensity);
    }
  }
  metrics.report("Composition/transition check");
  metrics.check();
  require(breath_maximum - breath_minimum > .25f, "Synthesized music did not produce visible breathing");
  std::printf("Musical breathing range %.4f..%.4f\n", breath_minimum, breath_maximum);
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("Synthesis time %.2fs for 288s of audio (%.1fx realtime)\n", elapsed, 288 / elapsed);
  synth.render(a.data(), 1024, std::numeric_limits<float>::quiet_NaN(),
               std::numeric_limits<float>::infinity(), -1);
  for (const float value : a) require(std::isfinite(value), "Invalid controls poison DSP");

  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  {
    AmbientMusic music;
    music.set_controls(true, false, 70);
    music.set_travel(1, .5f);
    require(music.start(), "Dummy audio device unavailable");
    const Uint64 deadline = SDL_GetTicks() + 3000;
    while (music.rendered_frames() < rate && SDL_GetTicks() < deadline) SDL_Delay(10);
    require(music.available() && music.rendered_frames() >= rate && music.envelope() > 0,
            "Live SDL callback failed");
    music.set_controls(true, true, 70);
    SDL_Delay(700);
    require(music.envelope() == 0, "Callback mute failed");
    music.set_controls(true, false, 70);
    SDL_Delay(300);
    require(music.envelope() > 0, "Callback unmute failed");
  }
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "unavailable-test-driver");
  {
    AmbientMusic music;
    require(!music.start() && !music.available(), "Unavailable device not handled");
  }
  SDL_Quit();
  std::puts("PASS: seeded variation, musical breathing, depth, Julia, callback partitions, mute continuity, headroom, stereo, SDL callback");
}
}

int main(int argc, char** argv) {
  try {
    if (argc >= 3 && std::string(argv[1]) == "--preview")
      preview(argv[2], argc >= 4 && std::string(argv[3]) == "julia");
    else if (argc >= 2 && std::string(argv[1]) == "--device-smoke")
      device_smoke();
    else tests();
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Audio review failed: %s\n", error.what());
    return 1;
  }
}
