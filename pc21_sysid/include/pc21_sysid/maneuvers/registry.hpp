#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "pc21_sysid/maneuvers/base.hpp"

namespace pc21_sysid {

// Builds a maneuver by name
// Throws std::invalid_argument on unknown names
// New maneuvers are registered in registry.cpp
std::unique_ptr<Maneuver> build_maneuver(const std::string &name, Axis axis,
                                         double amplitude,
                                         const nlohmann::json &params);

} // namespace pc21_sysid
