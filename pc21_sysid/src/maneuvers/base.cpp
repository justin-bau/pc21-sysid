#include "pc21_sysid/maneuvers/base.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace pc21_sysid {

Axis parse_axis(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (s == "roll")
    return Axis::Roll;
  if (s == "pitch")
    return Axis::Pitch;
  if (s == "yaw")
    return Axis::Yaw;
  throw std::invalid_argument("Unknown axis: " + s);
}

} // namespace pc21_sysid
