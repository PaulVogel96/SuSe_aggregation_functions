#ifndef SUSE_SUMMARY_SELECTOR_SUM_HPP
#define SUSE_SUMMARY_SELECTOR_SUM_HPP

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
class summary_selector_sum : public summary_selector_base<counter_type> {
  public:
    summary_selector_sum(std::string_view query, std::size_t summary_size, std::size_t time_window_size, std::size_t time_to_live = std::numeric_limits<std::size_t>::max())
        : summary_selector_base<counter_type>(query, summary_size, time_window_size, time_to_live),
          total_sum_counter_{this->automaton_.number_of_states()},
          total_detected_sum_counter_{this->automaton_.number_of_states()},
          active_window_sum_extension_{create_additional_window_info(time_window_size)} {}

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
        auto global_change_count = advance(this->active_window_.total_counter, this->per_character_edges_, new_event.type);
        auto global_change_sum = advance_sum(this->active_window_.total_counter, this->active_window_sum_extension_.total_sum_counter, this->per_character_edges_, new_event);

        this->active_window_.total_counter += global_change_count;
        this->total_counter_ += global_change_count;
        this->total_detected_counter_ += global_change_count;
        
        this->active_window_sum_extension_.total_sum_counter += global_change_sum;
        this->total_sum_counter_ += global_change_sum;
        this->total_detected_sum_counter_ += global_change_sum;

        const auto active_window_size = this->cache_.size() - this->active_window_.start_idx;
        for (std::size_t i = 0; i < active_window_size; ++i) {
            const auto cache_idx = this->active_window_.start_idx + i;

            const auto local_change_count = advance(this->active_window_.per_event_counters[i], this->per_character_edges_, new_event.type);
            this->cache_[cache_idx].state_counter += local_change_count;
            this->active_window_.per_event_counters[i] += local_change_count;

            const auto local_change_sum = advance_sum(this->active_window_.per_event_counters[i], this->active_window_sum_extension_.per_event_sum_counters[i], this->per_character_edges_, new_event);
            this->sum_cache_[cache_idx].state_counter += local_change_sum;
            this->active_window_sum_extension_.per_event_sum_counters[i] += local_change_sum;
        }

        this->active_window_.per_event_counters.push_back(global_change_count);
        this->cache_.emplace_back(new_event, std::move(global_change_count));

        this->active_window_sum_extension_.per_event_sum_counters.push_back(global_change_sum);
        this->sum_cache_.emplace_back(new_event, std::move(global_change_sum));
    }

    counter_type number_of_contained_complete_matches() const {
        return this->sum_over_complete_matches(this->total_counter_);
    }

    counter_type number_of_contained_partial_matches() const {
        return this->sum_over_partial_matches(this->total_counter_);
    }

    counter_type number_of_detected_complete_matches() const {
        return this->sum_over_complete_matches(this->total_detected_counter_);
    }

    counter_type number_of_detected_partial_matches() const {
        return this->sum_over_partial_matches(this->total_detected_counter_);
    }

    counter_type sum_of_contained_complete_matches() const {
        return this->sum_over_complete_matches(total_sum_counter_);
    }

    counter_type sum_of_contained_partial_matches() const {
        return this->sum_over_partial_matches(total_sum_counter_);
    }

    counter_type sum_of_detected_complete_matches() const {
        return this->sum_over_complete_matches(total_detected_sum_counter_);
    }

    counter_type sum_of_detected_partial_matches() const {
        return this->sum_over_partial_matches(total_detected_sum_counter_);
    }

  private:
    std::vector<cache_entry<counter_type>> sum_cache_;

    struct window_info_sum_extension {
        execution_state_counter<counter_type> total_sum_counter;
        suse::ring_buffer<execution_state_counter<counter_type>> per_event_sum_counters;
        friend auto operator<=>(const window_info_sum_extension &, const window_info_sum_extension &) = default;
    };
    window_info_sum_extension active_window_sum_extension_;

    execution_state_counter<counter_type> total_sum_counter_, total_detected_sum_counter_;

    auto create_additional_window_info(std::size_t window_size) const {
        window_info_sum_extension wnd{
            execution_state_counter<counter_type>{this->automaton_.number_of_states()},
            ring_buffer<execution_state_counter<counter_type>>{window_size, execution_state_counter<counter_type>{this->automaton_.number_of_states()}}
        };

        reset_additional_window_counters(wnd);
        return wnd;
    }

    void reset_additional_window_counters(window_info_sum_extension &window) const {
        window.total_sum_counter *= 0; // performance!
        window.total_sum_counter[this->automaton_.initial_state_id()] = 0;
        window.per_event_sum_counters.clear();
    }
};

} // namespace suse

#endif
