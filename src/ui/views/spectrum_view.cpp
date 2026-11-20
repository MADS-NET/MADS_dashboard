#include "ui/views/spectrum_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace mads_dash::ui {

namespace {
using cplx = std::complex<double>;

/// In-place iterative radix-2 FFT; a.size() must be a power of two.
void fft(std::vector<cplx> &a) {
  const std::size_t n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1) {
    const cplx wl = std::polar(1.0, -2 * std::numbers::pi / double(len));
    for (std::size_t i = 0; i < n; i += len) {
      cplx w = 1;
      for (std::size_t k = 0; k < len / 2; ++k, w *= wl) {
        const cplx u = a[i + k], x = a[i + k + len / 2] * w;
        a[i + k] = u + x;
        a[i + k + len / 2] = u - x;
      }
    }
  }
}
} // namespace

Spectrum spectrum(const std::vector<double> &t, const std::vector<double> &v) {
  const std::size_t n = std::min(t.size(), v.size());
  if (n < 4 || !(t[n - 1] > t[0]))
    return {};
  const double dt = (t[n - 1] - t[0]) / double(n - 1);

  // uniform resampling by linear interpolation, then mean removal
  std::vector<double> u(n);
  std::size_t j = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const double ti = t[0] + double(i) * dt;
    while (j + 2 < n && t[j + 1] < ti)
      ++j;
    const double span = t[j + 1] - t[j];
    const double a = span > 0 ? std::clamp((ti - t[j]) / span, 0.0, 1.0) : 0;
    u[i] = v[j] + a * (v[j + 1] - v[j]);
  }
  double mean = 0;
  for (double x : u)
    mean += x;
  mean /= double(n);

  // Hann window, zero-padding
  std::size_t m = 1;
  while (m < n)
    m <<= 1;
  std::vector<cplx> a(m, 0.0);
  double wsum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const double w =
        0.5 - 0.5 * std::cos(2 * std::numbers::pi * double(i) / double(n - 1));
    a[i] = (u[i] - mean) * w;
    wsum += w;
  }
  fft(a);

  Spectrum s;
  for (std::size_t k = 0; k <= m / 2; ++k) {
    s.f.push_back(double(k) / (double(m) * dt));
    s.amp.push_back(2 * std::abs(a[k]) / wsum);
  }
  return s;
}

void SpectrumView::draw_options() { ImGui::Checkbox("log Y", &_log_y); }

void SpectrumView::draw(const data::Store &store, const Timeline &tl) {
  if (!ImPlot::BeginPlot("##spectrum", ImVec2(-1, -1)))
    return;
  const ImPlotAxisFlags fit = tl.paused ? 0 : ImPlotAxisFlags_AutoFit;
  ImPlot::SetupAxes("Hz", "amplitude", fit, fit);
  ImPlot::SetupAxisScale(ImAxis_Y1,
                         _log_y ? ImPlotScale_Log10 : ImPlotScale_Linear);
  const double t0 = tl.now - tl.window;
  for (const auto &key : _sources) {
    auto it = store.series().find(key);
    if (it == store.series().end() || it->second.kind != data::Kind::Number)
      continue;
    const auto &s = it->second;
    const auto b = std::lower_bound(s.t.begin(), s.t.end(), t0) - s.t.begin();
    const auto e =
        std::upper_bound(s.t.begin(), s.t.end(), tl.now) - s.t.begin();
    if (e <= b)
      continue;
    // shortcut: recomputed every frame; fine for the default 10 s window,
    // cache per source (recompute on new data) if long windows at kHz rates lag
    const Spectrum sp = spectrum({s.t.begin() + b, s.t.begin() + e},
                                 {s.v.begin() + b, s.v.begin() + e});
    if (!sp.f.empty())
      ImPlot::PlotLine(key.c_str(), sp.f.data(), sp.amp.data(),
                       (int)sp.f.size());
  }
  ImPlot::EndPlot();
}

nlohmann::json SpectrumView::options() const { return {{"log_y", _log_y}}; }

void SpectrumView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("log_y"); it != j.end() && it->is_boolean())
    _log_y = it->get<bool>();
}

} // namespace mads_dash::ui
