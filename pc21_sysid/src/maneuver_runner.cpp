#include "pc21_sysid/maneuver_runner.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "pc21_sysid/constants.hpp"
#include "pc21_sysid/maneuvers/registry.hpp"

namespace pc21_sysid {

const char *state_to_string(State s) {
  switch (s) {
  case State::Idle:
    return "idle";
  case State::Arming:
    return "arming";
  case State::Executing:
    return "executing";
  case State::Recovery:
    return "recovery";
  case State::Aborted:
    return "aborted";
  }
  return "unknown";
}

namespace {

rclcpp::QoS px4_qos() {
  return rclcpp::QoS(kPx4QosDepth)
      .reliability(rclcpp::ReliabilityPolicy::BestEffort)
      .history(rclcpp::HistoryPolicy::KeepLast);
}

} // namespace

ManeuverRunner::ManeuverRunner() : Node("maneuver_runner") {
  const auto qos = px4_qos();

  manual_sub_ = create_subscription<px4_msgs::msg::ManualControlSetpoint>(
      "/fmu/out/manual_control_setpoint", qos,
      [this](const px4_msgs::msg::ManualControlSetpoint::SharedPtr msg) {
        on_manual_control(msg);
      });

  status_sub_ = create_subscription<px4_msgs::msg::VehicleStatus>(
      "/fmu/out/vehicle_status_v1", qos,
      [this](const px4_msgs::msg::VehicleStatus::SharedPtr msg) {
        on_vehicle_status(msg);
      });

  pti_params_sub_ = create_subscription<px4_msgs::msg::PtiParams>(
      "/fmu/out/pti_params", qos,
      [this](const px4_msgs::msg::PtiParams::SharedPtr msg) {
        on_pti_params(msg);
      });

  torque_pub_ = create_publisher<px4_msgs::msg::VehicleTorqueSetpoint>(
      "/fmu/in/vehicle_torque_setpoint", qos);
  thrust_pub_ = create_publisher<px4_msgs::msg::VehicleThrustSetpoint>(
      "/fmu/in/vehicle_thrust_setpoint", qos);
  offboard_pub_ = create_publisher<px4_msgs::msg::OffboardControlMode>(
      "/fmu/in/offboard_control_mode", qos);

  start_srv_ = create_service<pc21_sysid_interfaces::srv::StartManeuver>(
      "~/start_maneuver",
      [this](
          const std::shared_ptr<
              pc21_sysid_interfaces::srv::StartManeuver::Request>
              req,
          std::shared_ptr<pc21_sysid_interfaces::srv::StartManeuver::Response>
              resp) { handle_start_maneuver(req, resp); });

  abort_srv_ = create_service<std_srvs::srv::Trigger>(
      "~/abort_maneuver",
      [this](const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
             std::shared_ptr<std_srvs::srv::Trigger::Response> resp) {
        handle_abort(req, resp);
      });

  using namespace std::chrono_literals;
  const auto control_period =
      std::chrono::microseconds(static_cast<int64_t>(1e6 / kControlLoopHz));
  const auto heartbeat_period =
      std::chrono::microseconds(static_cast<int64_t>(1e6 / kHeartbeatHz));

  control_timer_ =
      create_wall_timer(control_period, [this]() { control_step(); });
  heartbeat_timer_ =
      create_wall_timer(heartbeat_period, [this]() { publish_heartbeat(); });

  t_state_entered_s_ = now_s();
  RCLCPP_INFO(get_logger(), "ManeuverRunner ready, state=%s",
              state_to_string(state_));
}

double ManeuverRunner::now_s() const { return now().nanoseconds() * 1e-9; }

void ManeuverRunner::on_manual_control(
    const px4_msgs::msg::ManualControlSetpoint::SharedPtr msg) {

  if (!have_manual_) {
    // First message: initialise trigger baseline to current value
    // so the first edge-detection pass sees no transition.
    pilot_aux_trigger_prev_ = msg->aux5;
  }
  pilot_aux_trigger_ = msg->aux5;

  pilot_roll_ = msg->roll;
  pilot_pitch_ = msg->pitch;
  pilot_yaw_ = msg->yaw;
  pilot_throttle_ = msg->throttle;
  have_manual_ = true;
}

void ManeuverRunner::on_vehicle_status(
    const px4_msgs::msg::VehicleStatus::SharedPtr msg) {
  const bool previously_offboard =
      have_status_ && (nav_state_ == kNavStateOffboard);
  nav_state_ = msg->nav_state;
  have_status_ = true;

  // If the pilot left offboard while a maneuver was running, treat as abort.
  if (previously_offboard && msg->nav_state != kNavStateOffboard) {
    if (state_ == State::Arming || state_ == State::Executing ||
        state_ == State::Recovery) {
      RCLCPP_WARN(get_logger(), "Pilot left offboard mode; aborting maneuver");
      abort_maneuver("left offboard mode");
    }
  }
}

void ManeuverRunner::on_pti_params(
    const px4_msgs::msg::PtiParams::SharedPtr msg) {
  pti_amplitude_ = msg->amplitude;
  pti_man_axis_ = msg->man_axis;
  pti_man_type_ = msg->man_type;
  pti_step_time_ = msg->step_time;
  pti_swp_dur_ = msg->swp_dur;
  pti_swp_fmin_ = msg->swp_fmin;
  pti_swp_fmax_ = msg->swp_fmax;
  have_pti_params_ = true;
}

StartResult ManeuverRunner::start_maneuver(const std::string &name,
                                           const std::string &axis_str,
                                           double amplitude,
                                           const std::string &params_json) {
  if (state_ != State::Idle) {
    return {false,
            std::string("Cannot start: state=") + state_to_string(state_)};
  }
  if (nav_state_ != kNavStateOffboard) {
    return {false, "Cannot start: not in offboard mode"};
  }

  try {
    const Axis axis = parse_axis(axis_str);
    const nlohmann::json params = params_json.empty()
                                      ? nlohmann::json::object()
                                      : nlohmann::json::parse(params_json);
    active_maneuver_ = build_maneuver(name, axis, amplitude, params);
    enter_state(State::Arming, "preconditions for " + name);
    RCLCPP_INFO(get_logger(),
                "Maneuver queued: %s axis=%s amp=%.3f duration=%.2fs",
                name.c_str(), axis_str.c_str(), amplitude,
                active_maneuver_->duration());
    return {true, "Started"};
  } catch (const std::exception &e) {
    return {false, std::string("Failed to build maneuver: ") + e.what()};
  }
}

bool ManeuverRunner::abort_maneuver(const std::string &reason) {
  if (state_ == State::Arming || state_ == State::Executing ||
      state_ == State::Recovery) {
    enter_state(State::Aborted, reason);
    return true;
  }
  return false;
}

void ManeuverRunner::enter_state(State new_state, const std::string &reason) {
  RCLCPP_INFO(get_logger(), "State: %s -> %s (%s)", state_to_string(state_),
              state_to_string(new_state), reason.c_str());
  state_ = new_state;
  t_state_entered_s_ = now_s();
}

void ManeuverRunner::handle_start_maneuver(
    const std::shared_ptr<pc21_sysid_interfaces::srv::StartManeuver::Request>
        request,
    std::shared_ptr<pc21_sysid_interfaces::srv::StartManeuver::Response>
        response) {
  auto [ok, msg] = start_maneuver(request->maneuver_name, request->axis,
                                  request->amplitude, request->params_json);
  response->success = ok;
  response->message = msg;
}

void ManeuverRunner::handle_abort(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> /*request*/,
    std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
  if (abort_maneuver("abort service called")) {
    response->success = true;
    response->message = "Aborted";
  } else {
    response->success = false;
    response->message = "No maneuver active";
  }
}

void ManeuverRunner::control_step() {
  if (!have_manual_) {
    return;
  }

  check_rc_trigger();

  ManeuverDelta delta;
  const double t = now_s();

  switch (state_) {
  case State::Idle:
    break;

  case State::Arming:
    // No runtime preflight checks defined yet. Transition immediately.
    // Future: validate attitude / airspeed / altitude bounds here.
    if (active_maneuver_) {
      active_maneuver_->start(t);
      enter_state(State::Executing, "preconditions OK");
    }
    break;

  case State::Executing: {
    auto result = active_maneuver_->step(t);
    if (!result.has_value()) {
      enter_state(State::Recovery, "maneuver complete, holding for " +
                                       std::to_string(kRecoveryDefaultS) + "s");
    } else {
      delta = result.value();
    }
    break;
  }

  case State::Recovery:
    if (t - t_state_entered_s_ >= kRecoveryDefaultS) {
      active_maneuver_.reset();
      enter_state(State::Idle, "recovery complete");
    }
    break;

  case State::Aborted:
    if (t - t_state_entered_s_ >= kAbortedHoldS) {
      active_maneuver_.reset();
      enter_state(State::Idle, "reset after abort");
    }
    break;
  }

  // Sum and clamp.
  constexpr double kRollTorqueSign = 1.0;
  constexpr double kPitchTorqueSign = -1.0;
  constexpr double kYawTorqueSign = 1.0;

  const double roll = std::clamp(kRollTorqueSign * (pilot_roll_ + delta.roll),
                                 kClampLower, kClampUpper);
  const double pitch =
      std::clamp(kPitchTorqueSign * (pilot_pitch_ + delta.pitch), kClampLower,
                 kClampUpper);
  const double yaw = std::clamp(kYawTorqueSign * (pilot_yaw_ + delta.yaw),
                                kClampLower, kClampUpper);
  const double thrust_x =
      std::clamp(pilot_throttle_, kThrottleLower, kThrottleUpper);

  const int64_t t_us = static_cast<int64_t>(t * 1e6);

  px4_msgs::msg::VehicleTorqueSetpoint torque_msg;
  torque_msg.timestamp = t_us;
  torque_msg.xyz = {static_cast<float>(roll), static_cast<float>(pitch),
                    static_cast<float>(yaw)};

  px4_msgs::msg::VehicleThrustSetpoint thrust_msg;
  thrust_msg.timestamp = t_us;
  thrust_msg.xyz = {static_cast<float>(thrust_x), 0.0f, 0.0f};

  const bool offboard = have_status_ && (nav_state_ == kNavStateOffboard);
  if (offboard) {
    torque_pub_->publish(torque_msg);
    thrust_pub_->publish(thrust_msg);
  }

  // --- Latency measurement: entry to publish ---
  const double latency_s = now_s() - t;
  max_latency_s_ = std::max(max_latency_s_, latency_s);
  min_latency_s_ = std::min(min_latency_s_, latency_s);
  sum_latency_s_ += latency_s;
  if (++latency_count_ >= 2000) { // print every ~10 s at 200 Hz
    const double avg_us = (sum_latency_s_ / latency_count_) * 1e6;
    RCLCPP_INFO(
        get_logger(),
        "control_step latency: min=%.1fus avg=%.1fus max=%.1fus over %d ticks",
        min_latency_s_ * 1e6, avg_us, max_latency_s_ * 1e6, latency_count_);
    max_latency_s_ = 0.0;
    min_latency_s_ = 1e9;
    sum_latency_s_ = 0.0;
    latency_count_ = 0;
  }
}

void ManeuverRunner::publish_heartbeat() {
  px4_msgs::msg::OffboardControlMode msg;
  msg.timestamp = static_cast<int64_t>(now_s() * 1e6);
  msg.position = false;
  msg.velocity = false;
  msg.acceleration = false;
  msg.attitude = false;
  msg.body_rate = false;
  msg.thrust_and_torque = true;
  msg.direct_actuator = false;
  offboard_pub_->publish(msg);
}

void ManeuverRunner::check_rc_trigger() {
  constexpr double kTriggerThreshold = 0.5;
  const bool trigger_now = pilot_aux_trigger_ > kTriggerThreshold;
  const bool trigger_prev = pilot_aux_trigger_prev_ > kTriggerThreshold;

  if (trigger_now && !trigger_prev) {
    handle_rc_trigger();
  } else if (!trigger_now && trigger_prev) {
    abort_maneuver("RC trigger off");
  }
  pilot_aux_trigger_prev_ = pilot_aux_trigger_;
}

void ManeuverRunner::handle_rc_trigger() {
  if (!have_pti_params_) {
    RCLCPP_WARN(get_logger(), "RC trigger ignored: no PtiParams received yet "
                              "(check /fmu/out/pti_params bridge)");
    return;
  }

  // Axis from PTI_MAN_AXIS.
  std::string axis_name;
  switch (pti_man_axis_) {
  case 0:
    axis_name = "roll";
    break;
  case 1:
    axis_name = "pitch";
    break;
  case 2:
    axis_name = "yaw";
    break;
  default:
    RCLCPP_WARN(get_logger(),
                "RC trigger ignored: PTI_MAN_AXIS=%u out of range",
                pti_man_axis_);
    return;
  }

  // Maneuver type and per-type parameters from PTI_MAN_TYPE + PTI_STEP_TIME /
  // sweep params.
  std::string maneuver_name;
  nlohmann::json params;
  switch (pti_man_type_) {
  case 0: // Step
    maneuver_name = "step";
    params["step_s"] = static_cast<double>(pti_step_time_);
    break;
  case 1: // Doublet
    maneuver_name = "doublet";
    params["pulse_s"] = static_cast<double>(pti_step_time_);
    break;

  case 2: // multistep_3211
    maneuver_name = "multistep_3211";
    params["dt_s"] = static_cast<double>(pti_step_time_);
    break;

  case 3: // Sweep
    if (!(pti_swp_fmin_ < pti_swp_fmax_)) {
      RCLCPP_WARN(get_logger(),
                  "RC trigger ignored: invalid sweep range "
                  "(PTI_SWP_FMIN=%.3f must be < PTI_SWP_FMAX=%.3f)",
                  pti_swp_fmin_, pti_swp_fmax_);
      return;
    }
    if (pti_swp_dur_ <= 0.0f) {
      RCLCPP_WARN(get_logger(),
                  "RC trigger ignored: PTI_SWP_DUR=%.3f must be > 0",
                  pti_swp_dur_);
      return;
    }
    maneuver_name = "sweep";
    params["duration_s"] = static_cast<double>(pti_swp_dur_);
    params["f_min_hz"] = static_cast<double>(pti_swp_fmin_);
    params["f_max_hz"] = static_cast<double>(pti_swp_fmax_);
    break;

  case 4: // Multisine
    if (!(pti_swp_fmin_ < pti_swp_fmax_)) {
      RCLCPP_WARN(get_logger(),
                  "RC trigger ignored: invalid multisine range "
                  "(PTI_SWP_FMIN=%.3f must be < PTI_SWP_FMAX=%.3f)",
                  pti_swp_fmin_, pti_swp_fmax_);
      return;
    }
    if (pti_swp_dur_ <= 0.0f || pti_swp_fmin_ * pti_swp_dur_ < 1.0f) {
      RCLCPP_WARN(get_logger(),
                  "RC trigger ignored: PTI_SWP_DUR=%.3f too short for "
                  "PTI_SWP_FMIN=%.3f (need f_min*T >= 1)",
                  pti_swp_dur_, pti_swp_fmin_);
      return;
    }
    maneuver_name = "multisine";
    params["duration_s"] = static_cast<double>(pti_swp_dur_);
    params["f_min_hz"] = static_cast<double>(pti_swp_fmin_);
    params["f_max_hz"] = static_cast<double>(pti_swp_fmax_);
    break;

  default:
    RCLCPP_WARN(get_logger(), "RC trigger ignored: PTI_MAN_TYPE=%u unknown",
                pti_man_type_);
    return;
  }

  const double amplitude =
      std::clamp(static_cast<double>(pti_amplitude_), -1.0, 1.0);

  RCLCPP_INFO(get_logger(), "RC trigger: %s axis=%s amp=%.3f params=%s",
              maneuver_name.c_str(), axis_name.c_str(), amplitude,
              params.dump().c_str());

  auto [ok, msg] =
      start_maneuver(maneuver_name, axis_name, amplitude, params.dump());
  if (!ok) {
    RCLCPP_WARN(get_logger(), "RC trigger refused: %s", msg.c_str());
  }
}

} // namespace pc21_sysid
