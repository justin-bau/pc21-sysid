#pragma once

#include "pc21_sysid/maneuvers/base.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace pc21_sysid {

class Step final : public Maneuver {
public:
  Step(Axis axis, double amplitude, double step_s);

  double duration() const override { return step_s_; }

  static std::unique_ptr<Maneuver> from_params(Axis axis, double amplitude,
                                               const nlohmann::json &params);

protected:
  double signal(double t_rel_s) const override;

private:
  static constexpr double kDefaultStepS = 0.5;
  double step_s_;
};

} // namespace pc21_sysid
