#ifndef SUSE_SUMMARY_SELECTOR_COUNT_HPP
#define SUSE_SUMMARY_SELECTOR_COUNT_HPP

#include "edgelist.hpp"
#include "event.hpp"
#include "execution_state_counter.hpp"
#include "nfa.hpp"
#include "ring_buffer.hpp"
#include "summary_selector_base.hpp"

#include <concepts>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cstddef>

namespace suse {

template <typename counter_type>
class summary_selector_count : public summary_selector_base<counter_type, window_info<counter_type>> {
  public:
    summary_selector_count(std::string_view query, std::size_t summary_size, std::size_t time_window_size, std::size_t time_to_live = std::numeric_limits<std::size_t>::max())
        : summary_selector_base<counter_type, window_info<counter_type>>(query, summary_size, time_window_size, time_to_live) {}

    void remove_event(std::size_t cache_index) override {
        assert(cache_index < this->cache_.size());

        this->total_counter_ -= this->cache_[cache_index].state_counter;
        const auto removed_timestamp = this->timestamp_at(cache_index);
        if (cache_index < this->active_window_.start_idx)
            --this->active_window_.start_idx;

        this->cache_.erase(this->cache_.begin() + cache_index);

        if (this->cache_.empty()) {
            this->active_window_.start_idx = 0;
            this->reset_counters(this->active_window_);
            return;
        }

        this->replay_affected_range(cache_index, removed_timestamp);
        if (this->in_shared_window(this->current_time_, removed_timestamp))
            this->replay_time_window(this->active_window_, std::span{this->cache_.begin() + this->active_window_.start_idx, this->cache_.end()});
    }

    void add_event(const event &new_event) override {
        auto global_counter_change = advance(this->active_window_.total_counter, this->per_character_edges_, new_event.type);
        this->active_window_.total_counter += global_counter_change;
        this->total_counter_ += global_counter_change;
        this->total_detected_counter_ += global_counter_change;

        const auto active_window_size = this->cache_.size() - this->active_window_.start_idx;
        for (std::size_t i = 0; i < active_window_size; ++i) {
            const auto cache_idx = this->active_window_.start_idx + i;

            const auto local_change = advance(this->active_window_.per_event_counters[i], this->per_character_edges_, new_event.type);
            this->cache_[cache_idx].state_counter += local_change;
            this->active_window_.per_event_counters[i] += local_change;
        }

        this->active_window_.per_event_counters.push_back(global_counter_change);
        this->cache_.emplace_back(new_event, std::move(global_counter_change));
    }

    counter_type number_of_contained_complete_matches() const {
        return number_of_complete_matches(this->total_counter_);
    }

    counter_type number_of_contained_partial_matches() const {
        return number_of_partial_matches(this->total_counter_);
    }

    counter_type number_of_detected_complete_matches() const {
        return number_of_complete_matches(this->total_detected_counter_);
    }

    counter_type number_of_detected_partial_matches() const {
        return number_of_partial_matches(this->total_detected_counter_);
    }

    counter_type number_of_complete_matches(const execution_state_counter<counter_type> &counter) const {
        assert(counter.size() == automaton.number_of_states());

        counter_type sum{0};
        for (std::size_t i = 0; i < counter.size(); ++i) {
            if (this->automaton_.states()[i].is_final)
                sum += counter[i];
        }

        return sum;
    }

    counter_type number_of_partial_matches(const execution_state_counter<counter_type> &counter) const {
        assert(counter.size() == automaton.number_of_states());

        counter_type sum{0};
        for (std::size_t i = 0; i < counter.size(); ++i) {
            if (!this->automaton_.states()[i].is_final)
                sum += counter[i];
        }

        return sum;
    }
};

} // namespace suse

#endif
