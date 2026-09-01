#include "pc21_sysid/maneuvers/multistep_3211.hpp"
#include <stdexcept>

namespace pc21_sysid {

Multistep3211::Multistep3211(Axis axis, double amplitude, double dt_s)
    : Maneuver(axis, amplitude), dt_s_(dt_s) {
  if (dt_s <= 0) {
    throw std::invalid_argument("3211: dt_s must be > 0");
  }
}

double Multistep3211::signal(double t_rel_s) const {
  const double t = t_rel_s / dt_s_;
  if (t < 3.0)
    return +1.0; // 3*dt
  if (t < 5.0)
    return -1.0; // 3+2 = 5*dt
  if (t < 6.0)
    return +1.0; // 5+1 = 6*dt
  return -1.0;   // 6+1 = 7*dt
}

std::unique_ptr<Maneuver>
Multistep3211::from_params(Axis axis, double amplitude,
                           const nlohmann::json &params) {
  const double dt_s = params.value("dt_s", kDefaultDtS);
  return std::make_unique<Multistep3211>(axis, amplitude, dt_s);
}

} // namespace pc21_sysid
