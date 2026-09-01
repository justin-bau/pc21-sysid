#include "pc21_sysid/maneuvers/registry.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "pc21_sysid/maneuvers/doublet.hpp"
#include "pc21_sysid/maneuvers/multisine.hpp"
#include "pc21_sysid/maneuvers/multistep_3211.hpp"
#include "pc21_sysid/maneuvers/step.hpp"
#include "pc21_sysid/maneuvers/sweep.hpp"

namespace pc21_sysid {

using FactoryFn = std::function<std::unique_ptr<Maneuver>(
    Axis, double, const nlohmann::json &)>;

namespace {

// Registry table. Add new maneuvers here.
const std::unordered_map<std::string, FactoryFn> &registry() {
  static const std::unordered_map<std::string, FactoryFn> kRegistry = {
      {"step", Step::from_params},
      {"doublet", Doublet::from_params},
      {"multistep_3211", Multistep3211::from_params},
      {"sweep", Sweep::from_params},
      {"multisine", Multisine::from_params},
  };
  return kRegistry;
}

} // namespace

std::unique_ptr<Maneuver> build_maneuver(const std::string &name, Axis axis,
                                         double amplitude,
                                         const nlohmann::json &params) {
  const auto &reg = registry();
  auto it = reg.find(name);
  if (it == reg.end()) {
    std::string available;
    for (const auto &entry : reg) {
      if (!available.empty())
        available += ", ";
      available += entry.first;
    }
    throw std::invalid_argument("Unknown maneuver '" + name +
                                "'; available: " + available);
  }
  return it->second(axis, amplitude, params);
}

} // namespace pc21_sysid
