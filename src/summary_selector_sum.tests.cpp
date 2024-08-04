#include "summary_selector.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <doctest/doctest.h>

TEST_SUITE("suse::summary_selector sum function")
{
	TEST_CASE("simple, irrelevant time window")
	{
		suse::summary_selector<int> selector("a(b*c)*d", 10, 10);
		std::string stream = "a3b4a1b2c5d6";

		for (std::size_t idx = 0; idx < stream.length(); idx + 2) {
			std::size_t event_index = idx;
			auto event_type = stream[event_index];
			auto event_value = stream[event_index++];
			selector.process_event({ event_type, idx, event_value });
		}
		CHECK(selector.number_of_contained_complete_matches() == 8);
		CHECK(selector.sum_of_contained_complete_matches() == 140);
	}
}