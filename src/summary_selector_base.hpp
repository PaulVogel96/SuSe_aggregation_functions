#ifndef SUSE_SUMMARY_SELECTOR_BASE_HPP
#define SUSE_SUMMARY_SELECTOR_BASE_HPP

#include "edgelist.hpp"
#include "event.hpp"
#include "execution_state_counter.hpp"
#include "nfa.hpp"
#include "regex.hpp"
#include "ring_buffer.hpp"

#include <concepts>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cstddef>

namespace suse {

template <typename T, typename cache_type>
concept callable_eviction_strategy = requires(const T strategy, const cache_type cache, const event e) {
    { strategy(cache, e) }
    ->std::convertible_to<std::optional<std::size_t>>;
};

template <typename T, typename cache_type>
concept eviction_strategy_object = requires(const T strategy, const cache_type cache, const event e) {
    { strategy.select(cache, e) }
    ->std::convertible_to<std::optional<std::size_t>>;
};

template <typename T, typename cache_type>
concept eviction_strategy = callable_eviction_strategy<T, cache_type> || eviction_strategy_object<T, cache_type>;

template <typename counter_type>
class summary_selector_base {
  public:
    summary_selector_base(std::string_view query, std::size_t summary_size, std::size_t time_window_size, std::size_t time_to_live) : automaton_{parse_regex(query)},
                                                                                                                                      per_character_edges_{compute_edges_per_character(automaton_)},
                                                                                                                                      time_to_live_{time_to_live},
                                                                                                                                      cache_{},
                                                                                                                                      total_counter_{automaton_.number_of_states()},
                                                                                                                                      total_detected_counter_{automaton_.number_of_states()},
                                                                                                                                      active_window_{create_window_info(time_window_size)} {
        cache_.reserve(summary_size);
    }

    virtual ~summary_selector_base() = default;

    template <eviction_strategy<summary_selector_base<counter_type>> strategy_type>
    void process_event(const event &new_event, const strategy_type &strategy) {
        current_time_ = new_event.timestamp;
        update_window(active_window_, new_event.timestamp);
        purge_expired();

        const auto select_idx_to_evict = [&]() -> std::optional<std::size_t> {
            if constexpr (callable_eviction_strategy<strategy_type, summary_selector_base>)
                return strategy(*this, new_event);
            else
                return strategy.select(*this, new_event);
        };

        if (cache_.size() == cache_.capacity()) {
            if (auto to_remove = select_idx_to_evict(); to_remove)
                remove_event(*to_remove);
        }

        if (cache_.size() < cache_.capacity())
            add_event(new_event);
    }

    void process_event(const event &new_event) {
        process_event(new_event, [](const auto &cache, const auto &event) { return std::nullopt; });
    }

    virtual void remove_event(std::size_t cache_index) = 0;

    auto cached_events() const {
        return std::span{cache_.begin(), cache_.end()};
    }

    const auto &active_window() const {
        return active_window_;
    }

    const auto &total_counts() const {
        return total_counter_;
    }

    const auto &active_counts() const {
        return active_window_.total_counter;
    }

    auto current_time() const {
        return current_time_;
    }

    const auto &automaton() const {
        return automaton_;
    }

    auto time_window_size() const {
        return active_window_.per_event_counters.capacity();
    }

    friend bool operator==(const summary_selector_base<counter_type> &lhs, const summary_selector_base<counter_type> &rhs) {
        if (lhs.per_character_edges_ != rhs.per_character_edges_)
            return false;
        if (lhs.cache_ != rhs.cache_)
            return false;
        if (lhs.total_counter_ != rhs.total_counter_)
            return false;
        if (lhs.current_time_ != rhs.current_time_)
            return false;

        return lhs.active_window_ == rhs.active_window_;
    }

  protected:
    nfa automaton_;
    edgelist per_character_edges_;
    std::size_t time_to_live_;

    struct cache_entry {
        event cached_event;
        execution_state_counter<counter_type> state_counter;
        friend auto operator<=>(const cache_entry &, const cache_entry &) = default;
    };
    std::vector<cache_entry> cache_;

    struct window_info {
        execution_state_counter<counter_type> total_counter;
        suse::ring_buffer<execution_state_counter<counter_type>> per_event_counters;
        std::size_t start_idx;
        friend auto operator<=>(const window_info &, const window_info &) = default;
    };

    execution_state_counter<counter_type> total_counter_, total_detected_counter_;
    window_info active_window_;

    std::size_t current_time_{0};

    virtual void add_event(const event &new_event) = 0;
    void purge_expired() {
        std::size_t purge_until = 0;
        while (purge_until < cache_.size() && current_time() - cache_[purge_until].cached_event.timestamp > time_to_live_) {
            total_counter_ -= cache_[purge_until].state_counter;
            const auto removed_timestamp = timestamp_at(purge_until);
            cache_[purge_until].cached_event.timestamp = std::numeric_limits<std::size_t>::max(); // dirty hack to make the replay ignore this event
            std::fill(cache_[purge_until].state_counter.begin(), cache_[purge_until].state_counter.end(), 0);
            replay_affected_range(purge_until + 1, removed_timestamp);
            ++purge_until;
        }

        if (purge_until == 0)
            return;

        cache_.erase(cache_.begin(), cache_.begin() + purge_until);

        if (cache_.empty()) {
            active_window_.start_idx = 0;
            reset_counters(active_window_);
            return;
        }

        if (purge_until < active_window_.start_idx)
            active_window_.start_idx -= purge_until;
        else {
            active_window_.start_idx = cache_.size() > time_window_size() ? cache_.size() - time_window_size() : 0;
            replay_time_window(active_window_, std::span{cache_.begin() + active_window_.start_idx, cache_.end()});
        }
    }

    void update_window(window_info &window, std::size_t timestamp) {
        const auto &initial_state = automaton_.states()[automaton_.initial_state_id()];

        bool removed_initiator = false;
        while (!window.per_event_counters.empty() && !in_shared_window(timestamp, timestamp_at(window.start_idx))) {
            const auto type = cache_[window.start_idx++].cached_event.type;
            removed_initiator |= initial_state.transitions.contains(type) || initial_state.transitions.contains(nfa::wildcard_symbol);
            window.per_event_counters.pop_front();
        }

        if (removed_initiator)
            replay_time_window(window);
    }

    void replay_time_window(window_info &window) const {
        replay_time_window(window, std::span{cache_.begin() + window.start_idx, window.per_event_counters.size()});
    }

    void replay_time_window(window_info &window, std::span<const cache_entry> events) const {
        reset_counters(window);

        for (std::size_t i = 0; i < events.size(); ++i) {
            const auto to_readd = events[i].cached_event.type;
            auto global_counter_change = advance(window.total_counter, per_character_edges_, to_readd);
            window.total_counter += global_counter_change;
            for (std::size_t j = 0; j < i; ++j) {
                const auto local_change = advance(window.per_event_counters[j], per_character_edges_, to_readd);
                window.per_event_counters[j] += local_change;
            }
            window.per_event_counters.push_back(std::move(global_counter_change));
        }
    }

    void replay_affected_range(std::size_t removed_idx, std::size_t removed_timestamp) {
        auto replay_start_idx = removed_idx < time_window_size() ? 0 : removed_idx - time_window_size();
        while (replay_start_idx < cache_.size() && !in_shared_window(removed_timestamp, timestamp_at(replay_start_idx)))
            ++replay_start_idx;

        const auto replay_start_timestamp = timestamp_at(replay_start_idx);

        auto time_window_replay_start_idx = replay_start_idx < time_window_size() ? 0 : replay_start_idx - time_window_size();
        while (time_window_replay_start_idx < cache_.size() && !in_shared_window(replay_start_timestamp, timestamp_at(time_window_replay_start_idx)))
            ++time_window_replay_start_idx;

        auto replay_window = create_window_info(time_window_size());
        replay_window.start_idx = time_window_replay_start_idx;
        const auto relevant_prefix = std::span{cache_.begin() + replay_window.start_idx, cache_.begin() + replay_start_idx};
        replay_time_window(replay_window, relevant_prefix);

        const auto is_relevant = [&](std::size_t idx) {
            const auto affected = in_shared_window(removed_timestamp, timestamp_at(idx));
            const auto in_affecting_window = in_shared_window(removed_timestamp, timestamp_at(replay_window.start_idx));

            return affected || in_affecting_window;
        };

        for (std::size_t idx = replay_start_idx; idx < cache_.size() && is_relevant(idx); ++idx) {
            update_window(replay_window, timestamp_at(idx));

            auto global_counter_change = advance(replay_window.total_counter, per_character_edges_, cache_[idx].cached_event.type);
            replay_window.total_counter += global_counter_change;

            const auto active_window_size = idx - replay_window.start_idx;
            for (std::size_t i = 0; i < active_window_size; ++i) {
                const auto cache_idx = replay_window.start_idx + i;

                const auto local_change = advance(replay_window.per_event_counters[i], per_character_edges_, cache_[idx].cached_event.type);
                if (cache_idx >= replay_start_idx && in_shared_window(removed_timestamp, timestamp_at(cache_idx)))
                    cache_[cache_idx].state_counter += local_change;
                replay_window.per_event_counters[i] += local_change;
            }

            replay_window.per_event_counters.push_back(global_counter_change);
            if (in_shared_window(removed_timestamp, timestamp_at(idx)))
                cache_[idx].state_counter = std::move(global_counter_change);
        }
    }

    auto create_window_info(std::size_t window_size) const {
        window_info wnd{
            execution_state_counter<counter_type>{automaton_.number_of_states()},
            ring_buffer<execution_state_counter<counter_type>>{window_size, execution_state_counter<counter_type>{automaton_.number_of_states()}},
            0};

        reset_counters(wnd);
        return wnd;
    }

    void reset_counters(window_info &window) const {
        window.total_counter *= 0; // performance!
        window.total_counter[automaton_.initial_state_id()] = 1;
        window.per_event_counters.clear();
    }

    std::size_t timestamp_at(std::size_t cache_idx) const {
        assert(cache_idx < cache_.size());

        return cache_[cache_idx].cached_event.timestamp;
    }

    bool in_shared_window(std::size_t timestamp0, std::size_t timestamp1) const {
        const auto time_window_size = active_window_.per_event_counters.capacity();

        if (timestamp1 > timestamp0)
            std::swap(timestamp0, timestamp1);

        return timestamp0 - timestamp1 <= time_window_size;
    }
};
} // namespace suse

#endif