#ifndef SUSE_EVENT_HPP
#define SUSE_EVENT_HPP

#include <iostream>

#include <cstddef>

namespace suse {
struct event {
  char type;
  int value;
  std::size_t timestamp;


  friend constexpr auto operator<=>(const event &, const event &) = default;
};

inline std::istream &operator>>(std::istream &in, event &e) {
  return in >> e.type >> e.value >> e.timestamp;
}
} // namespace suse

#endif
