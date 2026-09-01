#pragma once

#include <string>

#include <memory>
#include <rclcpp/rclcpp.hpp>

#include <px4_msgs/msg/manual_control_setpoint.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>
#include <px4_msgs/msg/vehicle_thrust_setpoint.hpp>
#include <px4_msgs/msg/vehicle_torque_setpoint.hpp>

#include <pc21_sysid_interfaces/srv/start_maneuver.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <px4_msgs/msg/pti_params.hpp>

#include "pc21_sysid/maneuvers/base.hpp"

namespace pc21_sysid {

enum class State {
  Idle,
  Arming,
  Executing,
  Recovery,
  Aborted,
};

struct StartResult {
  bool ok;
  std::string message;
};

const char *state_to_string(State s);

class ManeuverRunner : public rclcpp::Node {
public:
  ManeuverRunner();

private:
  // Subscriptions / publishers
  rclcpp::Subscription<px4_msgs::msg::ManualControlSetpoint>::SharedPtr
      manual_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr status_sub_;

  rclcpp::Publisher<px4_msgs::msg::VehicleTorqueSetpoint>::SharedPtr
      torque_pub_;
  rclcpp::Publisher<px4_msgs::msg::VehicleThrustSetpoint>::SharedPtr
      thrust_pub_;
  rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr
      offboard_pub_;

  // Services
  rclcpp::Service<pc21_sysid_interfaces::srv::StartManeuver>::SharedPtr
      start_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr abort_srv_;

  // Timers for the different loops
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // Latest received state
  double pilot_roll_ = 0.0;
  double pilot_pitch_ = 0.0;
  double pilot_yaw_ = 0.0;
  double pilot_throttle_ = 0.0;
  bool have_manual_ = false;

  uint8_t nav_state_ = 0;
  bool have_status_ = false;

  // RC auxiliary channels
  double pilot_aux_trigger_ = 0.0; // aux5, 2-pos trigger
  double pilot_aux_trigger_prev_ = 0.0;

  void check_rc_trigger();
  void handle_rc_trigger();

  rclcpp::Subscription<px4_msgs::msg::PtiParams>::SharedPtr pti_params_sub_;

  void on_pti_params(const px4_msgs::msg::PtiParams::SharedPtr msg);

  bool have_pti_params_ = false;
  float pti_amplitude_ = 0.0f;
  uint8_t pti_man_axis_ = 0;
  uint8_t pti_man_type_ = 0;
  float pti_step_time_ = 0.0f;
  float pti_swp_dur_ = 0.0f;
  float pti_swp_fmin_ = 0.0f;
  float pti_swp_fmax_ = 0.0f;

  // Latency measurement: time from control_step entry to publish.
  double max_latency_s_ = 0.0;
  double min_latency_s_ = 1e9;
  double sum_latency_s_ = 0.0;
  int latency_count_ = 0;

  // State machine
  State state_ = State::Idle;
  double t_state_entered_s_ = 0.0;
  std::unique_ptr<Maneuver> active_maneuver_;

  // Callbacks
  void
  on_manual_control(const px4_msgs::msg::ManualControlSetpoint::SharedPtr msg);
  void on_vehicle_status(const px4_msgs::msg::VehicleStatus::SharedPtr msg);

  void handle_start_maneuver(
      const std::shared_ptr<pc21_sysid_interfaces::srv::StartManeuver::Request>
          request,
      std::shared_ptr<pc21_sysid_interfaces::srv::StartManeuver::Response>
          response);

  void
  handle_abort(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
               std::shared_ptr<std_srvs::srv::Trigger::Response> response);

  void control_step();
  void publish_heartbeat();

  // State transitions and logic
  StartResult start_maneuver(const std::string &name,
                             const std::string &axis_str, double amplitude,
                             const std::string &params_json);
  bool abort_maneuver(const std::string &reason);
  void enter_state(State new_state, const std::string &reason);
  double now_s() const;
};

} // namespace pc21_sysid
