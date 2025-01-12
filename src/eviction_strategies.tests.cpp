#include "eviction_strategies.hpp"
#include "summary_selector.hpp"

#include <doctest/doctest.h>

#include <unordered_map>

TEST_SUITE("suse::eviction_strategies")
{
	/*
	* This Test is testing evictions strategies. He does this by 
	* limiting the cache to three events.Both selectors have the 
	* same regular expression, but one has the Eviction Strategy 
	* FIFO and processes the whole stream. The second selector only 
	* processes the last three events. The first selector should
	* match with the first 3 events (abc), but they will be removed 
	* when more events are added to the cache. 
	* So, when comparing both selectors after parsing the events, 
	* they shoudl only have xyz in their cache. This means that 
	* events are removed.
	*/
	TEST_CASE("fifo")
	{
		suse::summary_selector<int> selector{"abc",3,3};

		const std::string_view input = "abcdefghijklmnopqrstuvwxyz";
		
		for (std::size_t idx = 0; auto c: input) {
			//std::cout << "Fifo selector processing event: " << c << 0 << std::endl;
			selector.process_event({ c, idx++, 0 }, suse::eviction_strategies::fifo);
		}

		suse::summary_selector<int> correct_selector{"abc",3,3};
		for (std::size_t idx = 23; auto c: input.substr(23)) {
			//std::cout << "Normal selector processing event: " << c << 0 << std::endl;
			correct_selector.process_event({ c, idx++, 0 });
		}

		CHECK(selector==correct_selector);
	}
}
