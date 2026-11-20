#pragma once

#include "ui/view.hpp"

#include <vector>

namespace mads_dash::ui {

/// Single-sided amplitude spectrum: frequencies in Hz and amplitudes.
struct Spectrum {
  std::vector<double> f, amp;
};

/// Spectrum of samples (t, v), t increasing. Resamples to a uniform grid at
/// the mean sample rate, removes the mean, applies a Hann window, zero-pads to
/// a power of two and runs a radix-2 FFT. Empty for fewer than 4 samples.
Spectrum spectrum(const std::vector<double> &t, const std::vector<double> &v);

/// Amplitude spectrum of each Number source over [now - window, now].
class SpectrumView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Spectrum"; }
  bool accepts(data::Kind k) const override { return k == data::Kind::Number; }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;

private:
  bool _log_y = false;
};

} // namespace mads_dash::ui
