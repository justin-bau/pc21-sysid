#pragma once

#include <cstdint>

namespace pc21_sysid {

// Control loop and heartbeat rates
inline constexpr double kControlLoopHz = 200.0;
inline constexpr double kHeartbeatHz = 10.0;

// PX4 navigation states (from VehicleStatus.msg in PX4 v1.17)
// Verify px4_msgs/msg/VehicleStatus.hpp if upgrading PX4
inline constexpr uint8_t kNavStateOffboard = 14;

// Clamping bounds for normalized torque and stick inputs
inline constexpr double kClampLower = -1.0;
inline constexpr double kClampUpper = 1.0;

// Throttle bounds (positive thrust only for forward-flight aircraft)
inline constexpr double kThrottleLower = 0.0;
inline constexpr double kThrottleUpper = 1.0;

// Default durations for state holds
inline constexpr double kRecoveryDefaultS = 3.0;
inline constexpr double kAbortedHoldS = 1.0;

// QoS depth matching PX4's publishers.
inline constexpr int kPx4QosDepth = 5;

} // namespace pc21_sysid
