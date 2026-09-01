cmake_minimum_required(VERSION 3.8)
project(pc21_sysid)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclpy REQUIRED)
find_package(std_srvs REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(pc21_sysid_interfaces REQUIRED)

# ---- maneuver_runner C++ executable ----

add_executable(maneuver_runner
  src/main.cpp
  src/maneuver_runner.cpp
  src/maneuvers/base.cpp
  src/maneuvers/step.cpp
  src/maneuvers/doublet.cpp
  src/maneuvers/multistep_3211.cpp
  src/maneuvers/sweep.cpp
  src/maneuvers/multisine.cpp
  src/maneuvers/registry.cpp
)

target_include_directories(maneuver_runner PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

ament_target_dependencies(maneuver_runner
  rclcpp
  std_srvs
  px4_msgs
  pc21_sysid_interfaces
)

install(TARGETS maneuver_runner
  DESTINATION lib/${PROJECT_NAME}
)

# ---- Python utilities (stub + logger) ----

ament_python_install_package(${PROJECT_NAME})

install(PROGRAMS
  scripts/manual_control_stub
  scripts/angular_velocity_logger
  DESTINATION lib/${PROJECT_NAME}
)

# ---- Testing ----

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND FALSE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
