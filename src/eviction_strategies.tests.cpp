#include "eviction_strategies.hpp"
#include "summary_selector_base.hpp"
#include "summary_selector_base_impl.hpp"
#include "summary_selector_count.hpp"

#include <doctest/doctest.h>

#include <unordered_map>

TEST_SUITE("suse::eviction_strategies") {
  TEST_CASE("fifo") {
    suse::summary_selector_count<int> selector{"abc", 3, 3};

    const std::string_view input = "abcdefghijklmnopqrstuvwxyz";

    for (std::size_t idx = 0; auto c : input)
      selector.process_event({c, idx++}, suse::eviction_strategies::fifo);

    suse::summary_selector_count<int> correct_selector{"abc", 3, 3};
    for (std::size_t idx = 23; auto c : input.substr(23))
      correct_selector.process_event({c, idx++});

    CHECK(selector == correct_selector);
  }
}
