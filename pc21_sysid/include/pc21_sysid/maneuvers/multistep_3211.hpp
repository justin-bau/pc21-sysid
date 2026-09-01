#pragma once

#include <nlohmann/json.hpp>

#include "pc21_sysid/maneuvers/base.hpp"
#include <memory>

namespace pc21_sysid {

// 3-2-1-1 multistep: standard fixed-wing system identification input.
// +A for 3*dt, -A for 2*dt, +A for dt, -A for dt. Total = 7 * dt.
class Multistep3211 final : public Maneuver {
public:
  Multistep3211(Axis axis, double amplitude, double dt_s);

  double duration() const override { return 7.0 * dt_s_; }

  static std::unique_ptr<Maneuver> from_params(Axis axis, double amplitude,
                                               const nlohmann::json &params);

protected:
  double signal(double t_rel_s) const override;

private:
  static constexpr double kDefaultDtS = 0.5;
  double dt_s_;
};

} // namespace pc21_sysid
