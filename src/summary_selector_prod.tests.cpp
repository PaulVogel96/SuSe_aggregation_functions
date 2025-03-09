#include "summary_selector_prod.hpp"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <doctest/doctest.h>

TEST_SUITE("suse::summary_selector_prod function") {
    TEST_CASE("One Multiplication") {
        suse::summary_selector_prod<int> selector("a(b*c)*d", 10, 10);
        std::string stream = "a3b4";

        for (std::size_t idx = 0; idx < stream.length(); idx += 2) {
            std::size_t event_type_index = idx;
            std::size_t event_value_index = idx + 1;
            auto event_type = stream[event_type_index];
            int event_value = stream[event_value_index] - '0'; // convert char to int
            selector.process_event({event_type, event_value, idx / 2});
        }
    }

    TEST_CASE("Prod function simple") {
        suse::summary_selector_prod<boost::int64_t> selector("a(b*c)*d", 10, 10);
        std::string stream = "a3b5a2b4c2d5";

        for (std::size_t idx = 0; idx < stream.length(); idx += 2) {
            std::size_t event_type_index = idx;
            std::size_t event_value_index = idx + 1;
            auto event_type = stream[event_type_index];
            int event_value = stream[event_value_index] - '0'; // convert char to int
            selector.process_event({event_type, event_value, idx / 2});
        }
        CHECK(selector.number_of_contained_complete_matches() == 8);
        CHECK(selector.prod_of_contained_complete_matches() == 77760000000000);
    }

    TEST_CASE("Geometric Mean") {
        using namespace boost::multiprecision;
        suse::summary_selector_prod<cpp_dec_float_50> selector("a(b*c)*d", 10, 10);
        std::string stream = "a3b5a2b4c2d5";

        for (std::size_t idx = 0; idx < stream.length(); idx += 2) {
            std::size_t event_type_index = idx;
            std::size_t event_value_index = idx + 1;
            auto event_type = stream[event_type_index];
            int event_value = stream[event_value_index] - '0'; // convert char to int
            selector.process_event({event_type, event_value, idx / 2});
        }
        CHECK(selector.number_of_contained_complete_matches() == 8);
        CHECK(selector.prod_of_contained_complete_matches() == 77760000000000);
        CHECK(abs(selector.geometric_mean_of_contained_complete_matches() - cpp_dec_float_50("54.4934785300")) < cpp_dec_float_50("1e-10")); //test with small epsilon
    }
}