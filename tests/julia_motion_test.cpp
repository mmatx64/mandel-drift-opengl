#include "julia_transition.h"
#include <cstdio>
#include <stdexcept>
int main() {
  try {
    for (int i = -1000; i < static_cast<int>((kJuliaHoldSeconds + 2) * 1000); ++i) {
      const double t = i / 1000.;
      const double a = julia_hold_amount(t);
      if (a < 0 || a > 1 || std::abs(a - julia_hold_amount(kJuliaHoldSeconds - t)) > 1e-12 ||
          std::abs(a - julia_hold_amount(t + .001)) > .00016)
        throw std::runtime_error("Julia envelope range, symmetry or continuity failed");
    }
    if (julia_hold_amount(kJuliaQuietSeconds) != 0 || julia_hold_amount(kJuliaPlateauStart) != 1 ||
        julia_hold_amount(kJuliaPlateauEnd) != 1 || julia_hold_amount(kJuliaHoldSeconds - kJuliaQuietSeconds) != 0)
      throw std::runtime_error("Julia hold timing changed");
    // Independently differentiate the visible span for both complete ramps.
    // Verify centering joins and endpoints without creating a graphics context.
    double previous_span = .00003;
    double previous_x = -.7436, previous_y = .1318;
    for (int i = 1; i <= static_cast<int>(kJuliaHoldSeconds * 1000); ++i) {
      const double amount = julia_hold_amount(i / 1000.);
      const auto morph = julia_morph(amount, .00003, -.7436, .1318);
      const double span = .00003 * morph.seed_scale;
      const double x = morph.x + morph.seed_shift_x, y = morph.y + morph.seed_shift_y;
      if (std::abs(std::log(span / previous_span)) * 1000 > kJuliaMaxLogZoomSpeed ||
          std::hypot(x - previous_x, y - previous_y) / std::sqrt(span * previous_span) * 1000 > kJuliaMaxPanSpeed)
        throw std::runtime_error("Julia visible motion exceeded its speed limits");
      previous_span = span;
      previous_x = x;
      previous_y = y;
    }
    for (double amount : {0., .5, 1.}) {
      const auto morph = julia_morph(amount, .00003, -.7436, .1318);
      if (amount < 1 && .00003 * morph.seed_scale <= .25 &&
          (morph.seed_shift_x != 0 || morph.seed_shift_y != 0))
        throw std::runtime_error("Julia centered before revealing local detail");
      if (amount == 1 && std::hypot(morph.x + morph.seed_shift_x, morph.y + morph.seed_shift_y) > 1e-12)
        throw std::runtime_error("Whole Julia silhouette was not centered");
    }
    for (double time : {kJuliaPlateauStart, kJuliaPlateauStart + 3.5, kJuliaPlateauStart + 7.,
                        kJuliaPlateauStart + 10.5, kJuliaPlateauEnd}) {
      const auto a = julia_morph(1, .00003, -.7436, .1318, time);
      const auto b = julia_morph(1, .00003, -.7436, -.1318, time);
      if (a.parameter_shift_x != b.parameter_shift_x || a.parameter_shift_y != -b.parameter_shift_y ||
          std::abs(a.seed_scale * .00003 - kJuliaWideSpan) > 1e-12)
        throw std::runtime_error("Julia drift or whole-view span failed");
      if ((time == kJuliaPlateauStart || time == kJuliaPlateauEnd) &&
          (a.parameter_shift_x != 0 || a.parameter_shift_y != 0))
        throw std::runtime_error("Julia drift did not return to its anchor");
    }
    std::puts("Julia timing and motion bounds passed.");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what()); return 1;
  }
}
