#pragma once

#include <memory>
#include <nlohmann/json.hpp>

#include "pc21_sysid/maneuvers/base.hpp"

namespace pc21_sysid {

// Doublet: +A for pulse_s, then -A for pulse_s. Total duration = 2 * pulse_s.
class Doublet final : public Maneuver {
public:
  Doublet(Axis axis, double amplitude, double pulse_s);

  double duration() const override { return 2.0 * pulse_s_; }

  static std::unique_ptr<Maneuver> from_params(Axis axis, double amplitude,
                                               const nlohmann::json &params);

protected:
  double signal(double t_rel_s) const override;

private:
  static constexpr double kDefaultPulseS = 0.5;
  double pulse_s_;
};

} // namespace pc21_sysid
