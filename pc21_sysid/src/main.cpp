#include <memory>
#include <rclcpp/rclcpp.hpp>

#include "pc21_sysid/maneuver_runner.hpp"

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<pc21_sysid::ManeuverRunner>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
