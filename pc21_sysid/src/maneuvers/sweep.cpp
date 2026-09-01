#include "pc21_sysid/maneuvers/sweep.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace pc21_sysid {

Sweep::Sweep(Axis axis, double amplitude, double f_min_hz, double f_max_hz,
             double duration_s)
    : Maneuver(axis, amplitude) {
  std::tie(f_min_hz_, f_max_hz_) = std::minmax(f_min_hz, f_max_hz);

  if (f_min_hz_ <= 0.0) {
    throw std::invalid_argument("Sweep: f_min must be > 0");
  }
  if (duration_s <= 0.0) {
    throw std::invalid_argument("Sweep: duration must be > 0");
  }

  const double w = f_min_hz_ + (f_max_hz_ - f_min_hz_) * (1. / kC1 - kC2);
  duration_s_ = std::max(1.0, std::floor(w * duration_s)) / w;
}

double Sweep::signal(double t_rel_s) const {
  const double phase =
      2.0 * M_PI * f_min_hz_ * t_rel_s +
      kC2 * 2.0 * M_PI * (f_max_hz_ - f_min_hz_) *
          (duration_s_ / kC1 * (std::exp(kC1 * t_rel_s / duration_s_) - 1.0) -
           t_rel_s);

  return std::sin(phase);
}

std::unique_ptr<Maneuver> Sweep::from_params(Axis axis, double amplitude,
                                             const nlohmann::json &params) {
  const double f_min = params.value("f_min_hz", kDefaultFMinHz);
  const double f_max = params.value("f_max_hz", kDefaultFMaxHz);
  const double dur = params.value("duration_s", kDefaultDurationS);
  return std::make_unique<Sweep>(axis, amplitude, f_min, f_max, dur);
}

} // namespace pc21_sysid
