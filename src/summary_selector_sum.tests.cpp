#include "summary_selector_sum.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <doctest/doctest.h>

TEST_SUITE("suse::summary_selector_sum function") {
  TEST_CASE("Sum function simple") {
    suse::summary_selector_sum<int> selector("a(b*c)*d", 10, 10);
    std::string stream = "a3b4a1b2c5d6";

    for (std::size_t idx = 0; idx < stream.length(); idx += 2) {
      std::size_t event_type_index = idx;
      std::size_t event_value_index = idx + 1;
      auto event_type = stream[event_type_index];
      auto event_value = stream[event_value_index];
      selector.process_event({event_type, idx});
    }
    std::cout << std::endl;
    CHECK(selector.sum_of_contained_complete_matches() == 140);
  }
}