#pragma once

#include <optional>
#include <string>

namespace pc21_sysid {

enum class Axis {
  Roll,
  Pitch,
  Yaw,
};

// Parses 'roll' / 'pitch' / 'yaw' (case-insensitive), throws on unknown
Axis parse_axis(std::string s);

// Per-axis torque delta
// Caller clamps the sum of pilot + delta
struct ManeuverDelta {
  double roll = 0.0;
  double pitch = 0.0;
  double yaw = 0.0;
};

// Abstract base class for all maneuvers
// Each subclass defines the signal shape via signal(t_rel) returning a value
// in [-1, +1], and its total duration via duration()
// The framework manages lifecycle (start, step until nullopt, abort) externally
class Maneuver {
public:
  Maneuver(Axis axis, double amplitude) : axis_(axis), amplitude_(amplitude) {}

  virtual ~Maneuver() = default;

  void start(double t_now_s) { t_start_s_ = t_now_s; }

  // Returns delta for current time, or nullopt if maneuver complete.
  std::optional<ManeuverDelta> step(double t_now_s) const {
    if (!t_start_s_.has_value()) {
      return std::nullopt;
    }
    const double t_rel = t_now_s - t_start_s_.value();
    if (t_rel >= duration()) {
      return std::nullopt;
    }
    return apply_to_axis(amplitude_ * signal(t_rel));
  }

  // Total maneuver duration in seconds
  virtual double duration() const = 0;

protected:
  // Maneuver signal in normalized unit at relative time t_rel >= 0.
  // It can overshoot (like with multisine) and go above 1 or -1
  // control_step is expected to catch any overshoot signal * amplitude above
  // max setpoint
  virtual double signal(double t_rel_s) const = 0;

private:
  ManeuverDelta apply_to_axis(double value) const {
    ManeuverDelta d;
    switch (axis_) {
    case Axis::Roll:
      d.roll = value;
      break;
    case Axis::Pitch:
      d.pitch = value;
      break;
    case Axis::Yaw:
      d.yaw = value;
      break;
    }
    return d;
  }

  Axis axis_;
  double amplitude_;
  std::optional<double> t_start_s_;
};

} // namespace pc21_sysid
