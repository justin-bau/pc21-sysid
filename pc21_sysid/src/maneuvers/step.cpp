#include "pc21_sysid/maneuvers/step.hpp"

#include <stdexcept>

namespace pc21_sysid {

Step::Step(Axis axis, double amplitude, double step_s)
    : Maneuver(axis, amplitude), step_s_(step_s) {
  if (step_s <= 0) {
    throw std::invalid_argument("Step: step_s must be > 0");
  }
}

double Step::signal(double /*t_rel_s*/) const {
  // Hold at full amplitude for the entire maneuver duration.
  // After step_s_ elapses, the base class returns nullopt from step(),
  // the runner transitions to Recovery, and pilot stick input takes over
  // (returning the surface to pilot-commanded neutral).
  return +1.0;
}

std::unique_ptr<Maneuver> Step::from_params(Axis axis, double amplitude,
                                            const nlohmann::json &params) {
  const double step_s = params.value("step_s", kDefaultStepS);
  return std::make_unique<Step>(axis, amplitude, step_s);
}

} // namespace pc21_sysid
