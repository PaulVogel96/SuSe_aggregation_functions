#ifndef SUSE_SUMMARY_SELECTOR_GEO_MEAN_HPP
#define SUSE_SUMMARY_SELECTOR_GEO_MEAN_HPP

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
class summary_selector_prod : public summary_selector_base<counter_type> {
  public:
    summary_selector_prod(std::string_view query, std::size_t summary_size, std::size_t time_window_size, std::size_t time_to_live = std::numeric_limits<std::size_t>::max())
        : summary_selector_base<counter_type>(query, summary_size, time_window_size, time_to_live),
          total_prod_counter_{this->automaton_.number_of_states()},
          total_detected_prod_counter_{this->automaton_.number_of_states()},
          active_window_prod_extension_{create_additional_window_info(time_window_size)} {
    
        //prod counters must be initialzed with 1
        std::fill(total_prod_counter_.begin(), total_prod_counter_.end(), 1);
        std::fill(total_detected_prod_counter_.begin(), total_detected_prod_counter_.end(), 1);
    }

    void remove_event(std::size_t cache_index) override {
        assert(cache_index < this->cache_.size());

        this->total_counter_ -= this->cache_[cache_index].state_counter;
        this->total_prod_counter_ -= this->prod_cache_[cache_index].state_counter;
        const auto removed_timestamp = this->timestamp_at(cache_index);
        if (cache_index < this->active_window_.start_idx)
            --this->active_window_.start_idx;

        this->cache_.erase(this->cache_.begin() + cache_index);
        this->prod_cache_.erase(this->prod_cache_.begin() + cache_index);

        if (this->cache_.empty()) {
            this->active_window_.start_idx = 0;
            this->reset_counters(this->active_window_);
            this->reset_additional_window_counters(this->active_window_prod_extension_);
            return;
        }

        this->replay_affected_range(cache_index, removed_timestamp);
        if (this->in_shared_window(this->current_time_, removed_timestamp))
            this->replay_time_window(this->active_window_, std::span{this->cache_.begin() + this->active_window_.start_idx, this->cache_.end()});
    }

    void add_event(const event &new_event) override {
        auto global_change_count = advance(this->active_window_.total_counter, this->per_character_edges_, new_event.type);
        auto global_change_prod = advance_prod(this->active_window_.total_counter, this->active_window_prod_extension_.total_prod_counter, this->per_character_edges_, new_event);

        this->active_window_.total_counter += global_change_count;
        this->total_counter_ += global_change_count;
        this->total_detected_counter_ += global_change_count;

        this->active_window_prod_extension_.total_prod_counter *= global_change_prod;
        this->total_prod_counter_ *= global_change_prod;
        this->total_detected_prod_counter_ *= global_change_prod;

        const auto active_window_size = this->cache_.size() - this->active_window_.start_idx;
        for (std::size_t i = 0; i < active_window_size; ++i) {
            const auto cache_idx = this->active_window_.start_idx + i;

            const auto local_change_count = advance(this->active_window_.per_event_counters[i], this->per_character_edges_, new_event.type);
            this->cache_[cache_idx].state_counter += local_change_count;
            this->active_window_.per_event_counters[i] += local_change_count;

            const auto local_change_prod = advance_prod(this->active_window_.per_event_counters[i], this->active_window_prod_extension_.per_event_prod_counters[i], this->per_character_edges_, new_event);
            this->prod_cache_[cache_idx].state_counter *= local_change_prod;
            this->active_window_prod_extension_.per_event_prod_counters[i] *= local_change_prod;
        }

        this->active_window_.per_event_counters.push_back(global_change_count);
        this->cache_.emplace_back(new_event, std::move(global_change_count));

        this->active_window_prod_extension_.per_event_prod_counters.push_back(global_change_prod);
        this->prod_cache_.emplace_back(new_event, std::move(global_change_prod));
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

    counter_type prod_of_contained_complete_matches() const {
        return prod_over_complete_matches(total_prod_counter_);
    }

    counter_type prod_of_contained_partial_matches() const {
        return prod_over_partial_matches(total_prod_counter_);
    }

    counter_type prod_of_detected_complete_matches() const {
        return prod_over_complete_matches(total_detected_prod_counter_);
    }

    counter_type prod_of_detected_partial_matches() const {
        return prod_over_partial_matches(total_detected_prod_counter_);
    }

    counter_type geometric_mean_of_contained_complete_matches() const {
        return calculate_geometric_mean_over_complete_matches(total_prod_counter_, this->total_counter_);
    }

    counter_type geometric_mean_of_contained_partial_matches() const {
        return calculate_geometric_mean_over_partial_matches(total_prod_counter_, this->total_counter_);
    }

    counter_type geometric_mean_of_detected_complete_matches() const {
        return calculate_geometric_mean_over_complete_matches(total_detected_prod_counter_, this->total_detected_counter_);
    }

    counter_type geometric_mean_of_detected_partial_matches() const {
        return calculate_geometric_mean_over_partial_matches(total_detected_prod_counter_, this->total_detected_counter_);
    }

  private:
    std::vector<cache_entry<counter_type>> prod_cache_;

    struct window_info_prod_extension {
        execution_state_counter<counter_type> total_prod_counter;
        suse::ring_buffer<execution_state_counter<counter_type>> per_event_prod_counters;
        friend auto operator<=>(const window_info_prod_extension &, const window_info_prod_extension &) = default;
    };
    window_info_prod_extension active_window_prod_extension_;

    execution_state_counter<counter_type> total_prod_counter_, total_detected_prod_counter_;

    auto create_additional_window_info(std::size_t window_size) const {
        window_info_prod_extension wnd{
            execution_state_counter<counter_type>{this->automaton_.number_of_states()},
            ring_buffer<execution_state_counter<counter_type>>{window_size, execution_state_counter<counter_type>{this->automaton_.number_of_states()}}};

        reset_additional_window_counters(wnd);
        return wnd;
    }

    void reset_additional_window_counters(window_info_prod_extension &window) const {
        std::fill(window.total_prod_counter.begin(), window.total_prod_counter.end(), 1);
        window.per_event_prod_counters.clear();
    }

    counter_type prod_over_complete_matches(const execution_state_counter<counter_type> &counter) const {
        assert(counter.size() == automaton.number_of_states());

        counter_type product{1};
        for (std::size_t i = 0; i < counter.size(); ++i) {
            if (this->automaton_.states()[i].is_final)
                product *= counter[i];
        }

        return product;
    }

    counter_type prod_over_partial_matches(const execution_state_counter<counter_type> &counter) const {
        assert(counter.size() == automaton.number_of_states());

        counter_type product{1};
        for (std::size_t i = 0; i < counter.size(); ++i) {
            if (!this->automaton_.states()[i].is_final)
                product *= counter[i];
        }

        return product;
    }

    counter_type calculate_geometric_mean_over_complete_matches(const execution_state_counter<counter_type> &prod_counter, const execution_state_counter<counter_type> &count_counter) const {
        assert(prod_counter.size() == count_counter.size() == automaton.number_of_states());

        counter_type product{1};
        counter_type count{0};
        for (std::size_t i = 0; i < prod_counter.size(); ++i) {
            if (this->automaton_.states()[i].is_final) {
                product *= prod_counter[i];
                count += count_counter[i];
            }
        }

        return pow(product, 1 / count);
    }

    counter_type calculate_geometric_mean_over_partial_matches(const execution_state_counter<counter_type> &prod_counter, const execution_state_counter<counter_type> &count_counter) const {
        assert(prod_counter.size() == count_counter.size() == automaton.number_of_states());

        counter_type product{1};
        counter_type count{0};
        for (std::size_t i = 0; i < prod_counter.size(); ++i) {
            if (!this->automaton_.states()[i].is_final) {
                product *= prod_counter[i];
                count += count_counter[i];
            }
        }

        return pow(product, 1 / count);
    }
};

} // namespace suse

#endif
