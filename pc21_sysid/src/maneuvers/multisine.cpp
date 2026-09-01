#include "pc21_sysid/maneuvers/multisine.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

namespace pc21_sysid {

Multisine::Multisine(Axis axis, double amplitude, double f_min_hz,
                     double f_max_hz, double duration_s)
    : Maneuver(axis, amplitude), duration_s_(duration_s) {

  if (duration_s <= 0.0) {
    throw std::invalid_argument("Multisine: duration must be > 0");
  }
  // Only harmonics of 1/T fit a whole number of cycles in the record, so the
  // component frequencies are k/T. k >= 1 excludes DC.
  const int k_lo =
      std::max(1, static_cast<int>(std::lround(f_min_hz * duration_s_)));
  const int k_hi =
      std::max(k_lo, static_cast<int>(std::lround(f_max_hz * duration_s_)));
  const int M = k_hi - k_lo + 1;

  omega_.reserve(M);
  phi_.reserve(M);

  // See Schroeder, IEEE Trans. Inf. Theory IT-16(1), 1970, pp. 85-89.
  std::complex<double> z{0.0, 0.0};
  for (int m = 1; m <= M; ++m) {
    const double p = -M_PI * static_cast<double>(m) * (m - 1) / M;
    phi_.push_back(p);
    z += std::polar(1.0, p);
  }

  // Rotate all phases by a common offset so the sum is zero at t = 0, and
  // also at t = T since every component completes whole cycles.
  const double offset = M_PI / 2.0 - std::arg(z);
  for (int m = 0; m < M; ++m) {
    phi_[m] += offset;
    omega_.push_back(2.0 * M_PI * (k_lo + m) / duration_s_);
  }
}

double Multisine::signal(double t_rel_s) const {
  double sum = 0.0;
  for (std::size_t m = 0; m < omega_.size(); ++m) {
    sum += std::cos(omega_[m] * t_rel_s + phi_[m]);
  }
  return sum / std::sqrt(static_cast<double>(omega_.size()));
}

std::unique_ptr<Maneuver> Multisine::from_params(Axis axis, double amplitude,
                                                 const nlohmann::json &params) {
  const double f_min = params.value("f_min_hz", kDefaultFMinHz);
  const double f_max = params.value("f_max_hz", kDefaultFMaxHz);
  const double dur = params.value("duration_s", kDefaultDurationS);
  return std::make_unique<Multisine>(axis, amplitude, f_min, f_max, dur);
}

} // namespace pc21_sysid
