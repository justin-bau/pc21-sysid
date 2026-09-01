#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include "pc21_sysid/maneuvers/base.hpp"

namespace pc21_sysid {

class Multisine final : public Maneuver {
public:
  Multisine(Axis axis, double amplitude, double f_min_hz, double f_max_hz,
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

  double duration_s_;
  std::vector<double> omega_;
  std::vector<double> phi_;
};

} // namespace pc21_sysid
