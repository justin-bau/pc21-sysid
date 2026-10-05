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
  // Only harmonics of 1/T fit a whole number of cycles, so the
  // component frequencies are k/T
  const int k_lo =
      std::max(2, static_cast<int>(std::floor(f_min_hz * duration_s_ + 1e-9)));
  const int k_hi = std::max(
      k_lo, static_cast<int>(std::ceil(f_max_hz * duration_s_ - 1e-9)));
  const int M = k_hi - k_lo + 1;

  omega_.reserve(M);
  phi_.reserve(M);

  // See Schroeder1970
  std::complex<double> z{0.0, 0.0};
  for (int k = k_lo; k <= k_hi; ++k) {
    const double n = k - k_lo + 1.0;
    const double p = -M_PI * n * n / M;
    phi_.push_back(p);
    omega_.push_back(2.0 * M_PI * k / duration_s_);
    z += std::polar(1.0, p);
  }

  // Rotate all phases by a common offset so the sum is zero at t = 0, and
  // also at t = T since every component completes whole cycles.
  const double offset = M_PI / 2.0 - std::arg(z);
  for (double &p : phi_)
    p += offset;

  // Peak may go above set amplitude (peak factor)
  // One solution would be precompute the entire maneuver at construction and
  // scale by the max peak, so max peak is the set amplitude from parameters.
  // We are instead relying on iterative setting of the amplitude to acceptable
  // levels here.
}

// During maneuvering, each call needs M cos calculations. Should not be a
// problem here (about M=50-100). Careful for other applications (longer
// maneuvers, wide bands, ...) at high control rate
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
