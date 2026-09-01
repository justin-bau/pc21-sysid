#include "pc21_sysid/maneuvers/doublet.hpp"

#include <stdexcept>

namespace pc21_sysid {

Doublet::Doublet(Axis axis, double amplitude, double pulse_s)
    : Maneuver(axis, amplitude), pulse_s_(pulse_s) {
  if (pulse_s <= 0) {
    throw std::invalid_argument("Doublet: pulse_s must be > 0");
  }
}

double Doublet::signal(double t_rel_s) const {
  return (t_rel_s < pulse_s_) ? +1.0 : -1.0;
}

std::unique_ptr<Maneuver> Doublet::from_params(Axis axis, double amplitude,
                                               const nlohmann::json &params) {
  const double pulse_s = params.value("pulse_s", kDefaultPulseS);
  return std::make_unique<Doublet>(axis, amplitude, pulse_s);
}

} // namespace pc21_sysid
