#pragma once

#include <nlohmann/json.hpp>

#include "pc21_sysid/maneuvers/base.hpp"
#include <memory>

namespace pc21_sysid {

class Sweep final : public Maneuver {
public:
  Sweep(Axis axis, double amplitude, double f_min_hz, double f_max_hz,
        double duration_s);

  double duration() const override { return duration_s_; }

  static std::unique_ptr<Maneuver> from_params(Axis axis, double amplitude,
                                               const nlohmann::json &params);

protected:
  double signal(double t_rel_s) const override;

private:
  static constexpr double kDefaultFMinHz = 0.3;
  static constexpr double kDefaultFMaxHz = 2.5;
  static constexpr double kDefaultDurationS = 10.0;

  static constexpr double kC1 = 4.0;
  static constexpr double kC2 = 0.018657360363774; // 1/(exp(kC1) - 1)

  double f_min_hz_;
  double f_max_hz_;
  double duration_s_;
};

} // namespace pc21_sysid
